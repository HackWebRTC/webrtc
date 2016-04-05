/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.content.Context;
import android.os.Handler;
import android.os.SystemClock;
import android.view.Surface;
import android.view.WindowManager;

import org.webrtc.CameraEnumerationAndroid.CaptureFormat;
import org.webrtc.Logging;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.HashSet;
import java.util.List;
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
public class VideoCapturerAndroid implements
    VideoCapturer,
    android.hardware.Camera.PreviewCallback,
    SurfaceTextureHelper.OnTextureFrameAvailableListener {
  private final static String TAG = "VideoCapturerAndroid";
  private final static int CAMERA_OBSERVER_PERIOD_MS = 2000;
  private final static int CAMERA_FREEZE_REPORT_TIMOUT_MS = 4000;
  private static final int CAMERA_STOP_TIMEOUT_MS = 7000;

  private boolean isDisposed = false;
  private android.hardware.Camera camera;  // Only non-null while capturing.
  private final Object handlerLock = new Object();
  // |cameraThreadHandler| must be synchronized on |handlerLock| when not on the camera thread,
  // or when modifying the reference. Use maybePostOnCameraThread() instead of posting directly to
  // the handler - this way all callbacks with a specifed token can be removed at once.
  private Handler cameraThreadHandler;
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
  private SurfaceTextureHelper surfaceHelper;
  // The camera API can output one old frame after the camera has been switched or the resolution
  // has been changed. This flag is used for dropping the first frame after camera restart.
  private boolean dropNextFrame = false;
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
        if (CAMERA_OBSERVER_PERIOD_MS * freezePeriodCount >= CAMERA_FREEZE_REPORT_TIMOUT_MS
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
      maybePostDelayedOnCameraThread(CAMERA_OBSERVER_PERIOD_MS, this);
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
    return VideoCapturerAndroid.create(name, eventsHandler, false /* captureToTexture */);
  }

  // Deprecated. Use create() function below instead.
  public static VideoCapturerAndroid create(String name,
      CameraEventsHandler eventsHandler, EglBase.Context sharedEglContext) {
    return create(name, eventsHandler, (sharedEglContext != null) /* captureToTexture */);
  }

  public static VideoCapturerAndroid create(String name,
      CameraEventsHandler eventsHandler, boolean captureToTexture) {
    final int cameraId = lookupDeviceName(name);
    if (cameraId == -1) {
      return null;
    }
    return new VideoCapturerAndroid(cameraId, eventsHandler, captureToTexture);
  }

  public void printStackTrace() {
    Thread cameraThread = null;
    synchronized (handlerLock) {
      if (cameraThreadHandler != null) {
        cameraThread = cameraThreadHandler.getLooper().getThread();
      }
    }
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
  public void switchCamera(final CameraSwitchHandler switchEventsHandler) {
    if (android.hardware.Camera.getNumberOfCameras() < 2) {
      if (switchEventsHandler != null) {
        switchEventsHandler.onCameraSwitchError("No camera to switch to.");
      }
      return;
    }
    synchronized (pendingCameraSwitchLock) {
      if (pendingCameraSwitch) {
        // Do not handle multiple camera switch request to avoid blocking
        // camera thread by handling too many switch request from a queue.
        Logging.w(TAG, "Ignoring camera switch request.");
        if (switchEventsHandler != null) {
          switchEventsHandler.onCameraSwitchError("Pending camera switch already in progress.");
        }
        return;
      }
      pendingCameraSwitch = true;
    }
    final boolean didPost = maybePostOnCameraThread(new Runnable() {
      @Override
      public void run() {
        switchCameraOnCameraThread();
        synchronized (pendingCameraSwitchLock) {
          pendingCameraSwitch = false;
        }
        if (switchEventsHandler != null) {
          switchEventsHandler.onCameraSwitchDone(
              info.facing == android.hardware.Camera.CameraInfo.CAMERA_FACING_FRONT);
        }
      }
    });
    if (!didPost && switchEventsHandler != null) {
      switchEventsHandler.onCameraSwitchError("Camera is stopped.");
    }
  }

  // Requests a new output format from the video capturer. Captured frames
  // by the camera will be scaled/or dropped by the video capturer.
  // It does not matter if width and height are flipped. I.E, |width| = 640, |height| = 480 produce
  // the same result as |width| = 480, |height| = 640.
  // TODO(magjed/perkj): Document what this function does. Change name?
  public void onOutputFormatRequest(final int width, final int height, final int framerate) {
    maybePostOnCameraThread(new Runnable() {
      @Override public void run() {
        onOutputFormatRequestOnCameraThread(width, height, framerate);
      }
    });
  }

  // Reconfigure the camera to capture in a new format. This should only be called while the camera
  // is running.
  public void changeCaptureFormat(final int width, final int height, final int framerate) {
    maybePostOnCameraThread(new Runnable() {
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

  @Override
  public List<CaptureFormat> getSupportedFormats() {
    return CameraEnumerationAndroid.getSupportedFormats(getCurrentCameraId());
  }

  // Returns true if this VideoCapturer is setup to capture video frames to a SurfaceTexture.
  public boolean isCapturingToTexture() {
    return isCapturingToTexture;
  }

  private VideoCapturerAndroid(int cameraId, CameraEventsHandler eventsHandler,
      boolean captureToTexture) {
    this.id = cameraId;
    this.eventsHandler = eventsHandler;
    isCapturingToTexture = captureToTexture;
    cameraStatistics = new CameraStatistics();
    Logging.d(TAG, "VideoCapturerAndroid isCapturingToTexture : " + isCapturingToTexture);
  }

  private void checkIsOnCameraThread() {
    if (Thread.currentThread() != cameraThreadHandler.getLooper().getThread()) {
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

  private boolean maybePostOnCameraThread(Runnable runnable) {
    return maybePostDelayedOnCameraThread(0 /* delayMs */, runnable);
  }

  private boolean maybePostDelayedOnCameraThread(int delayMs, Runnable runnable) {
    synchronized (handlerLock) {
      return cameraThreadHandler != null
          && cameraThreadHandler.postAtTime(
              runnable, this /* token */, SystemClock.uptimeMillis() + delayMs);
    }
  }

  // Dispose the SurfaceTextureHelper. This needs to be done manually, otherwise the
  // SurfaceTextureHelper thread and resources will not be garbage collected.
  @Override
  public void dispose() {
    Logging.d(TAG, "release");
    if (isDisposed()) {
      throw new IllegalStateException("Already released");
    }
    synchronized (handlerLock) {
      if (cameraThreadHandler != null) {
        throw new IllegalStateException("dispose() called while camera is running");
      }
    }
    isDisposed = true;
  }

  // Used for testing purposes to check if dispose() has been called.
  public boolean isDisposed() {
    return isDisposed;
  }

  // Note that this actually opens the camera, and Camera callbacks run on the
  // thread that calls open(), so this is done on the CameraThread.
  @Override
  public void startCapture(
      final int width, final int height, final int framerate,
      final SurfaceTextureHelper surfaceTextureHelper, final Context applicationContext,
      final CapturerObserver frameObserver) {
    Logging.d(TAG, "startCapture requested: " + width + "x" + height + "@" + framerate);
    if (surfaceTextureHelper == null) {
      throw new IllegalArgumentException("surfaceTextureHelper not set.");
    }
    if (applicationContext == null) {
      throw new IllegalArgumentException("applicationContext not set.");
    }
    if (frameObserver == null) {
      throw new IllegalArgumentException("frameObserver not set.");
    }
    synchronized (handlerLock) {
      if (this.cameraThreadHandler != null) {
        throw new RuntimeException("Camera has already been started.");
      }
      this.cameraThreadHandler = surfaceTextureHelper.getHandler();
      this.surfaceHelper = surfaceTextureHelper;
      final boolean didPost = maybePostOnCameraThread(new Runnable() {
        @Override
        public void run() {
          openCameraAttempts = 0;
          startCaptureOnCameraThread(width, height, framerate, frameObserver,
              applicationContext);
        }
      });
      if (!didPost) {
        frameObserver.onCapturerStarted(false);
        if (eventsHandler != null) {
          eventsHandler.onCameraError("Could not post task to camera thread.");
        }
      }
    }
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
          maybePostDelayedOnCameraThread(OPEN_CAMERA_DELAY_MS, new Runnable() {
            @Override public void run() {
              startCaptureOnCameraThread(width, height, framerate, frameObserver,
                  applicationContext);
            }
          });
          return;
        }
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
      if (isCapturingToTexture) {
        surfaceHelper.startListening(this);
      }

      // Start camera observer.
      maybePostDelayedOnCameraThread(CAMERA_OBSERVER_PERIOD_MS, cameraObserver);
      return;
    } catch (RuntimeException e) {
      error = e;
    }
    Logging.e(TAG, "startCapture failed", error);
    // Make sure the camera is released.
    stopCaptureOnCameraThread();
    synchronized (handlerLock) {
      // Remove all pending Runnables posted from |this|.
      cameraThreadHandler.removeCallbacksAndMessages(this /* token */);
      cameraThreadHandler = null;
    }
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
    for (int[] fpsRange : parameters.getSupportedPreviewFpsRange()) {
      Logging.d(TAG, "Available fps range: " +
          fpsRange[android.hardware.Camera.Parameters.PREVIEW_FPS_MIN_INDEX] + ":" +
          fpsRange[android.hardware.Camera.Parameters.PREVIEW_FPS_MAX_INDEX]);
    }
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

  // Blocks until camera is known to be stopped.
  @Override
  public void stopCapture() throws InterruptedException {
    Logging.d(TAG, "stopCapture");
    final CountDownLatch barrier = new CountDownLatch(1);
    final boolean didPost = maybePostOnCameraThread(new Runnable() {
      @Override public void run() {
        stopCaptureOnCameraThread();
        synchronized (handlerLock) {
          // Remove all pending Runnables posted from |this|.
          cameraThreadHandler.removeCallbacksAndMessages(this /* token */);
          cameraThreadHandler = null;
          surfaceHelper = null;
        }
        barrier.countDown();
      }
    });
    if (!didPost) {
      Logging.e(TAG, "Calling stopCapture() for already stopped camera.");
      return;
    }
    if (!barrier.await(CAMERA_STOP_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
      Logging.e(TAG, "Camera stop timeout");
      printStackTrace();
      if (eventsHandler != null) {
        eventsHandler.onCameraError("Camera stop timeout");
      }
    }
    Logging.d(TAG, "stopCapture done");
  }

  private void stopCaptureOnCameraThread() {
    checkIsOnCameraThread();
    Logging.d(TAG, "stopCaptureOnCameraThread");
    // Note that the camera might still not be started here if startCaptureOnCameraThread failed
    // and we posted a retry.

    // Make sure onTextureFrameAvailable() is not called anymore.
    if (surfaceHelper != null) {
      surfaceHelper.stopListening();
    }
    cameraThreadHandler.removeCallbacks(cameraObserver);
    cameraStatistics.getAndResetFrameCount();
    Logging.d(TAG, "Stop preview.");
    if (camera != null) {
      camera.stopPreview();
      camera.setPreviewCallbackWithBuffer(null);
    }
    queuedBuffers.clear();
    captureFormat = null;

    Logging.d(TAG, "Release camera.");
    if (camera != null) {
      camera.release();
      camera = null;
    }
    if (eventsHandler != null) {
      eventsHandler.onCameraClosed();
    }
    Logging.d(TAG, "stopCaptureOnCameraThread done");
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
    if (cameraThreadHandler == null) {
      // The camera has been stopped.
      return;
    }
    checkIsOnCameraThread();
    if (!queuedBuffers.contains(data)) {
      // |data| is an old invalid buffer.
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
    if (cameraThreadHandler == null) {
      throw new RuntimeException("onTextureFrameAvailable() called after stopCapture().");
    }
    checkIsOnCameraThread();
    if (dropNextFrame)  {
      surfaceHelper.returnTextureFrame();
      dropNextFrame = false;
      return;
    }
    if (eventsHandler != null && !firstFrameReported) {
      eventsHandler.onFirstFrameAvailable();
      firstFrameReported = true;
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
}
