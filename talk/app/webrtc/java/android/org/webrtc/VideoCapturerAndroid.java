/*
 * libjingle
 * Copyright 2015 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package org.webrtc;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.view.Surface;
import android.view.WindowManager;

import org.json.JSONException;
import org.webrtc.CameraEnumerationAndroid.CaptureFormat;
import org.webrtc.Logging;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

// Android specific implementation of VideoCapturer.
// An instance of this class can be created by an application using
// VideoCapturerAndroid.create();
// This class extends VideoCapturer with a method to easily switch between the
// front and back camera. It also provides methods for enumerating valid device
// names.
//
// Threading notes: this class is called from C++ code, Android Camera callbacks, and possibly
// arbitrary Java threads. All public entry points are thread safe, and delegate the work to the
// camera thread. The internal *OnCameraThread() methods must check |camera| for null to check if
// the camera has been stopped.
@SuppressWarnings("deprecation")
public class VideoCapturerAndroid extends VideoCapturer implements
    android.hardware.Camera.PreviewCallback,
    SurfaceTextureHelper.OnTextureFrameAvailableListener {
  private final static String TAG = "VideoCapturerAndroid";
  private final static int CAMERA_OBSERVER_PERIOD_MS = 2000;
  private final static int CAMERA_FREEZE_REPORT_TIMOUT_MS = 6000;

  private android.hardware.Camera camera;  // Only non-null while capturing.
  private HandlerThread cameraThread;
  private final Handler cameraThreadHandler;
  private Context applicationContext;
  // Synchronization lock for |id|.
  private final Object cameraIdLock = new Object();
  private int id;
  private android.hardware.Camera.CameraInfo info;
  private final CameraStatistics cameraStatistics;
  // Remember the requested format in case we want to switch cameras.
  private int requestedWidth;
  private int requestedHeight;
  private int requestedFramerate;
  // The capture format will be the closest supported format to the requested format.
  private CaptureFormat captureFormat;
  private final Object pendingCameraSwitchLock = new Object();
  private volatile boolean pendingCameraSwitch;
  private CapturerObserver frameObserver = null;
  private final CameraEventsHandler eventsHandler;
  private boolean firstFrameReported;
  // Arbitrary queue depth.  Higher number means more memory allocated & held,
  // lower number means more sensitivity to processing time in the client (and
  // potentially stalling the capturer if it runs out of buffers to write to).
  private static final int NUMBER_OF_CAPTURE_BUFFERS = 3;
  private final Set<byte[]> queuedBuffers = new HashSet<byte[]>();
  private final boolean isCapturingToTexture;
  final SurfaceTextureHelper surfaceHelper; // Package visible for testing purposes.
  // The camera API can output one old frame after the camera has been switched or the resolution
  // has been changed. This flag is used for dropping the first frame after camera restart.
  private boolean dropNextFrame = false;
  // |openCameraOnCodecThreadRunner| is used for retrying to open the camera if it is in use by
  // another application when startCaptureOnCameraThread is called.
  private Runnable openCameraOnCodecThreadRunner;
  private final static int MAX_OPEN_CAMERA_ATTEMPTS = 3;
  private final static int OPEN_CAMERA_DELAY_MS = 500;
  private int openCameraAttempts;

  // Camera error callback.
  private final android.hardware.Camera.ErrorCallback cameraErrorCallback =
      new android.hardware.Camera.ErrorCallback() {
    @Override
    public void onError(int error, android.hardware.Camera camera) {
      String errorMessage;
      if (error == android.hardware.Camera.CAMERA_ERROR_SERVER_DIED) {
        errorMessage = "Camera server died!";
      } else {
        errorMessage = "Camera error: " + error;
      }
      Logging.e(TAG, errorMessage);
      if (eventsHandler != null) {
        eventsHandler.onCameraError(errorMessage);
      }
    }
  };

  // Camera observer - monitors camera framerate. Observer is executed on camera thread.
  private final Runnable cameraObserver = new Runnable() {
    private int freezePeriodCount;
    @Override
    public void run() {
      int cameraFramesCount = cameraStatistics.getAndResetFrameCount();
      int cameraFps = (cameraFramesCount * 1000 + CAMERA_OBSERVER_PERIOD_MS / 2)
          / CAMERA_OBSERVER_PERIOD_MS;

      Logging.d(TAG, "Camera fps: " + cameraFps +".");
      if (cameraFramesCount == 0) {
        ++freezePeriodCount;
        if (CAMERA_OBSERVER_PERIOD_MS * freezePeriodCount > CAMERA_FREEZE_REPORT_TIMOUT_MS
            && eventsHandler != null) {
          Logging.e(TAG, "Camera freezed.");
          if (surfaceHelper.isTextureInUse()) {
            // This can only happen if we are capturing to textures.
            eventsHandler.onCameraFreezed("Camera failure. Client must return video buffers.");
          } else {
            eventsHandler.onCameraFreezed("Camera failure.");
          }
          return;
        }
      } else {
        freezePeriodCount = 0;
      }
      cameraThreadHandler.postDelayed(this, CAMERA_OBSERVER_PERIOD_MS);
    }
  };

  private static class CameraStatistics {
    private int frameCount = 0;
    private final ThreadUtils.ThreadChecker threadChecker = new ThreadUtils.ThreadChecker();

    CameraStatistics() {
      threadChecker.detachThread();
    }

    public void addFrame() {
      threadChecker.checkIsOnValidThread();
      ++frameCount;
    }

    public int getAndResetFrameCount() {
      threadChecker.checkIsOnValidThread();
      int count = frameCount;
      frameCount = 0;
      return count;
    }
  }

  public static interface CameraEventsHandler {
    // Camera error handler - invoked when camera can not be opened
    // or any camera exception happens on camera thread.
    void onCameraError(String errorDescription);

    // Invoked when camera stops receiving frames
    void onCameraFreezed(String errorDescription);

    // Callback invoked when camera is opening.
    void onCameraOpening(int cameraId);

    // Callback invoked when first camera frame is available after camera is opened.
    void onFirstFrameAvailable();

    // Callback invoked when camera closed.
    void onCameraClosed();
  }

  // Camera switch handler - one of these functions are invoked with the result of switchCamera().
  // The callback may be called on an arbitrary thread.
  public interface CameraSwitchHandler {
    // Invoked on success. |isFrontCamera| is true if the new camera is front facing.
    void onCameraSwitchDone(boolean isFrontCamera);
    // Invoked on failure, e.g. camera is stopped or only one camera available.
    void onCameraSwitchError(String errorDescription);
  }

  public static VideoCapturerAndroid create(String name,
      CameraEventsHandler eventsHandler) {
    return VideoCapturerAndroid.create(name, eventsHandler, null);
  }

  public static VideoCapturerAndroid create(String name,
      CameraEventsHandler eventsHandler, EglBase.Context sharedEglContext) {
    final int cameraId = lookupDeviceName(name);
    if (cameraId == -1) {
      return null;
    }

    final VideoCapturerAndroid capturer = new VideoCapturerAndroid(cameraId, eventsHandler,
        sharedEglContext);
    capturer.setNativeCapturer(
        nativeCreateVideoCapturer(capturer, capturer.surfaceHelper));
    return capturer;
  }

  public void printStackTrace() {
    if (cameraThread != null) {
      StackTraceElement[] cameraStackTraces = cameraThread.getStackTrace();
      if (cameraStackTraces.length > 0) {
        Logging.d(TAG, "VideoCapturerAndroid stacks trace:");
        for (StackTraceElement stackTrace : cameraStackTraces) {
          Logging.d(TAG, stackTrace.toString());
        }
      }
    }
  }

  // Switch camera to the next valid camera id. This can only be called while
  // the camera is running.
  public void switchCamera(final CameraSwitchHandler handler) {
    if (android.hardware.Camera.getNumberOfCameras() < 2) {
      if (handler != null) {
        handler.onCameraSwitchError("No camera to switch to.");
      }
      return;
    }
    synchronized (pendingCameraSwitchLock) {
      if (pendingCameraSwitch) {
        // Do not handle multiple camera switch request to avoid blocking
        // camera thread by handling too many switch request from a queue.
        Logging.w(TAG, "Ignoring camera switch request.");
        if (handler != null) {
          handler.onCameraSwitchError("Pending camera switch already in progress.");
        }
        return;
      }
      pendingCameraSwitch = true;
    }
    cameraThreadHandler.post(new Runnable() {
      @Override public void run() {
        if (camera == null) {
          if (handler != null) {
            handler.onCameraSwitchError("Camera is stopped.");
          }
          return;
        }
        switchCameraOnCameraThread();
        synchronized (pendingCameraSwitchLock) {
          pendingCameraSwitch = false;
        }
        if (handler != null) {
          handler.onCameraSwitchDone(
              info.facing == android.hardware.Camera.CameraInfo.CAMERA_FACING_FRONT);
        }
      }
    });
  }

  // Requests a new output format from the video capturer. Captured frames
  // by the camera will be scaled/or dropped by the video capturer.
  // It does not matter if width and height are flipped. I.E, |width| = 640, |height| = 480 produce
  // the same result as |width| = 480, |height| = 640.
  // TODO(magjed/perkj): Document what this function does. Change name?
  public void onOutputFormatRequest(final int width, final int height, final int framerate) {
    cameraThreadHandler.post(new Runnable() {
      @Override public void run() {
        onOutputFormatRequestOnCameraThread(width, height, framerate);
      }
    });
  }

  // Reconfigure the camera to capture in a new format. This should only be called while the camera
  // is running.
  public void changeCaptureFormat(final int width, final int height, final int framerate) {
    cameraThreadHandler.post(new Runnable() {
      @Override public void run() {
        startPreviewOnCameraThread(width, height, framerate);
      }
    });
  }

  // Helper function to retrieve the current camera id synchronously. Note that the camera id might
  // change at any point by switchCamera() calls.
  int getCurrentCameraId() {
    synchronized (cameraIdLock) {
      return id;
    }
  }

  public List<CaptureFormat> getSupportedFormats() {
    return CameraEnumerationAndroid.getSupportedFormats(getCurrentCameraId());
  }

  // Returns true if this VideoCapturer is setup to capture video frames to a SurfaceTexture.
  public boolean isCapturingToTexture() {
    return isCapturingToTexture;
  }

  // Called from native code.
  private String getSupportedFormatsAsJson() throws JSONException {
    return CameraEnumerationAndroid.getSupportedFormatsAsJson(getCurrentCameraId());
  }

  // Called from native VideoCapturer_nativeCreateVideoCapturer.
  private VideoCapturerAndroid(int cameraId) {
    this(cameraId, null, null);
  }

  private VideoCapturerAndroid(int cameraId, CameraEventsHandler eventsHandler,
      EglBase.Context sharedContext) {
    this.id = cameraId;
    this.eventsHandler = eventsHandler;
    cameraThread = new HandlerThread(TAG);
    cameraThread.start();
    cameraThreadHandler = new Handler(cameraThread.getLooper());
    isCapturingToTexture = (sharedContext != null);
    cameraStatistics = new CameraStatistics();
    surfaceHelper = SurfaceTextureHelper.create(sharedContext, cameraThreadHandler);
    if (isCapturingToTexture) {
      surfaceHelper.setListener(this);
    }
    Logging.d(TAG, "VideoCapturerAndroid isCapturingToTexture : " + isCapturingToTexture);
  }

  private void checkIsOnCameraThread() {
    if (Thread.currentThread() != cameraThread) {
      throw new IllegalStateException("Wrong thread");
    }
  }

  // Returns the camera index for camera with name |deviceName|, or -1 if no such camera can be
  // found. If |deviceName| is empty, the first available device is used.
  private static int lookupDeviceName(String deviceName) {
    Logging.d(TAG, "lookupDeviceName: " + deviceName);
    if (deviceName == null || android.hardware.Camera.getNumberOfCameras() == 0) {
      return -1;
    }
    if (deviceName.isEmpty()) {
      return 0;
    }
    for (int i = 0; i < android.hardware.Camera.getNumberOfCameras(); ++i) {
      if (deviceName.equals(CameraEnumerationAndroid.getDeviceName(i))) {
        return i;
      }
    }
    return -1;
  }

  // Called by native code to quit the camera thread. This needs to be done manually, otherwise the
  // thread and handler will not be garbage collected.
  private void release() {
    Logging.d(TAG, "release");
    if (isReleased()) {
      throw new IllegalStateException("Already released");
    }
    ThreadUtils.invokeUninterruptibly(cameraThreadHandler, new Runnable() {
      @Override
      public void run() {
        if (camera != null) {
          throw new IllegalStateException("Release called while camera is running");
        }
      }
    });
    surfaceHelper.disconnect(cameraThreadHandler);
    cameraThread = null;
  }

  // Used for testing purposes to check if release() has been called.
  public boolean isReleased() {
    return (cameraThread == null);
  }

  // Called by native code.
  //
  // Note that this actually opens the camera, and Camera callbacks run on the
  // thread that calls open(), so this is done on the CameraThread.
  void startCapture(
      final int width, final int height, final int framerate,
      final Context applicationContext, final CapturerObserver frameObserver) {
    Logging.d(TAG, "startCapture requested: " + width + "x" + height
        + "@" + framerate);
    if (applicationContext == null) {
      throw new RuntimeException("applicationContext not set.");
    }
    if (frameObserver == null) {
      throw new RuntimeException("frameObserver not set.");
    }

    cameraThreadHandler.post(new Runnable() {
      @Override public void run() {
        startCaptureOnCameraThread(width, height, framerate, frameObserver,
            applicationContext);
      }
    });
  }

  private void startCaptureOnCameraThread(
      final int width, final int height, final int framerate, final CapturerObserver frameObserver,
      final Context applicationContext) {
    Throwable error = null;
    checkIsOnCameraThread();
    if (camera != null) {
      throw new RuntimeException("Camera has already been started.");
    }
    this.applicationContext = applicationContext;
    this.frameObserver = frameObserver;
    this.firstFrameReported = false;

    try {
      try {
        synchronized (cameraIdLock) {
          Logging.d(TAG, "Opening camera " + id);
          if (eventsHandler != null) {
            eventsHandler.onCameraOpening(id);
          }
          camera = android.hardware.Camera.open(id);
          info = new android.hardware.Camera.CameraInfo();
          android.hardware.Camera.getCameraInfo(id, info);
        }
      } catch (RuntimeException e) {
        openCameraAttempts++;
        if (openCameraAttempts < MAX_OPEN_CAMERA_ATTEMPTS) {
          Logging.e(TAG, "Camera.open failed, retrying", e);
          openCameraOnCodecThreadRunner = new Runnable() {
            @Override public void run() {
              startCaptureOnCameraThread(width, height, framerate, frameObserver,
                  applicationContext);
            }
          };
          cameraThreadHandler.postDelayed(openCameraOnCodecThreadRunner, OPEN_CAMERA_DELAY_MS);
          return;
        }
        openCameraAttempts = 0;
        throw e;
      }

      try {
        camera.setPreviewTexture(surfaceHelper.getSurfaceTexture());
      } catch (IOException e) {
        Logging.e(TAG, "setPreviewTexture failed", error);
        throw new RuntimeException(e);
      }

      Logging.d(TAG, "Camera orientation: " + info.orientation +
          " .Device orientation: " + getDeviceOrientation());
      camera.setErrorCallback(cameraErrorCallback);
      startPreviewOnCameraThread(width, height, framerate);
      frameObserver.onCapturerStarted(true);

      // Start camera observer.
      cameraThreadHandler.postDelayed(cameraObserver, CAMERA_OBSERVER_PERIOD_MS);
      return;
    } catch (RuntimeException e) {
      error = e;
    }
    Logging.e(TAG, "startCapture failed", error);
    stopCaptureOnCameraThread();
    frameObserver.onCapturerStarted(false);
    if (eventsHandler != null) {
      eventsHandler.onCameraError("Camera can not be started.");
    }
    return;
  }

  // (Re)start preview with the closest supported format to |width| x |height| @ |framerate|.
  private void startPreviewOnCameraThread(int width, int height, int framerate) {
    checkIsOnCameraThread();
    Logging.d(
        TAG, "startPreviewOnCameraThread requested: " + width + "x" + height + "@" + framerate);
    if (camera == null) {
      Logging.e(TAG, "Calling startPreviewOnCameraThread on stopped camera.");
      return;
    }

    requestedWidth = width;
    requestedHeight = height;
    requestedFramerate = framerate;

    // Find closest supported format for |width| x |height| @ |framerate|.
    final android.hardware.Camera.Parameters parameters = camera.getParameters();
    final int[] range = CameraEnumerationAndroid.getFramerateRange(parameters, framerate * 1000);
    final android.hardware.Camera.Size previewSize =
        CameraEnumerationAndroid.getClosestSupportedSize(
            parameters.getSupportedPreviewSizes(), width, height);
    final CaptureFormat captureFormat = new CaptureFormat(
        previewSize.width, previewSize.height,
        range[android.hardware.Camera.Parameters.PREVIEW_FPS_MIN_INDEX],
        range[android.hardware.Camera.Parameters.PREVIEW_FPS_MAX_INDEX]);

    // Check if we are already using this capture format, then we don't need to do anything.
    if (captureFormat.isSameFormat(this.captureFormat)) {
      return;
    }

    // Update camera parameters.
    Logging.d(TAG, "isVideoStabilizationSupported: " +
        parameters.isVideoStabilizationSupported());
    if (parameters.isVideoStabilizationSupported()) {
      parameters.setVideoStabilization(true);
    }
    // Note: setRecordingHint(true) actually decrease frame rate on N5.
    // parameters.setRecordingHint(true);
    if (captureFormat.maxFramerate > 0) {
      parameters.setPreviewFpsRange(captureFormat.minFramerate, captureFormat.maxFramerate);
    }
    parameters.setPreviewSize(captureFormat.width, captureFormat.height);

    if (!isCapturingToTexture) {
      parameters.setPreviewFormat(captureFormat.imageFormat);
    }
    // Picture size is for taking pictures and not for preview/video, but we need to set it anyway
    // as a workaround for an aspect ratio problem on Nexus 7.
    final android.hardware.Camera.Size pictureSize =
        CameraEnumerationAndroid.getClosestSupportedSize(
            parameters.getSupportedPictureSizes(), width, height);
    parameters.setPictureSize(pictureSize.width, pictureSize.height);

    // Temporarily stop preview if it's already running.
    if (this.captureFormat != null) {
      camera.stopPreview();
      dropNextFrame = true;
      // Calling |setPreviewCallbackWithBuffer| with null should clear the internal camera buffer
      // queue, but sometimes we receive a frame with the old resolution after this call anyway.
      camera.setPreviewCallbackWithBuffer(null);
    }

    // (Re)start preview.
    Logging.d(TAG, "Start capturing: " + captureFormat);
    this.captureFormat = captureFormat;

    List<String> focusModes = parameters.getSupportedFocusModes();
    if (focusModes.contains(android.hardware.Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO)) {
      parameters.setFocusMode(android.hardware.Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
    }

    camera.setParameters(parameters);
    if (!isCapturingToTexture) {
      queuedBuffers.clear();
      final int frameSize = captureFormat.frameSize();
      for (int i = 0; i < NUMBER_OF_CAPTURE_BUFFERS; ++i) {
        final ByteBuffer buffer = ByteBuffer.allocateDirect(frameSize);
        queuedBuffers.add(buffer.array());
        camera.addCallbackBuffer(buffer.array());
      }
      camera.setPreviewCallbackWithBuffer(this);
    }
    camera.startPreview();
  }

  // Called by native code.  Returns true when camera is known to be stopped.
  void stopCapture() throws InterruptedException {
    Logging.d(TAG, "stopCapture");
    final CountDownLatch barrier = new CountDownLatch(1);
    cameraThreadHandler.post(new Runnable() {
        @Override public void run() {
          stopCaptureOnCameraThread();
          barrier.countDown();
        }
    });
    barrier.await();
    Logging.d(TAG, "stopCapture done");
  }

  private void stopCaptureOnCameraThread() {
    checkIsOnCameraThread();
    Logging.d(TAG, "stopCaptureOnCameraThread");
    if (openCameraOnCodecThreadRunner != null) {
      cameraThreadHandler.removeCallbacks(openCameraOnCodecThreadRunner);
    }
    openCameraAttempts = 0;
    if (camera == null) {
      Logging.e(TAG, "Calling stopCapture() for already stopped camera.");
      return;
    }

    cameraThreadHandler.removeCallbacks(cameraObserver);
    cameraStatistics.getAndResetFrameCount();
    Logging.d(TAG, "Stop preview.");
    camera.stopPreview();
    camera.setPreviewCallbackWithBuffer(null);
    queuedBuffers.clear();
    captureFormat = null;

    Logging.d(TAG, "Release camera.");
    camera.release();
    camera = null;
    if (eventsHandler != null) {
      eventsHandler.onCameraClosed();
    }
  }

  private void switchCameraOnCameraThread() {
    checkIsOnCameraThread();
    Logging.d(TAG, "switchCameraOnCameraThread");
    stopCaptureOnCameraThread();
    synchronized (cameraIdLock) {
      id = (id + 1) % android.hardware.Camera.getNumberOfCameras();
    }
    dropNextFrame = true;
    startCaptureOnCameraThread(requestedWidth, requestedHeight, requestedFramerate, frameObserver,
        applicationContext);
    Logging.d(TAG, "switchCameraOnCameraThread done");
  }

  private void onOutputFormatRequestOnCameraThread(int width, int height, int framerate) {
    checkIsOnCameraThread();
    if (camera == null) {
      Logging.e(TAG, "Calling onOutputFormatRequest() on stopped camera.");
      return;
    }
    Logging.d(TAG, "onOutputFormatRequestOnCameraThread: " + width + "x" + height +
        "@" + framerate);
    frameObserver.onOutputFormatRequest(width, height, framerate);
  }

  // Exposed for testing purposes only.
  Handler getCameraThreadHandler() {
    return cameraThreadHandler;
  }

  private int getDeviceOrientation() {
    int orientation = 0;

    WindowManager wm = (WindowManager) applicationContext.getSystemService(
        Context.WINDOW_SERVICE);
    switch(wm.getDefaultDisplay().getRotation()) {
      case Surface.ROTATION_90:
        orientation = 90;
        break;
      case Surface.ROTATION_180:
        orientation = 180;
        break;
      case Surface.ROTATION_270:
        orientation = 270;
        break;
      case Surface.ROTATION_0:
      default:
        orientation = 0;
        break;
    }
    return orientation;
  }

  private int getFrameOrientation() {
    int rotation = getDeviceOrientation();
    if (info.facing == android.hardware.Camera.CameraInfo.CAMERA_FACING_BACK) {
      rotation = 360 - rotation;
    }
    return (info.orientation + rotation) % 360;
  }

  // Called on cameraThread so must not "synchronized".
  @Override
  public void onPreviewFrame(byte[] data, android.hardware.Camera callbackCamera) {
    checkIsOnCameraThread();
    if (camera == null || !queuedBuffers.contains(data)) {
      // The camera has been stopped or |data| is an old invalid buffer.
      return;
    }
    if (camera != callbackCamera) {
      throw new RuntimeException("Unexpected camera in callback!");
    }

    final long captureTimeNs =
        TimeUnit.MILLISECONDS.toNanos(SystemClock.elapsedRealtime());

    if (eventsHandler != null && !firstFrameReported) {
      eventsHandler.onFirstFrameAvailable();
      firstFrameReported = true;
    }

    cameraStatistics.addFrame();
    frameObserver.onByteBufferFrameCaptured(data, captureFormat.width, captureFormat.height,
        getFrameOrientation(), captureTimeNs);
    camera.addCallbackBuffer(data);
  }

  @Override
  public void onTextureFrameAvailable(
      int oesTextureId, float[] transformMatrix, long timestampNs) {
    checkIsOnCameraThread();
    if (camera == null) {
      // Camera is stopped, we need to return the buffer immediately.
      surfaceHelper.returnTextureFrame();
      return;
    }
    if (dropNextFrame)  {
     surfaceHelper.returnTextureFrame();
     dropNextFrame = false;
     return;
    }

    int rotation = getFrameOrientation();
    if (info.facing == android.hardware.Camera.CameraInfo.CAMERA_FACING_FRONT) {
      // Undo the mirror that the OS "helps" us with.
      // http://developer.android.com/reference/android/hardware/Camera.html#setDisplayOrientation(int)
      transformMatrix =
          RendererCommon.multiplyMatrices(transformMatrix, RendererCommon.horizontalFlipMatrix());
    }
    cameraStatistics.addFrame();
    frameObserver.onTextureFrameCaptured(captureFormat.width, captureFormat.height, oesTextureId,
        transformMatrix, rotation, timestampNs);
  }

  // Interface used for providing callbacks to an observer.
  interface CapturerObserver {
    // Notify if the camera have been started successfully or not.
    // Called on a Java thread owned by VideoCapturerAndroid.
    abstract void onCapturerStarted(boolean success);

    // Delivers a captured frame. Called on a Java thread owned by
    // VideoCapturerAndroid.
    abstract void onByteBufferFrameCaptured(byte[] data, int width, int height, int rotation,
        long timeStamp);

    // Delivers a captured frame in a texture with id |oesTextureId|. Called on a Java thread
    // owned by VideoCapturerAndroid.
    abstract void onTextureFrameCaptured(
        int width, int height, int oesTextureId, float[] transformMatrix, int rotation,
        long timestamp);

    // Requests an output format from the video capturer. Captured frames
    // by the camera will be scaled/or dropped by the video capturer.
    // Called on a Java thread owned by VideoCapturerAndroid.
    abstract void onOutputFormatRequest(int width, int height, int framerate);
  }

  // An implementation of CapturerObserver that forwards all calls from
  // Java to the C layer.
  static class NativeObserver implements CapturerObserver {
    private final long nativeCapturer;

    public NativeObserver(long nativeCapturer) {
      this.nativeCapturer = nativeCapturer;
    }

    @Override
    public void onCapturerStarted(boolean success) {
      nativeCapturerStarted(nativeCapturer, success);
    }

    @Override
    public void onByteBufferFrameCaptured(byte[] data, int width, int height,
        int rotation, long timeStamp) {
      nativeOnByteBufferFrameCaptured(nativeCapturer, data, data.length, width, height, rotation,
          timeStamp);
    }

    @Override
    public void onTextureFrameCaptured(
        int width, int height, int oesTextureId, float[] transformMatrix, int rotation,
        long timestamp) {
      nativeOnTextureFrameCaptured(nativeCapturer, width, height, oesTextureId, transformMatrix,
          rotation, timestamp);
    }

    @Override
    public void onOutputFormatRequest(int width, int height, int framerate) {
      nativeOnOutputFormatRequest(nativeCapturer, width, height, framerate);
    }

    private native void nativeCapturerStarted(long nativeCapturer,
        boolean success);
    private native void nativeOnByteBufferFrameCaptured(long nativeCapturer,
        byte[] data, int length, int width, int height, int rotation, long timeStamp);
    private native void nativeOnTextureFrameCaptured(long nativeCapturer, int width, int height,
        int oesTextureId, float[] transformMatrix, int rotation, long timestamp);
    private native void nativeOnOutputFormatRequest(long nativeCapturer,
        int width, int height, int framerate);
  }

  private static native long nativeCreateVideoCapturer(
      VideoCapturerAndroid videoCapturer,
      SurfaceTextureHelper surfaceHelper);
}
