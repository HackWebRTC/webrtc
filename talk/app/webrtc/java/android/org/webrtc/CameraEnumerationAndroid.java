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

import android.hardware.Camera;
import android.util.Log;
import android.graphics.ImageFormat;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

@SuppressWarnings("deprecation")
public class CameraEnumerationAndroid {
  private final static String TAG = "CameraEnumerationAndroid";
  // List of formats supported by all cameras. This list is filled once in order
  // to be able to switch cameras.
  public static List<List<CaptureFormat>> supportedFormats;

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

  public static boolean initStatics() {
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

  public static String getSupportedFormatsAsJson(int id) throws JSONException {
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
  public static ArrayList<CaptureFormat> getSupportedFormats(int id) {
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

  // Helper class for finding the closest supported format for the two functions below.
  private static abstract class ClosestComparator<T> implements Comparator<T> {
    // Difference between supported and requested parameter.
    abstract int diff(T supportedParameter);

    @Override
    public int compare(T t1, T t2) {
      return diff(t1) - diff(t2);
    }
  }

  public static int[] getFramerateRange(Camera.Parameters parameters, final int framerate) {
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

  public static Camera.Size getClosestSupportedSize(
      List<Camera.Size> supportedSizes, final int requestedWidth, final int requestedHeight) {
    return Collections.min(supportedSizes,
        new ClosestComparator<Camera.Size>() {
          @Override int diff(Camera.Size size) {
            return abs(requestedWidth - size.width) + abs(requestedHeight - size.height);
          }
     });
  }
}
