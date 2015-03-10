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
import java.util.List;
import java.util.concurrent.Exchanger;

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

  private Camera camera;  // Only non-null while capturing.
  private CameraThread cameraThread;
  private Handler cameraThreadHandler;
  private Context applicationContext;
  private int id;
  private Camera.CameraInfo info;
  private SurfaceTexture cameraSurfaceTexture;
  private int[] cameraGlTextures = null;
  private FramePool videoBuffers = null;
  private int width;
  private int height;
  private int framerate;
  private CapturerObserver frameObserver = null;
  // List of formats supported by all cameras. This list is filled once in order
  // to be able to switch cameras.
  private static List<List<CaptureFormat>> supportedFormats;

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

  public static String getDeviceName(int index) {
    Camera.CameraInfo info = new Camera.CameraInfo();
    Camera.getCameraInfo(index, info);
    String facing =
        (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) ? "front" : "back";
    return "Camera " + index + ", Facing " + facing
        + ", Orientation " + info.orientation;
  }

  public static String getNameOfFrontFacingDevice() {
    for (int i = 0; i < Camera.getNumberOfCameras(); ++i) {
      Camera.CameraInfo info = new Camera.CameraInfo();
      Camera.getCameraInfo(i, info);
      if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT)
        return getDeviceName(i);
    }
    return null;
  }

  public static String getNameOfBackFacingDevice() {
    for (int i = 0; i < Camera.getNumberOfCameras(); ++i) {
      Camera.CameraInfo info = new Camera.CameraInfo();
      Camera.getCameraInfo(i, info);
      if (info.facing == Camera.CameraInfo.CAMERA_FACING_BACK)
        return getDeviceName(i);
    }
    return null;
  }

  public static VideoCapturerAndroid create(String name) {
    VideoCapturer capturer = VideoCapturer.create(name);
    if (capturer != null)
      return (VideoCapturerAndroid) capturer;
    return null;
  }

  // Switch camera to the next valid camera id. This can only be called while
  // the camera is running.
  // Returns true on success. False if the next camera does not support the
  // current resolution.
  public synchronized boolean switchCamera() {
    if (Camera.getNumberOfCameras() < 2 )
      return false;

    if (cameraThread == null) {
      Log.e(TAG, "Camera has not been started");
      return false;
    }

    id = ++id % Camera.getNumberOfCameras();

    CaptureFormat formatToUse  = null;
    List<CaptureFormat> formats = supportedFormats.get(id);
    for (CaptureFormat format : formats) {
      if (format.width == width && format.height == height) {
        formatToUse = format;
        break;
      }
    }

    if (formatToUse == null) {
      Log.d(TAG, "No valid format found to switch camera.");
      return false;
    }

    cameraThreadHandler.post(new Runnable() {
      @Override public void run() {
        switchCameraOnCameraThread();
      }
    });
    return true;
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
  boolean init(String deviceName) {
    Log.d(TAG, "init " + deviceName);
    if (!initStatics())
      return false;

    boolean foundDevice = false;
    if (deviceName.isEmpty()) {
      this.id = 0;
      foundDevice = true;
    } else {
      for (int i = 0; i < Camera.getNumberOfCameras(); ++i) {
        if (deviceName.equals(getDeviceName(i))) {
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
      supportedFormats =
          new ArrayList<List<CaptureFormat>>(Camera.getNumberOfCameras());
      for (int i = 0; i < Camera.getNumberOfCameras(); ++i) {
        supportedFormats.add(getSupportedFormats(i));
      }
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

  static class CaptureFormat {
    public final int width;
    public final int height;
    public final int maxFramerate;
    public final int minFramerate;

    public CaptureFormat(int width, int height, int minFramerate,
        int maxFramerate) {
      this.width = width;
      this.height = height;
      this.minFramerate = minFramerate;
      this.maxFramerate = maxFramerate;
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
    Camera camera;
    camera = Camera.open(id);
    Camera.Parameters parameters;
    parameters = camera.getParameters();

    ArrayList<CaptureFormat> formatList = new ArrayList<CaptureFormat>();
    // getSupportedPreviewFpsRange returns a sorted list.
    List<int[]> listFpsRange = parameters.getSupportedPreviewFpsRange();
    int[] range = {0, 0};
    if (listFpsRange != null)
      range = listFpsRange.get(listFpsRange.size() -1);

    List<Camera.Size> supportedSizes = parameters.getSupportedPreviewSizes();
    for (Camera.Size size : supportedSizes) {
      if (size.width % 16 != 0) {
        // If the width is not a multiple of 16, the frames received from the
        // camera will have a stride != width when YV12 is used. Since we
        // currently only support tightly packed images, we simply ignore those
        // resolutions.
        continue;
      }
      formatList.add(new CaptureFormat(size.width, size.height,
          range[Camera.Parameters.PREVIEW_FPS_MIN_INDEX],
          range[Camera.Parameters.PREVIEW_FPS_MAX_INDEX]));
    }
    camera.release();
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
    if (width % 16 != 0) {
      throw new RuntimeException("width must be a multiple of 16." );
    }
    this.width = width;
    this.height = height;
    this.framerate = framerate;

    Exchanger<Handler> handlerExchanger = new Exchanger<Handler>();
    cameraThread = new CameraThread(handlerExchanger);
    cameraThread.start();
    cameraThreadHandler = exchange(handlerExchanger, null);
    // We must guarantee that buffers sent to an observer are kept alive until
    // stopCapture have completed. Therefore, create the buffers here and
    // abandon them after the camera thread have been stopped.
    videoBuffers = new FramePool(width, height, ImageFormat.YV12);
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
      this.camera = Camera.open(id);
      this.info = new Camera.CameraInfo();
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
        throw new RuntimeException(e);
      }

      Log.d(TAG, "Camera orientation: " + info.orientation +
          " .Device orientation: " + getDeviceOrientation());
      Camera.Parameters parameters = camera.getParameters();
      Log.d(TAG, "isVideoStabilizationSupported: " +
          parameters.isVideoStabilizationSupported());
      if (parameters.isVideoStabilizationSupported()) {
        parameters.setVideoStabilization(true);
      }

      int androidFramerate = framerate * 1000;
      int[] range = getFramerateRange(parameters, androidFramerate);
      if (range != null) {
        Log.d(TAG, "Start capturing: " + width + "x" + height + "@[" +
            range[Camera.Parameters.PREVIEW_FPS_MIN_INDEX]  + ":" +
            range[Camera.Parameters.PREVIEW_FPS_MAX_INDEX] + "]");
        parameters.setPreviewFpsRange(
            range[Camera.Parameters.PREVIEW_FPS_MIN_INDEX],
            range[Camera.Parameters.PREVIEW_FPS_MAX_INDEX]);
      }
      Camera.Size pictureSize = getPictureSize(parameters, width, height);
      parameters.setPictureSize(pictureSize.width, pictureSize.height);
      parameters.setPreviewSize(width, height);
      int format = ImageFormat.YV12;
      parameters.setPreviewFormat(format);
      camera.setParameters(parameters);
      // Note: setRecordingHint(true) actually decrease frame rate on N5.
      // parameters.setRecordingHint(true);

      videoBuffers.addBuffersAsCameraCallbackBuffers(camera);
      camera.setPreviewCallbackWithBuffer(this);
      camera.startPreview();
      frameObserver.OnCapturerStarted(true);
      return;
    } catch (RuntimeException e) {
      error = e;
    }
    Log.e(TAG, "startCapture failed", error);
    if (camera != null) {
      stopCaptureOnCameraThread();
      frameObserver.OnCapturerStarted(false);
    }
    frameObserver.OnCapturerStarted(false);
    return;
  }

  // Called by native code.  Returns true when camera is known to be stopped.
  synchronized void stopCapture() throws InterruptedException {
    Log.d(TAG, "stopCapture");
    cameraThreadHandler.post(new Runnable() {
        @Override public void run() {
          stopCaptureOnCameraThread();
        }
    });
    cameraThread.join();
    cameraThreadHandler = null;
    videoBuffers = null;
  }

  private void stopCaptureOnCameraThread() {
    Log.d(TAG, "stopCaptureOnCameraThread");
    doStopCaptureOnCamerathread();
    Looper.myLooper().quit();
    return;
  }

  private void doStopCaptureOnCamerathread() {
    try {
      camera.stopPreview();
      camera.setPreviewCallbackWithBuffer(null);

      camera.setPreviewTexture(null);
      cameraSurfaceTexture = null;
      if (cameraGlTextures != null) {
        GLES20.glDeleteTextures(1, cameraGlTextures, 0);
        cameraGlTextures = null;
      }

      camera.release();
      camera = null;
    } catch (IOException e) {
      Log.e(TAG, "Failed to stop camera", e);
    }
  }

  private void switchCameraOnCameraThread() {
    Log.d(TAG, "switchCameraOnCameraThread");

    doStopCaptureOnCamerathread();
    startCaptureOnCameraThread(width, height, framerate, frameObserver,
        applicationContext);
  }

  synchronized void returnBuffer(final long timeStamp) {
    cameraThreadHandler.post(new Runnable() {
      @Override public void run() {
        if (camera == null)
          return;
        videoBuffers.addBufferAsCameraCallbackBuffer(camera, timeStamp);
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

  private static int[] getFramerateRange(Camera.Parameters parameters,
                                         int framerate) {
    List<int[]> listFpsRange = parameters.getSupportedPreviewFpsRange();
    int[] bestRange = null;
    int bestRangeDiff = Integer.MAX_VALUE;
    for (int[] range : listFpsRange) {
      int rangeDiff =
          abs(framerate -range[Camera.Parameters.PREVIEW_FPS_MIN_INDEX])
          + abs(range[Camera.Parameters.PREVIEW_FPS_MAX_INDEX] - framerate);
      if (bestRangeDiff > rangeDiff) {
        bestRange = range;
        bestRangeDiff = rangeDiff;
      }
    }
    return bestRange;
  }

  private static Camera.Size getPictureSize(Camera.Parameters parameters,
      int width, int height) {
    int bestAreaDiff = Integer.MAX_VALUE;
    Camera.Size bestSize = null;
    int requestedArea = width * height;
    for (Camera.Size pictureSize : parameters.getSupportedPictureSizes()) {
      int areaDiff = abs(requestedArea
          - pictureSize.width * pictureSize.height);
      if (areaDiff < bestAreaDiff) {
        bestAreaDiff = areaDiff;
        bestSize = pictureSize;
      }
    }
    return bestSize;
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

    long captureTimeMs = SystemClock.elapsedRealtime();

    int rotation = getDeviceOrientation();
    if (info.facing == Camera.CameraInfo.CAMERA_FACING_BACK) {
      rotation = 360 - rotation;
    }
    rotation = (info.orientation + rotation) % 360;
    // Mark the frame owning |data| as used.
    // Note that since data is directBuffer,
    // data.length >= videoBuffers.frameSize.
    videoBuffers.reserveByteBuffer(data, captureTimeMs);
    frameObserver.OnFrameCaptured(data, videoBuffers.frameSize, rotation,
        captureTimeMs);
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
    private static int numCaptureBuffers = 3;
    private final Frame cameraFrames[];
    public final int frameSize;

    private static class Frame {
      private final ByteBuffer buffer;
      public long timeStamp = -1;

      Frame(int frameSize) {
        buffer = ByteBuffer.allocateDirect(frameSize);
      }

      byte[] data() {
        return buffer.array();
      }
    }

    FramePool(int width, int height, int format) {
      cameraFrames = new Frame[numCaptureBuffers];
      frameSize = width * height * ImageFormat.getBitsPerPixel(format) / 8;
      for (int i = 0; i < numCaptureBuffers; i++) {
        cameraFrames[i] = new Frame(frameSize);
      }
    }

    // Add all free buffers to |camera| that are currently not sent to a client.
    void addBuffersAsCameraCallbackBuffers(Camera camera) {
      for (Frame frame : cameraFrames) {
        if (frame.timeStamp < 0)
          camera.addCallbackBuffer(frame.data());
      }
    }

    void reserveByteBuffer(byte[] data, long timeStamp) {
      for (Frame frame : cameraFrames) {
        if (data == frame.data()) {
          frame.timeStamp = timeStamp;
          return;
        }
      }
      throw new RuntimeException("unknown data buffer?!?");
    }

    // Add the buffer with |timeStamp| to |camera|.
    void addBufferAsCameraCallbackBuffer(Camera camera, long timeStamp) {
      for (Frame frame : cameraFrames) {
        if (timeStamp == frame.timeStamp) {
          frame.timeStamp = -1;
          camera.addCallbackBuffer(frame.data());
          return;
        }
      }
      throw new RuntimeException("unknown data buffer returned?!?");
    }
  }

  // Interface used for providing callbacks to an observer.
  interface CapturerObserver {
    // Notify if the camera have been started successfully or not.
    // Called on a Java thread owned by VideoCapturerAndroid.
    abstract void OnCapturerStarted(boolean success);

    // Delivers a captured frame. Called on a Java thread owned by
    // VideoCapturerAndroid.
    abstract void OnFrameCaptured(byte[] data, int length, int rotation,
        long timeStamp);
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
    public void OnFrameCaptured(byte[] data, int length, int rotation,
        long timeStamp) {
      nativeOnFrameCaptured(nativeCapturer, data, length, rotation, timeStamp);
    }

    private native void nativeCapturerStarted(long nativeCapturer,
        boolean success);
    private native void nativeOnFrameCaptured(long nativeCapturer,
        byte[] data, int length, int rotation, long timeStamp);
  }
}
