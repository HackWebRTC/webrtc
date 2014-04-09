/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.videoengine;

import java.io.IOException;
import java.util.Locale;
import java.util.concurrent.Exchanger;
import java.util.concurrent.locks.ReentrantLock;

import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.graphics.YuvImage;
import android.hardware.Camera.PreviewCallback;
import android.hardware.Camera;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.SurfaceHolder.Callback;
import android.view.SurfaceHolder;

// Wrapper for android Camera, with support for direct local preview rendering.
// Threading notes: this class is called from ViE C++ code, and from Camera &
// SurfaceHolder Java callbacks.  Since these calls happen on different threads,
// the entry points to this class are all synchronized.  This shouldn't present
// a performance bottleneck because only onPreviewFrame() is called more than
// once (and is called serially on a single thread), so the lock should be
// uncontended.  Note that each of these synchronized methods must check
// |camera| for null to account for having possibly waited for stopCapture() to
// complete.
public class VideoCaptureAndroid implements PreviewCallback, Callback {
  private final static String TAG = "WEBRTC-JC";

  private static SurfaceHolder localPreview;
  private Camera camera;  // Only non-null while capturing.
  private CameraThread cameraThread;
  private Handler cameraThreadHandler;
  private final int id;
  private final Camera.CameraInfo info;
  private final long native_capturer;  // |VideoCaptureAndroid*| in C++.
  private SurfaceTexture dummySurfaceTexture;
  // Arbitrary queue depth.  Higher number means more memory allocated & held,
  // lower number means more sensitivity to processing time in the client (and
  // potentially stalling the capturer if it runs out of buffers to write to).
  private final int numCaptureBuffers = 3;

  // Requests future capturers to send their frames to |localPreview| directly.
  public static void setLocalPreview(SurfaceHolder localPreview) {
    // It is a gross hack that this is a class-static.  Doing it right would
    // mean plumbing this through the C++ API and using it from
    // webrtc/examples/android/media_demo's MediaEngine class.
    VideoCaptureAndroid.localPreview = localPreview;
  }

