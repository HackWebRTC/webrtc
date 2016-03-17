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

import static java.lang.Math.abs;
import android.graphics.ImageFormat;

import org.webrtc.Logging;

import java.util.Collections;
import java.util.Comparator;
import java.util.List;

@SuppressWarnings("deprecation")
public class CameraEnumerationAndroid {
  private final static String TAG = "CameraEnumerationAndroid";
  // Synchronized on |CameraEnumerationAndroid.this|.
  private static Enumerator enumerator = new CameraEnumerator();

  public interface Enumerator {
    /**
     * Returns a list of supported CaptureFormats for the camera with index |cameraId|.
     */
    List<CaptureFormat> getSupportedFormats(int cameraId);
  }

  public static synchronized void setEnumerator(Enumerator enumerator) {
    CameraEnumerationAndroid.enumerator = enumerator;
  }

  public static synchronized List<CaptureFormat> getSupportedFormats(int cameraId) {
    final List<CaptureFormat> formats = enumerator.getSupportedFormats(cameraId);
    Logging.d(TAG, "Supported formats for camera " + cameraId + ": " + formats);
    return formats;
  }

  public static class CaptureFormat {
    public final int width;
    public final int height;
    public final int maxFramerate;
    public final int minFramerate;
    // TODO(hbos): If VideoCapturer.startCapture is updated to support other image formats then this
    // needs to be updated and VideoCapturer.getSupportedFormats need to return CaptureFormats of
    // all imageFormats.
    public final int imageFormat = ImageFormat.NV21;

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
    // supporting ImageFormat.NV21.
    // The size is width * height * number of bytes per pixel.
    // http://developer.android.com/reference/android/hardware/Camera.html#addCallbackBuffer(byte[])
    public static int frameSize(int width, int height, int imageFormat) {
      if (imageFormat != ImageFormat.NV21) {
        throw new UnsupportedOperationException("Don't know how to calculate "
            + "the frame size of non-NV21 image formats.");
      }
      return (width * height * ImageFormat.getBitsPerPixel(imageFormat)) / 8;
    }

    @Override
    public String toString() {
      return width + "x" + height + "@[" + minFramerate + ":" + maxFramerate + "]";
    }

    public boolean isSameFormat(final CaptureFormat that) {
      if (that == null) {
        return false;
      }
      return width == that.width && height == that.height && maxFramerate == that.maxFramerate
          && minFramerate == that.minFramerate;
    }
  }

  // Returns device names that can be used to create a new VideoCapturerAndroid.
  public static String[] getDeviceNames() {
    String[] names = new String[android.hardware.Camera.getNumberOfCameras()];
    for (int i = 0; i < android.hardware.Camera.getNumberOfCameras(); ++i) {
      names[i] = getDeviceName(i);
    }
    return names;
  }

  // Returns number of cameras on device.
  public static int getDeviceCount() {
    return android.hardware.Camera.getNumberOfCameras();
  }

  // Returns the name of the camera with camera index. Returns null if the
  // camera can not be used.
  public static String getDeviceName(int index) {
    android.hardware.Camera.CameraInfo info = new android.hardware.Camera.CameraInfo();
    try {
      android.hardware.Camera.getCameraInfo(index, info);
    } catch (Exception e) {
      Logging.e(TAG, "getCameraInfo failed on index " + index,e);
      return null;
    }

    String facing =
        (info.facing == android.hardware.Camera.CameraInfo.CAMERA_FACING_FRONT) ? "front" : "back";
    return "Camera " + index + ", Facing " + facing
        + ", Orientation " + info.orientation;
  }

  // Returns the name of the front facing camera. Returns null if the
  // camera can not be used or does not exist.
  public static String getNameOfFrontFacingDevice() {
    return getNameOfDevice(android.hardware.Camera.CameraInfo.CAMERA_FACING_FRONT);
  }

  // Returns the name of the back facing camera. Returns null if the
  // camera can not be used or does not exist.
  public static String getNameOfBackFacingDevice() {
    return getNameOfDevice(android.hardware.Camera.CameraInfo.CAMERA_FACING_BACK);
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

  public static int[] getFramerateRange(android.hardware.Camera.Parameters parameters,
      final int framerate) {
    List<int[]> listFpsRange = parameters.getSupportedPreviewFpsRange();
    if (listFpsRange.isEmpty()) {
      Logging.w(TAG, "No supported preview fps range");
      return new int[]{0, 0};
    }
    return Collections.min(listFpsRange,
        new ClosestComparator<int[]>() {
          @Override int diff(int[] range) {
            final int maxFpsWeight = 10;
            return range[android.hardware.Camera.Parameters.PREVIEW_FPS_MIN_INDEX]
                + maxFpsWeight * abs(framerate
                    - range[android.hardware.Camera.Parameters.PREVIEW_FPS_MAX_INDEX]);
          }
     });
  }

  public static android.hardware.Camera.Size getClosestSupportedSize(
      List<android.hardware.Camera.Size> supportedSizes, final int requestedWidth,
      final int requestedHeight) {
    return Collections.min(supportedSizes,
        new ClosestComparator<android.hardware.Camera.Size>() {
          @Override int diff(android.hardware.Camera.Size size) {
            return abs(requestedWidth - size.width) + abs(requestedHeight - size.height);
          }
     });
  }

  private static String getNameOfDevice(int facing) {
    final android.hardware.Camera.CameraInfo info = new android.hardware.Camera.CameraInfo();
    for (int i = 0; i < android.hardware.Camera.getNumberOfCameras(); ++i) {
      try {
        android.hardware.Camera.getCameraInfo(i, info);
        if (info.facing == facing) {
          return getDeviceName(i);
        }
      } catch (Exception e) {
        Logging.e(TAG, "getCameraInfo() failed on index " + i, e);
      }
    }
    return null;
  }
}
