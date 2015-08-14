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

import static java.lang.Math.abs;
import static java.lang.Math.ceil;

import android.content.Context;
import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.hardware.Camera.PreviewCallback;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.view.Surface;
import android.view.WindowManager;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Exchanger;
import java.util.concurrent.TimeUnit;

// Android specific implementation of VideoCapturer.
// An instance of this class can be created by an application using
// VideoCapturerAndroid.create();
// This class extends VideoCapturer with a method to easily switch between the
// front and back camera. It also provides methods for enumerating valid device
// names.
//
// Threading notes: this class is called from C++ code, and from Camera
// Java callbacks.  Since these calls happen on different threads,
// the entry points to this class are all synchronized.  This shouldn't present
// a performance bottleneck because only onPreviewFrame() is called more than
// once (and is called serially on a single thread), so the lock should be
// uncontended.  Note that each of these synchronized methods must check
// |camera| for null to account for having possibly waited for stopCapture() to
// complete.
@SuppressWarnings("deprecation")
public class VideoCapturerAndroid extends VideoCapturer implements PreviewCallback {
  private final static String TAG = "VideoCapturerAndroid";
  private final static int CAMERA_OBSERVER_PERIOD_MS = 5000;

  private Camera camera;  // Only non-null while capturing.
  private CameraThread cameraThread;
  private Handler cameraThreadHandler;
  private Context applicationContext;
  private int id;
  private Camera.CameraInfo info;
  private SurfaceTexture cameraSurfaceTexture;
  private int[] cameraGlTextures = null;
  private final FramePool videoBuffers = new FramePool();
  // Remember the requested format in case we want to switch cameras.
  private int requestedWidth;
  private int requestedHeight;
  private int requestedFramerate;
  // The capture format will be the closest supported format to the requested format.
  private CaptureFormat captureFormat;
  private int cameraFramesCount;
  private int captureBuffersCount;
  private volatile boolean pendingCameraSwitch;
  private CapturerObserver frameObserver = null;
  private CameraErrorHandler errorHandler = null;
  // List of formats supported by all cameras. This list is filled once in order
  // to be able to switch cameras.
  private static List<List<CaptureFormat>> supportedFormats;

  // Camera error callback.
  private final Camera.ErrorCallback cameraErrorCallback =
      new Camera.ErrorCallback() {
    @Override
    public void onError(int error, Camera camera) {
      String errorMessage;
      if (error == android.hardware.Camera.CAMERA_ERROR_SERVER_DIED) {
        errorMessage = "Camera server died!";
      } else {
        errorMessage = "Camera error: " + error;
      }
      Log.e(TAG, errorMessage);
      if (errorHandler != null) {
        errorHandler.onCameraError(errorMessage);
      }
    }
  };

  // Camera observer - monitors camera framerate and amount of available
  // camera buffers. Observer is excecuted on camera thread.
  private final Runnable cameraObserver = new Runnable() {
    @Override
    public void run() {
      int cameraFps = (cameraFramesCount * 1000 + CAMERA_OBSERVER_PERIOD_MS / 2)
          / CAMERA_OBSERVER_PERIOD_MS;
      double averageCaptureBuffersCount = 0;
      if (cameraFramesCount > 0) {
        averageCaptureBuffersCount =
            (double)captureBuffersCount / cameraFramesCount;
      }
      Log.d(TAG, "Camera fps: " + cameraFps + ". CaptureBuffers: " +
          String.format("%.1f", averageCaptureBuffersCount) +
          ". Pending buffers: " + videoBuffers.pendingFramesTimeStamps());
      if (cameraFramesCount == 0) {
        Log.e(TAG, "Camera freezed.");
        if (errorHandler != null) {
          errorHandler.onCameraError("Camera failure.");
        }
      } else {
        cameraFramesCount = 0;
        captureBuffersCount = 0;
        if (cameraThreadHandler != null) {
          cameraThreadHandler.postDelayed(this, CAMERA_OBSERVER_PERIOD_MS);
        }
      }
    }
  };

  // Camera error handler - invoked when camera stops receiving frames
  // or any camera exception happens on camera thread.
  public static interface CameraErrorHandler {
    public void onCameraError(String errorDescription);
  }