  public VideoCaptureAndroid(int id, long native_capturer) {
    this.id = id;
    this.native_capturer = native_capturer;
    this.info = new Camera.CameraInfo();
    Camera.getCameraInfo(id, info);
    Exchanger<Handler> handlerExchanger = new Exchanger<Handler>();
    cameraThread = new CameraThread(handlerExchanger);
    cameraThread.start();
    cameraThreadHandler = exchange(handlerExchanger, null);
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
  // thread that calls open(), so this is done on the CameraThread.  Since ViE
  // API needs a synchronous success return value we wait for the result.
  private synchronized boolean startCapture(
      final int width, final int height,
      final int min_mfps, final int max_mfps) {
    final Exchanger<Boolean> result = new Exchanger<Boolean>();
    cameraThreadHandler.post(new Runnable() {
        @Override public void run() {
          startCaptureOnCameraThread(width, height, min_mfps, max_mfps, result);
        }
      });
    return exchange(result, false);  // |false| is a dummy value here.
  }

  private void startCaptureOnCameraThread(
      int width, int height, int min_mfps, int max_mfps,
      Exchanger<Boolean> result) {
    Log.d(TAG, "startCapture: " + width + "x" + height + "@" +
        min_mfps + ":" + max_mfps);
    Throwable error = null;
    try {
      camera = Camera.open(id);

      if (localPreview != null) {
        localPreview.addCallback(this);
        if (localPreview.getSurface() != null &&
            localPreview.getSurface().isValid()) {
          camera.setPreviewDisplay(localPreview);
        }
      } else {
        // No local renderer (we only care about onPreviewFrame() buffers, not a
        // directly-displayed UI element).  Camera won't capture without
        // setPreview{Texture,Display}, so we create a dummy SurfaceTexture and
        // hand it over to Camera, but never listen for frame-ready callbacks,
        // and never call updateTexImage on it.
        try {
          // "42" because http://goo.gl/KaEn8
          dummySurfaceTexture = new SurfaceTexture(42);
          camera.setPreviewTexture(dummySurfaceTexture);
        } catch (IOException e) {
          throw new RuntimeException(e);
        }
      }

      Camera.Parameters parameters = camera.getParameters();
      Log.d(TAG, "isVideoStabilizationSupported: " +
          parameters.isVideoStabilizationSupported());
      if (parameters.isVideoStabilizationSupported()) {
        parameters.setVideoStabilization(true);
      }
      parameters.setPreviewSize(width, height);
      parameters.setPreviewFpsRange(min_mfps, max_mfps);
      int format = ImageFormat.NV21;
      parameters.setPreviewFormat(format);
      camera.setParameters(parameters);
      int bufSize = width * height * ImageFormat.getBitsPerPixel(format) / 8;
      for (int i = 0; i < numCaptureBuffers; i++) {
        camera.addCallbackBuffer(new byte[bufSize]);
      }
      camera.setPreviewCallbackWithBuffer(this);
      camera.startPreview();
      exchange(result, true);
      return;
    } catch (IOException e) {
      error = e;
    } catch (RuntimeException e) {
      error = e;
    }
    Log.e(TAG, "startCapture failed", error);
    if (camera != null) {
      Exchanger<Boolean> resultDropper = new Exchanger<Boolean>();
      stopCaptureOnCameraThread(resultDropper);
      exchange(resultDropper, false);
    }
    exchange(result, false);
    return;
  }

  // Called by native code.  Returns true when camera is known to be stopped.
  private synchronized boolean stopCapture() {
    final Exchanger<Boolean> result = new Exchanger<Boolean>();
    cameraThreadHandler.post(new Runnable() {
        @Override public void run() {
          stopCaptureOnCameraThread(result);
        }
      });
    return exchange(result, false);  // |false| is a dummy value here.
  }

  private void stopCaptureOnCameraThread(
      Exchanger<Boolean> result) {
    Log.d(TAG, "stopCapture");
    if (camera == null) {
      throw new RuntimeException("Camera is already stopped!");
    }
    Throwable error = null;
    try {
      camera.stopPreview();
      camera.setPreviewCallbackWithBuffer(null);
      if (localPreview != null) {
        localPreview.removeCallback(this);
        camera.setPreviewDisplay(null);
      } else {
        camera.setPreviewTexture(null);
      }
      camera.release();
      camera = null;
      exchange(result, true);
      return;
    } catch (IOException e) {
      error = e;
    } catch (RuntimeException e) {
      error = e;
    }
    Log.e(TAG, "Failed to stop camera", error);
    exchange(result, false);
    return;
  }

  private native void ProvideCameraFrame(
      byte[] data, int length, long captureObject);

  public synchronized void onPreviewFrame(byte[] data, Camera callbackCamera) {
    if (camera == null) {
      return;
    }
    if (camera != callbackCamera) {
      throw new RuntimeException("Unexpected camera in callback!");
    }
    ProvideCameraFrame(data, data.length, native_capturer);
    camera.addCallbackBuffer(data);
  }

  // Sets the rotation of the preview render window.
  // Does not affect the captured video image.
  // Called by native code.
  private synchronized void setPreviewRotation(final int rotation) {
    cameraThreadHandler.post(new Runnable() {
        @Override public void run() {
          setPreviewRotationOnCameraThread(rotation);
        }
      });
  }

  private void setPreviewRotationOnCameraThread(int rotation) {
    Log.v(TAG, "setPreviewRotation:" + rotation);

    if (camera == null) {
      return;
    }

    int resultRotation = 0;
    if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
      // This is a front facing camera.  SetDisplayOrientation will flip
      // the image horizontally before doing the rotation.
      resultRotation = ( 360 - rotation ) % 360; // Compensate for the mirror.
    } else {
      // Back-facing camera.
      resultRotation = rotation;
    }
    camera.setDisplayOrientation(resultRotation);
  }

  public synchronized void surfaceChanged(
      SurfaceHolder holder, int format, int width, int height) {
    Log.d(TAG, "VideoCaptureAndroid::surfaceChanged ignored: " +
        format + ": " + width + "x" + height);
  }

  public synchronized void surfaceCreated(final SurfaceHolder holder) {
    Log.d(TAG, "VideoCaptureAndroid::surfaceCreated");
    if (camera == null) {
      return;
    }
    final Exchanger<IOException> result = new Exchanger<IOException>();
    cameraThreadHandler.post(new Runnable() {
        @Override public void run() {
          setPreviewDisplayOnCameraThread(holder, result);
        }
      });
    IOException e = exchange(result, null);  // |null| is a dummy value here.
    if (e != null) {
      throw new RuntimeException(e);
    }
  }

  public synchronized void surfaceDestroyed(SurfaceHolder holder) {
    Log.d(TAG, "VideoCaptureAndroid::surfaceDestroyed");
    if (camera == null) {
      return;
    }
    final Exchanger<IOException> result = new Exchanger<IOException>();
    cameraThreadHandler.post(new Runnable() {
        @Override public void run() {
          setPreviewDisplayOnCameraThread(null, result);
        }
      });
    IOException e = exchange(result, null);  // |null| is a dummy value here.
    if (e != null) {
      throw new RuntimeException(e);
    }
  }

  private void setPreviewDisplayOnCameraThread(
      SurfaceHolder holder, Exchanger<IOException> result) {
    try {
      camera.setPreviewDisplay(holder);
    } catch (IOException e) {
      exchange(result, e);
      return;
    }
    exchange(result, null);
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
}
