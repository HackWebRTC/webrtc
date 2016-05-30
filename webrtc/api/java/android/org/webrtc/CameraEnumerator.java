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

import android.os.SystemClock;

import org.webrtc.CameraEnumerationAndroid.CaptureFormat;
import org.webrtc.Logging;

import java.util.ArrayList;
import java.util.List;

@SuppressWarnings("deprecation")
public class CameraEnumerator implements CameraEnumerationAndroid.Enumerator {
  private final static String TAG = "CameraEnumerator";
  // Each entry contains the supported formats for corresponding camera index. The formats for all
  // cameras are enumerated on the first call to getSupportedFormats(), and cached for future
  // reference.
  private List<List<CaptureFormat>> cachedSupportedFormats;

  @Override
  public List<CaptureFormat> getSupportedFormats(int cameraId) {
    synchronized (this) {
      if (cachedSupportedFormats == null) {
        cachedSupportedFormats = new ArrayList<List<CaptureFormat>>();
        for (int i = 0; i < CameraEnumerationAndroid.getDeviceCount(); ++i) {
          cachedSupportedFormats.add(enumerateFormats(i));
        }
      }
    }
    return cachedSupportedFormats.get(cameraId);
  }

  private List<CaptureFormat> enumerateFormats(int cameraId) {
    Logging.d(TAG, "Get supported formats for camera index " + cameraId + ".");
    final long startTimeMs = SystemClock.elapsedRealtime();
    final android.hardware.Camera.Parameters parameters;
    android.hardware.Camera camera = null;
    try {
      Logging.d(TAG, "Opening camera with index " + cameraId);
      camera = android.hardware.Camera.open(cameraId);
      parameters = camera.getParameters();
    } catch (RuntimeException e) {
      Logging.e(TAG, "Open camera failed on camera index " + cameraId, e);
      return new ArrayList<CaptureFormat>();
    } finally {
      if (camera != null) {
        camera.release();
      }
    }

    final List<CaptureFormat> formatList = new ArrayList<CaptureFormat>();
    try {
      int minFps = 0;
      int maxFps = 0;
      final List<int[]> listFpsRange = parameters.getSupportedPreviewFpsRange();
      if (listFpsRange != null) {
        // getSupportedPreviewFpsRange() returns a sorted list. Take the fps range
        // corresponding to the highest fps.
        final int[] range = listFpsRange.get(listFpsRange.size() - 1);
        minFps = range[android.hardware.Camera.Parameters.PREVIEW_FPS_MIN_INDEX];
        maxFps = range[android.hardware.Camera.Parameters.PREVIEW_FPS_MAX_INDEX];
      }
      for (android.hardware.Camera.Size size : parameters.getSupportedPreviewSizes()) {
        formatList.add(new CaptureFormat(size.width, size.height, minFps, maxFps));
      }
    } catch (Exception e) {
      Logging.e(TAG, "getSupportedFormats() failed on camera index " + cameraId, e);
    }

    final long endTimeMs = SystemClock.elapsedRealtime();
    Logging.d(TAG, "Get supported formats for camera index " + cameraId + " done."
        + " Time spent: " + (endTimeMs - startTimeMs) + " ms.");
    return formatList;
  }
}
