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
import android.graphics.ImageFormat;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.os.Build;
import android.os.SystemClock;
import android.util.Range;
import android.util.Size;

import org.webrtc.CameraEnumerationAndroid.CaptureFormat;
import org.webrtc.Logging;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class Camera2Enumerator implements CameraEnumerationAndroid.Enumerator {
  private final static String TAG = "Camera2Enumerator";
  private final static double NANO_SECONDS_PER_SECOND = 1.0e9;

  private final CameraManager cameraManager;
  // Each entry contains the supported formats for a given camera index. The formats are enumerated
  // lazily in getSupportedFormats(), and cached for future reference.
  private final Map<Integer, List<CaptureFormat>> cachedSupportedFormats =
      new HashMap<Integer, List<CaptureFormat>>();

  public static boolean isSupported() {
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
  }

  public Camera2Enumerator(Context context) {
    cameraManager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
  }

  @Override
  public List<CaptureFormat> getSupportedFormats(int cameraId) {
    synchronized (cachedSupportedFormats) {
      if (cachedSupportedFormats.containsKey(cameraId)) {
        return cachedSupportedFormats.get(cameraId);
      }
      Logging.d(TAG, "Get supported formats for camera index " + cameraId + ".");
      final long startTimeMs = SystemClock.elapsedRealtime();

      final CameraCharacteristics cameraCharacteristics;
      try {
        cameraCharacteristics = cameraManager.getCameraCharacteristics(Integer.toString(cameraId));
      } catch (Exception ex) {
        Logging.e(TAG, "getCameraCharacteristics(): " + ex);
        return new ArrayList<CaptureFormat>();
      }

      // Calculate default max fps from auto-exposure ranges in case getOutputMinFrameDuration() is
      // not supported.
      final Range<Integer>[] fpsRanges =
          cameraCharacteristics.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
      int defaultMaxFps = 0;
      for (Range<Integer> fpsRange : fpsRanges) {
        defaultMaxFps = Math.max(defaultMaxFps, fpsRange.getUpper());
      }

      final StreamConfigurationMap streamMap =
          cameraCharacteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
      final Size[] sizes = streamMap.getOutputSizes(ImageFormat.YUV_420_888);
      if (sizes == null) {
        throw new RuntimeException("ImageFormat.YUV_420_888 not supported.");
      }

      final List<CaptureFormat> formatList = new ArrayList<CaptureFormat>();
      for (Size size : sizes) {
        long minFrameDurationNs = 0;
        try {
          minFrameDurationNs = streamMap.getOutputMinFrameDuration(ImageFormat.YUV_420_888, size);
        } catch (Exception e) {
          // getOutputMinFrameDuration() is not supported on all devices. Ignore silently.
        }
        final int maxFps = (minFrameDurationNs == 0)
                               ? defaultMaxFps
                               : (int) Math.round(NANO_SECONDS_PER_SECOND / minFrameDurationNs);
        formatList.add(new CaptureFormat(size.getWidth(), size.getHeight(), 0, maxFps * 1000));
      }
      cachedSupportedFormats.put(cameraId, formatList);
      final long endTimeMs = SystemClock.elapsedRealtime();
      Logging.d(TAG, "Get supported formats for camera index " + cameraId + " done."
          + " Time spent: " + (endTimeMs - startTimeMs) + " ms.");
      return formatList;
    }
  }
}