  // Returns device names that can be used to create a new VideoCapturerAndroid.
  public static String[] getDeviceNames() {
    String[] names = new String[Camera.getNumberOfCameras()];
    for (int i = 0; i < Camera.getNumberOfCameras(); ++i) {
      names[i] = getDeviceName(i);
    }
    return names;
  }

  // Returns number of cameras on device.
  public static int getDeviceCount() {
    return Camera.getNumberOfCameras();
  }

  // Returns the name of the camera with camera index. Returns null if the
  // camera can not be used.
  public static String getDeviceName(int index) {
    Camera.CameraInfo info = new Camera.CameraInfo();
    try {
      Camera.getCameraInfo(index, info);
    } catch (Exception e) {
      Log.e(TAG, "getCameraInfo failed on index " + index,e);
      return null;
    }

    String facing =
        (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) ? "front" : "back";
    return "Camera " + index + ", Facing " + facing
        + ", Orientation " + info.orientation;
  }

  // Returns the name of the front facing camera. Returns null if the
  // camera can not be used or does not exist.
  public static String getNameOfFrontFacingDevice() {
    for (int i = 0; i < Camera.getNumberOfCameras(); ++i) {
      Camera.CameraInfo info = new Camera.CameraInfo();
      try {
        Camera.getCameraInfo(i, info);
        if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT)
          return getDeviceName(i);
      } catch (Exception e) {
        Log.e(TAG, "getCameraInfo failed on index " + i, e);
      }
    }
    return null;
  }

  // Returns the name of the back facing camera. Returns null if the
  // camera can not be used or does not exist.
  public static String getNameOfBackFacingDevice() {
    for (int i = 0; i < Camera.getNumberOfCameras(); ++i) {
      Camera.CameraInfo info = new Camera.CameraInfo();
      try {
        Camera.getCameraInfo(i, info);
        if (info.facing == Camera.CameraInfo.CAMERA_FACING_BACK)
          return getDeviceName(i);
      } catch (Exception e) {
        Log.e(TAG, "getCameraInfo failed on index " + i, e);
      }
    }
    return null;
  }

  public static VideoCapturerAndroid create(String name,
      CameraErrorHandler errorHandler) {
    VideoCapturer capturer = VideoCapturer.create(name);
    if (capturer != null) {
      VideoCapturerAndroid capturerAndroid = (VideoCapturerAndroid) capturer;
      capturerAndroid.errorHandler = errorHandler;
      return capturerAndroid;
    }
    return null;
  }

  // Switch camera to the next valid camera id. This can only be called while
  // the camera is running.
  // Returns true on success. False if the next camera does not support the
  // current resolution.
  public synchronized boolean switchCamera(final Runnable switchDoneEvent) {
    if (Camera.getNumberOfCameras() < 2 )
      return false;

    if (cameraThread == null) {
      Log.e(TAG, "Camera has not been started");
      return false;
    }
    if (pendingCameraSwitch) {
      // Do not handle multiple camera switch request to avoid blocking
      // camera thread by handling too many switch request from a queue.
      Log.w(TAG, "Ignoring camera switch request.");
      return false;
    }

    pendingCameraSwitch = true;
    id = (id + 1) % Camera.getNumberOfCameras();
    cameraThreadHandler.post(new Runnable() {
      @Override public void run() {
        switchCameraOnCameraThread(switchDoneEvent);
      }
    });
    return true;
  }

  // Requests a new output format from the video capturer. Captured frames
  // by the camera will be scaled/or dropped by the video capturer.
  public synchronized void onOutputFormatRequest(
      final int width, final int height, final int fps) {
    if (cameraThreadHandler == null) {
      Log.e(TAG, "Calling onOutputFormatRequest() for already stopped camera.");
      return;
    }
    cameraThreadHandler.post(new Runnable() {
      @Override public void run() {
        onOutputFormatRequestOnCameraThread(width, height, fps);
      }
    });
  }

  // Reconfigure the camera to capture in a new format. This should only be called while the camera
  // is running.
  public synchronized void changeCaptureFormat(
      final int width, final int height, final int framerate) {
    if (cameraThreadHandler == null) {
      Log.e(TAG, "Calling changeCaptureFormat() for already stopped camera.");
      return;
    }
    cameraThreadHandler.post(new Runnable() {
      @Override public void run() {
        startPreviewOnCameraThread(width, height, framerate);
      }
    });
  }

  public synchronized List<CaptureFormat> getSupportedFormats() {
    return supportedFormats.get(id);
  }

  private VideoCapturerAndroid() {
    Log.d(TAG, "VideoCapturerAndroid");
  }

  // Called by native code.
  // Enumerates resolution and frame rates for all cameras to be able to switch
  // cameras. Initializes local variables for the camera named |deviceName| and
  // starts a thread to be used for capturing.
  // If deviceName is empty, the first available device is used in order to be
  // compatible with the generic VideoCapturer class.
  synchronized boolean init(String deviceName) {
    Log.d(TAG, "init: " + deviceName);
    if (deviceName == null || !initStatics())
      return false;

    boolean foundDevice = false;
    if (deviceName.isEmpty()) {
      this.id = 0;
      foundDevice = true;
    } else {
      for (int i = 0; i < Camera.getNumberOfCameras(); ++i) {
        String existing_device = getDeviceName(i);
        if (existing_device != null && deviceName.equals(existing_device)) {
          this.id = i;
          foundDevice = true;
        }
      }
    }
    return foundDevice;
  }

  private static boolean initStatics() {
    if (supportedFormats != null)
      return true;
    try {
      Log.d(TAG, "Get supported formats.");
      supportedFormats =
          new ArrayList<List<CaptureFormat>>(Camera.getNumberOfCameras());
      // Start requesting supported formats from camera with the highest index
      // (back camera) first. If it fails then likely camera is in bad state.
      for (int i = Camera.getNumberOfCameras() - 1; i >= 0; i--) {
        ArrayList<CaptureFormat> supportedFormat = getSupportedFormats(i);
        if (supportedFormat.size() == 0) {
          Log.e(TAG, "Fail to get supported formats for camera " + i);
          supportedFormats = null;
          return false;
        }
        supportedFormats.add(supportedFormat);
      }
      // Reverse the list since it is filled in reverse order.
      Collections.reverse(supportedFormats);
      Log.d(TAG, "Get supported formats done.");
      return true;
    } catch (Exception e) {
      supportedFormats = null;
      Log.e(TAG, "InitStatics failed",e);
    }
    return false;
  }

  String getSupportedFormatsAsJson() throws JSONException {
    return getSupportedFormatsAsJson(id);
  }

  public static class CaptureFormat {
    public final int width;
    public final int height;
    public final int maxFramerate;
    public final int minFramerate;
    // TODO(hbos): If VideoCapturerAndroid.startCapture is updated to support
    // other image formats then this needs to be updated and
    // VideoCapturerAndroid.getSupportedFormats need to return CaptureFormats of
    // all imageFormats.
    public final int imageFormat = ImageFormat.YV12;

    public CaptureFormat(int width, int height, int minFramerate,
        int maxFramerate) {
      this.width = width;
      this.height = height;
      this.minFramerate = minFramerate;
      this.maxFramerate = maxFramerate;
    }

    // Calculates the frame size of this capture format.
    public int frameSize() {
      return frameSize(width, height, imageFormat);
    }

    // Calculates the frame size of the specified image format. Currently only
    // supporting ImageFormat.YV12. The YV12's stride is the closest rounded up
    // multiple of 16 of the width and width and height are always even.
    // Android guarantees this:
    // http://developer.android.com/reference/android/hardware/Camera.Parameters.html#setPreviewFormat%28int%29
    public static int frameSize(int width, int height, int imageFormat) {
      if (imageFormat != ImageFormat.YV12) {
        throw new UnsupportedOperationException("Don't know how to calculate "
            + "the frame size of non-YV12 image formats.");
      }
      int yStride = roundUp(width, 16);
      int uvStride = roundUp(yStride / 2, 16);
      int ySize = yStride * height;
      int uvSize = uvStride * height / 2;
      return ySize + uvSize * 2;
    }

    // Rounds up |x| to the closest value that is a multiple of |alignment|.
    private static int roundUp(int x, int alignment) {
      return (int)ceil(x / (double)alignment) * alignment;
    }

    @Override
    public String toString() {
      return width + "x" + height + "@[" + minFramerate + ":" + maxFramerate + "]";
    }

    @Override
    public boolean equals(Object that) {
      if (!(that instanceof CaptureFormat)) {
        return false;
      }
      final CaptureFormat c = (CaptureFormat) that;
      return width == c.width && height == c.height && maxFramerate == c.maxFramerate
          && minFramerate == c.minFramerate;
    }
  }

  private static String getSupportedFormatsAsJson(int id) throws JSONException {
    List<CaptureFormat> formats = supportedFormats.get(id);
    JSONArray json_formats = new JSONArray();
    for (CaptureFormat format : formats) {
      JSONObject json_format = new JSONObject();
      json_format.put("width", format.width);
      json_format.put("height", format.height);
      json_format.put("framerate", (format.maxFramerate + 999) / 1000);
      json_formats.put(json_format);
    }
    Log.d(TAG, "Supported formats for camera " + id + ": "
        +  json_formats.toString(2));
    return json_formats.toString();
  }

  // Returns a list of CaptureFormat for the camera with index id.
  static ArrayList<CaptureFormat> getSupportedFormats(int id) {
    ArrayList<CaptureFormat> formatList = new ArrayList<CaptureFormat>();

    Camera camera;
    try {
      Log.d(TAG, "Opening camera " + id);
      camera = Camera.open(id);
    } catch (Exception e) {
      Log.e(TAG, "Open camera failed on id " + id, e);
      return formatList;
    }

    try {
      Camera.Parameters parameters;
      parameters = camera.getParameters();
      // getSupportedPreviewFpsRange returns a sorted list.
      List<int[]> listFpsRange = parameters.getSupportedPreviewFpsRange();
      int[] range = {0, 0};
      if (listFpsRange != null)
        range = listFpsRange.get(listFpsRange.size() -1);

      List<Camera.Size> supportedSizes = parameters.getSupportedPreviewSizes();
      for (Camera.Size size : supportedSizes) {
        formatList.add(new CaptureFormat(size.width, size.height,
            range[Camera.Parameters.PREVIEW_FPS_MIN_INDEX],
            range[Camera.Parameters.PREVIEW_FPS_MAX_INDEX]));
      }
    } catch (Exception e) {
      Log.e(TAG, "getSupportedFormats failed on id " + id, e);
    }
    camera.release();
    camera = null;
    return formatList;
  }

  private class CameraThread extends Thread {
    private Exchanger<Handler> handlerExchanger;
    public CameraThread(Exchanger<Handler> handlerExchanger) {
      this.handlerExchanger = handlerExchanger;
    }

    @Override public void run() {
      Looper.prepare();
      exchange(handlerExchanger, new Handler());
      Looper.loop();
    }
  }

  // Called by native code.  Returns true if capturer is started.
  //
  // Note that this actually opens the camera, and Camera callbacks run on the
  // thread that calls open(), so this is done on the CameraThread.  Since the
  // API needs a synchronous success return value we wait for the result.
  synchronized void startCapture(
      final int width, final int height, final int framerate,
      final Context applicationContext, final CapturerObserver frameObserver) {
    Log.d(TAG, "startCapture requested: " + width + "x" + height
        + "@" + framerate);
    if (applicationContext == null) {
      throw new RuntimeException("applicationContext not set.");
    }
    if (frameObserver == null) {
      throw new RuntimeException("frameObserver not set.");
    }
    if (cameraThreadHandler != null) {
      throw new RuntimeException("Camera has already been started.");
    }

    Exchanger<Handler> handlerExchanger = new Exchanger<Handler>();
    cameraThread = new CameraThread(handlerExchanger);
    cameraThread.start();
    cameraThreadHandler = exchange(handlerExchanger, null);
    cameraThreadHandler.post(new Runnable() {
      @Override public void run() {
        startCaptureOnCameraThread(width, height, framerate, frameObserver,
            applicationContext);
      }
    });
  }

  private void startCaptureOnCameraThread(
      int width, int height, int framerate, CapturerObserver frameObserver,
      Context applicationContext) {
    Throwable error = null;
    this.applicationContext = applicationContext;
    this.frameObserver = frameObserver;
    try {
      Log.d(TAG, "Opening camera " + id);
      camera = Camera.open(id);
      info = new Camera.CameraInfo();
      Camera.getCameraInfo(id, info);
      // No local renderer (we only care about onPreviewFrame() buffers, not a
      // directly-displayed UI element).  Camera won't capture without
      // setPreview{Texture,Display}, so we create a SurfaceTexture and hand
      // it over to Camera, but never listen for frame-ready callbacks,
      // and never call updateTexImage on it.
      try {
        cameraSurfaceTexture = null;

        cameraGlTextures = new int[1];
        // Generate one texture pointer and bind it as an external texture.
        GLES20.glGenTextures(1, cameraGlTextures, 0);
        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            cameraGlTextures[0]);
        GLES20.glTexParameterf(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameterf(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);

        cameraSurfaceTexture = new SurfaceTexture(cameraGlTextures[0]);
        cameraSurfaceTexture.setOnFrameAvailableListener(null);

        camera.setPreviewTexture(cameraSurfaceTexture);
      } catch (IOException e) {
        Log.e(TAG, "setPreviewTexture failed", error);
        throw new RuntimeException(e);
      }

      Log.d(TAG, "Camera orientation: " + info.orientation +
          " .Device orientation: " + getDeviceOrientation());
      camera.setErrorCallback(cameraErrorCallback);
      startPreviewOnCameraThread(width, height, framerate);
      frameObserver.OnCapturerStarted(true);

      // Start camera observer.
      cameraFramesCount = 0;
      captureBuffersCount = 0;
      cameraThreadHandler.postDelayed(cameraObserver, CAMERA_OBSERVER_PERIOD_MS);
      return;
    } catch (RuntimeException e) {
      error = e;
    }
    Log.e(TAG, "startCapture failed", error);
    stopCaptureOnCameraThread();
    cameraThreadHandler = null;
    frameObserver.OnCapturerStarted(false);
    if (errorHandler != null) {
      errorHandler.onCameraError("Camera can not be started.");
    }
    return;
  }

  // (Re)start preview with the closest supported format to |width| x |height| @ |framerate|.
  private void startPreviewOnCameraThread(int width, int height, int framerate) {
    Log.d(TAG, "startPreviewOnCameraThread requested: " + width + "x" + height + "@" + framerate);
    if (camera == null) {
      Log.e(TAG, "Calling startPreviewOnCameraThread on stopped camera.");
      return;
    }

    requestedWidth = width;
    requestedHeight = height;
    requestedFramerate = framerate;

    // Find closest supported format for |width| x |height| @ |framerate|.
    final Camera.Parameters parameters = camera.getParameters();
    final int[] range = getFramerateRange(parameters, framerate * 1000);
    final Camera.Size previewSize =
        getClosestSupportedSize(parameters.getSupportedPreviewSizes(), width, height);
    final CaptureFormat captureFormat = new CaptureFormat(
        previewSize.width, previewSize.height,
        range[Camera.Parameters.PREVIEW_FPS_MIN_INDEX],
        range[Camera.Parameters.PREVIEW_FPS_MAX_INDEX]);

    // Check if we are already using this capture format, then we don't need to do anything.
    if (captureFormat.equals(this.captureFormat)) {
      return;
    }

    // Update camera parameters.
    Log.d(TAG, "isVideoStabilizationSupported: " +
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
    parameters.setPreviewFormat(captureFormat.imageFormat);
    // Picture size is for taking pictures and not for preview/video, but we need to set it anyway
    // as a workaround for an aspect ratio problem on Nexus 7.
    final Camera.Size pictureSize =
        getClosestSupportedSize(parameters.getSupportedPictureSizes(), width, height);
    parameters.setPictureSize(pictureSize.width, pictureSize.height);

    // Temporarily stop preview if it's already running.
    if (this.captureFormat != null) {
      camera.stopPreview();
      // Calling |setPreviewCallbackWithBuffer| with null should clear the internal camera buffer
      // queue, but sometimes we receive a frame with the old resolution after this call anyway.
      camera.setPreviewCallbackWithBuffer(null);
    }

    // (Re)start preview.
    Log.d(TAG, "Start capturing: " + captureFormat);
    this.captureFormat = captureFormat;
    camera.setParameters(parameters);
    videoBuffers.queueCameraBuffers(captureFormat.frameSize(), camera);
    camera.setPreviewCallbackWithBuffer(this);
    camera.startPreview();
  }

  // Called by native code.  Returns true when camera is known to be stopped.
  synchronized void stopCapture() throws InterruptedException {
    if (cameraThreadHandler == null) {
      Log.e(TAG, "Calling stopCapture() for already stopped camera.");
      return;
    }
    Log.d(TAG, "stopCapture");
    cameraThreadHandler.post(new Runnable() {
        @Override public void run() {
          stopCaptureOnCameraThread();
        }
    });
    cameraThread.join();
    cameraThreadHandler = null;
    Log.d(TAG, "stopCapture done");
  }

  private void stopCaptureOnCameraThread() {
    doStopCaptureOnCameraThread();
    Looper.myLooper().quit();
    return;
  }

  private void doStopCaptureOnCameraThread() {
    Log.d(TAG, "stopCaptureOnCameraThread");
    if (camera == null) {
      return;
    }
    try {
      cameraThreadHandler.removeCallbacks(cameraObserver);
      Log.d(TAG, "Stop preview.");
      camera.stopPreview();
      camera.setPreviewCallbackWithBuffer(null);
      videoBuffers.stopReturnBuffersToCamera();
      captureFormat = null;

      camera.setPreviewTexture(null);
      cameraSurfaceTexture = null;
      if (cameraGlTextures != null) {
        GLES20.glDeleteTextures(1, cameraGlTextures, 0);
        cameraGlTextures = null;
      }

      Log.d(TAG, "Release camera.");
      camera.release();
      camera = null;
    } catch (IOException e) {
      Log.e(TAG, "Failed to stop camera", e);
    }
  }

  private void switchCameraOnCameraThread(Runnable switchDoneEvent) {
    Log.d(TAG, "switchCameraOnCameraThread");

    doStopCaptureOnCameraThread();
    startCaptureOnCameraThread(requestedWidth, requestedHeight, requestedFramerate, frameObserver,
        applicationContext);
    pendingCameraSwitch = false;
    Log.d(TAG, "switchCameraOnCameraThread done");
    if (switchDoneEvent != null) {
      switchDoneEvent.run();
    }
  }

  private void onOutputFormatRequestOnCameraThread(
      int width, int height, int fps) {
    if (camera == null) {
      return;
    }
    Log.d(TAG, "onOutputFormatRequestOnCameraThread: " + width + "x" + height +
        "@" + fps);
    frameObserver.OnOutputFormatRequest(width, height, fps);
  }

  synchronized void returnBuffer(final long timeStamp) {
    if (cameraThreadHandler == null) {
      // The camera has been stopped.
      videoBuffers.returnBuffer(timeStamp);
      return;
    }
    cameraThreadHandler.post(new Runnable() {
      @Override public void run() {
        videoBuffers.returnBuffer(timeStamp);
      }
    });
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

  // Helper class for finding the closest supported format for the two functions below.
  private static abstract class ClosestComparator<T> implements Comparator<T> {
    // Difference between supported and requested parameter.
    abstract int diff(T supportedParameter);

    @Override
    public int compare(T t1, T t2) {
      return diff(t1) - diff(t2);
    }
  }

  private static int[] getFramerateRange(Camera.Parameters parameters, final int framerate) {
    List<int[]> listFpsRange = parameters.getSupportedPreviewFpsRange();
    if (listFpsRange.isEmpty()) {
      Log.w(TAG, "No supported preview fps range");
      return new int[]{0, 0};
    }
    return Collections.min(listFpsRange,
        new ClosestComparator<int[]>() {
          @Override int diff(int[] range) {
            return abs(framerate - range[Camera.Parameters.PREVIEW_FPS_MIN_INDEX])
                + abs(framerate - range[Camera.Parameters.PREVIEW_FPS_MAX_INDEX]);
          }
     });
  }

  private static Camera.Size getClosestSupportedSize(
      List<Camera.Size> supportedSizes, final int requestedWidth, final int requestedHeight) {
    return Collections.min(supportedSizes,
        new ClosestComparator<Camera.Size>() {
          @Override int diff(Camera.Size size) {
            return abs(requestedWidth - size.width) + abs(requestedHeight - size.height);
          }
     });
  }

  // Called on cameraThread so must not "synchronized".
  @Override
  public void onPreviewFrame(byte[] data, Camera callbackCamera) {
    if (Thread.currentThread() != cameraThread) {
      throw new RuntimeException("Camera callback not on camera thread?!?");
    }
    if (camera == null) {
      return;
    }
    if (camera != callbackCamera) {
      throw new RuntimeException("Unexpected camera in callback!");
    }

    final long captureTimeNs =
        TimeUnit.MILLISECONDS.toNanos(SystemClock.elapsedRealtime());

    captureBuffersCount += videoBuffers.numCaptureBuffersAvailable();
    int rotation = getDeviceOrientation();
    if (info.facing == Camera.CameraInfo.CAMERA_FACING_BACK) {
      rotation = 360 - rotation;
    }
    rotation = (info.orientation + rotation) % 360;
    // Mark the frame owning |data| as used.
    // Note that since data is directBuffer,
    // data.length >= videoBuffers.frameSize.
    if (videoBuffers.reserveByteBuffer(data, captureTimeNs)) {
      cameraFramesCount++;
      frameObserver.OnFrameCaptured(data, videoBuffers.frameSize, captureFormat.width,
          captureFormat.height, rotation, captureTimeNs);
    } else {
      Log.w(TAG, "reserveByteBuffer failed - dropping frame.");
    }
  }

  // runCameraThreadUntilIdle make sure all posted messages to the cameraThread
  // is processed before returning. It does that by itself posting a message to
  // to the message queue and waits until is has been processed.
  // It is used in tests.
  void runCameraThreadUntilIdle() {
    if (cameraThreadHandler == null)
      return;
    final Exchanger<Boolean> result = new Exchanger<Boolean>();
    cameraThreadHandler.post(new Runnable() {
      @Override public void run() {
        exchange(result, true); // |true| is a dummy here.
      }
    });
    exchange(result, false);  // |false| is a dummy value here.
    return;
  }

  // Exchanges |value| with |exchanger|, converting InterruptedExceptions to
  // RuntimeExceptions (since we expect never to see these).
  private static <T> T exchange(Exchanger<T> exchanger, T value) {
    try {
      return exchanger.exchange(value);
    } catch (InterruptedException e) {
      throw new RuntimeException(e);
    }
  }

  // Class used for allocating and bookkeeping video frames. All buffers are
  // direct allocated so that they can be directly used from native code.
  private static class FramePool {
    // Arbitrary queue depth.  Higher number means more memory allocated & held,
    // lower number means more sensitivity to processing time in the client (and
    // potentially stalling the capturer if it runs out of buffers to write to).
    private static final int numCaptureBuffers = 3;
    // This container tracks the buffers added as camera callback buffers. It is needed for finding
    // the corresponding ByteBuffer given a byte[].
    private final Map<byte[], ByteBuffer> queuedBuffers = new IdentityHashMap<byte[], ByteBuffer>();
    // This container tracks the frames that have been sent but not returned. It is needed for
    // keeping the buffers alive and for finding the corresponding ByteBuffer given a timestamp.
    private final Map<Long, ByteBuffer> pendingBuffers = new HashMap<Long, ByteBuffer>();
    private int frameSize = 0;
    private Camera camera;

    int numCaptureBuffersAvailable() {
      return queuedBuffers.size();
    }

    // Discards previous queued buffers and adds new callback buffers to camera.
    void queueCameraBuffers(int frameSize, Camera camera) {
      this.camera = camera;
      this.frameSize = frameSize;

      queuedBuffers.clear();
      for (int i = 0; i < numCaptureBuffers; ++i) {
        final ByteBuffer buffer = ByteBuffer.allocateDirect(frameSize);
        camera.addCallbackBuffer(buffer.array());
        queuedBuffers.put(buffer.array(), buffer);
      }
      Log.d(TAG, "queueCameraBuffers enqueued " + numCaptureBuffers
          + " buffers of size " + frameSize + ".");
    }

    String pendingFramesTimeStamps() {
      List<Long> timeStampsMs = new ArrayList<Long>();
      for (Long timeStampNs : pendingBuffers.keySet()) {
        timeStampsMs.add(TimeUnit.NANOSECONDS.toMillis(timeStampNs));
      }
      return timeStampsMs.toString();
    }

    void stopReturnBuffersToCamera() {
      this.camera = null;
      queuedBuffers.clear();
      // Frames in |pendingBuffers| need to be kept alive until they are returned.
      Log.d(TAG, "stopReturnBuffersToCamera called."
            + (pendingBuffers.isEmpty() ?
                   " All buffers have been returned."
                   : " Pending buffers: " + pendingFramesTimeStamps() + "."));
    }

    boolean reserveByteBuffer(byte[] data, long timeStamp) {
      final ByteBuffer buffer = queuedBuffers.remove(data);
      if (buffer == null) {
        // Frames might be posted to |onPreviewFrame| with the previous format while changing
        // capture format in |startPreviewOnCameraThread|. Drop these old frames.
        Log.w(TAG, "Received callback buffer from previous configuration with length: "
            + (data == null ? "null" : data.length));
        return false;
      }
      if (buffer.capacity() != frameSize) {
        throw new IllegalStateException("Callback buffer has unexpected frame size");
      }
      if (pendingBuffers.containsKey(timeStamp)) {
        Log.e(TAG, "Timestamp already present in pending buffers - they need to be unique");
        return false;
      }
      pendingBuffers.put(timeStamp, buffer);
      if (queuedBuffers.isEmpty()) {
        Log.v(TAG, "Camera is running out of capture buffers."
            + " Pending buffers: " + pendingFramesTimeStamps());
      }
      return true;
    }

    void returnBuffer(long timeStamp) {
      final ByteBuffer returnedFrame = pendingBuffers.remove(timeStamp);
      if (returnedFrame == null) {
        throw new RuntimeException("unknown data buffer with time stamp "
            + timeStamp + "returned?!?");
      }

      if (camera != null && returnedFrame.capacity() == frameSize) {
        camera.addCallbackBuffer(returnedFrame.array());
        if (queuedBuffers.isEmpty()) {
          Log.v(TAG, "Frame returned when camera is running out of capture"
              + " buffers for TS " + TimeUnit.NANOSECONDS.toMillis(timeStamp));
        }
        queuedBuffers.put(returnedFrame.array(), returnedFrame);
        return;
      }

      if (returnedFrame.capacity() != frameSize) {
        Log.d(TAG, "returnBuffer with time stamp "
            + TimeUnit.NANOSECONDS.toMillis(timeStamp)
            + " called with old frame size, " + returnedFrame.capacity() + ".");
        // Since this frame has the wrong size, don't requeue it. Frames with the correct size are
        // created in queueCameraBuffers so this must be an old buffer.
        return;
      }

      Log.d(TAG, "returnBuffer with time stamp "
          + TimeUnit.NANOSECONDS.toMillis(timeStamp)
          + " called after camera has been stopped.");
    }
  }

  // Interface used for providing callbacks to an observer.
  interface CapturerObserver {
    // Notify if the camera have been started successfully or not.
    // Called on a Java thread owned by VideoCapturerAndroid.
    abstract void OnCapturerStarted(boolean success);

    // Delivers a captured frame. Called on a Java thread owned by
    // VideoCapturerAndroid.
    abstract void OnFrameCaptured(byte[] data, int length, int width, int height,
        int rotation, long timeStamp);

    // Requests an output format from the video capturer. Captured frames
    // by the camera will be scaled/or dropped by the video capturer.
    // Called on a Java thread owned by VideoCapturerAndroid.
    abstract void OnOutputFormatRequest(int width, int height, int fps);
  }

  // An implementation of CapturerObserver that forwards all calls from
  // Java to the C layer.
  static class NativeObserver implements CapturerObserver {
    private final long nativeCapturer;

    public NativeObserver(long nativeCapturer) {
      this.nativeCapturer = nativeCapturer;
    }

    @Override
    public void OnCapturerStarted(boolean success) {
      nativeCapturerStarted(nativeCapturer, success);
    }

    @Override
    public void OnFrameCaptured(byte[] data, int length, int width, int height,
        int rotation, long timeStamp) {
      nativeOnFrameCaptured(nativeCapturer, data, length, width, height, rotation, timeStamp);
    }

    @Override
    public void OnOutputFormatRequest(int width, int height, int fps) {
      nativeOnOutputFormatRequest(nativeCapturer, width, height, fps);
    }

    private native void nativeCapturerStarted(long nativeCapturer,
        boolean success);
    private native void nativeOnFrameCaptured(long nativeCapturer,
        byte[] data, int length, int width, int height, int rotation, long timeStamp);
    private native void nativeOnOutputFormatRequest(long nativeCapturer,
        int width, int height, int fps);
  }
}
