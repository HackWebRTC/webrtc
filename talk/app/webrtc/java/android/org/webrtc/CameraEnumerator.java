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

import android.hardware.Camera;
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
    final Camera.Parameters parameters;
    Camera camera = null;
    try {
      Logging.d(TAG, "Opening camera with index " + cameraId);
      camera = Camera.open(cameraId);
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
        minFps = range[Camera.Parameters.PREVIEW_FPS_MIN_INDEX];
        maxFps = range[Camera.Parameters.PREVIEW_FPS_MAX_INDEX];
      }
      for (Camera.Size size : parameters.getSupportedPreviewSizes()) {
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
