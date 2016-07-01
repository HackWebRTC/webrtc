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

import org.webrtc.CameraEnumerationAndroid.CaptureFormat;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.os.Build;
import android.os.SystemClock;
import android.util.Range;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

@TargetApi(21)
public class Camera2Enumerator implements CameraEnumerator {
  private final static String TAG = "Camera2Enumerator";
  private final static double NANO_SECONDS_PER_SECOND = 1.0e9;

  // Each entry contains the supported formats for a given camera index. The formats are enumerated
  // lazily in getSupportedFormats(), and cached for future reference.
  private static final Map<String, List<CaptureFormat>> cachedSupportedFormats =
      new HashMap<String, List<CaptureFormat>>();

  final Context context;
  final CameraManager cameraManager;

  public Camera2Enumerator(Context context) {
    this.context = context;
    this.cameraManager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
  }

  @Override
  public String[] getDeviceNames() {
    try {
      return cameraManager.getCameraIdList();
    } catch (CameraAccessException e) {
      Logging.e(TAG, "Camera access exception: " + e);
      return new String[] {};
    }
  }

  @Override
  public boolean isFrontFacing(String deviceName) {
    CameraCharacteristics characteristics
        = getCameraCharacteristics(deviceName);

    return characteristics != null
        && characteristics.get(CameraCharacteristics.LENS_FACING)
            == CameraMetadata.LENS_FACING_FRONT;
  }

  @Override
  public boolean isBackFacing(String deviceName) {
    CameraCharacteristics characteristics
        = getCameraCharacteristics(deviceName);

    return characteristics != null
        && characteristics.get(CameraCharacteristics.LENS_FACING)
            == CameraMetadata.LENS_FACING_BACK;
  }

  @Override
  public CameraVideoCapturer createCapturer(String deviceName,
      CameraVideoCapturer.CameraEventsHandler eventsHandler) {
    return new Camera2Capturer(context, deviceName, eventsHandler);
  }

  private CameraCharacteristics getCameraCharacteristics(String deviceName) {
    try {
      return cameraManager.getCameraCharacteristics(deviceName);
    } catch (CameraAccessException e) {
      Logging.e(TAG, "Camera access exception: " + e);
      return null;
    }
  }

  public static boolean isSupported() {
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
  }

  static List<CaptureFormat.FramerateRange> getSupportedFramerateRanges(
      CameraCharacteristics cameraCharacteristics) {
    final Range<Integer>[] fpsRanges =
        cameraCharacteristics.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);

    if (fpsRanges == null) {
      return new ArrayList<CaptureFormat.FramerateRange>();
    }

    int maxFps = 0;
    for (Range<Integer> fpsRange : fpsRanges) {
      maxFps = Math.max(maxFps, fpsRange.getUpper());
    }
    int unitFactor = maxFps < 1000 ? 1000 : 1;
    return convertFramerates(fpsRanges, unitFactor);
  }

  static List<Size> getSupportedSizes(
      CameraCharacteristics cameraCharacteristics) {
    final StreamConfigurationMap streamMap =
          cameraCharacteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
    final android.util.Size[] sizes = streamMap.getOutputSizes(SurfaceTexture.class);
    if (sizes == null) {
      Logging.e(TAG, "No supported camera output sizes.");
      return new ArrayList<Size>();
    }
    return convertSizes(sizes);
  }

  static List<CaptureFormat> getSupportedFormats(Context context, String cameraId) {
    return getSupportedFormats(
        (CameraManager) context.getSystemService(Context.CAMERA_SERVICE), cameraId);
  }

  static List<CaptureFormat> getSupportedFormats(
      CameraManager cameraManager, String cameraId) {
    synchronized (cachedSupportedFormats) {
      if (cachedSupportedFormats.containsKey(cameraId)) {
        return cachedSupportedFormats.get(cameraId);
      }

      Logging.d(TAG, "Get supported formats for camera index " + cameraId + ".");
      final long startTimeMs = SystemClock.elapsedRealtime();

      final CameraCharacteristics cameraCharacteristics;
      try {
        cameraCharacteristics = cameraManager.getCameraCharacteristics(cameraId);
      } catch (Exception ex) {
        Logging.e(TAG, "getCameraCharacteristics(): " + ex);
        return new ArrayList<CaptureFormat>();
      }

      final StreamConfigurationMap streamMap =
          cameraCharacteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);

      List<CaptureFormat.FramerateRange> framerateRanges = getSupportedFramerateRanges(
          cameraCharacteristics);
      List<Size> sizes = getSupportedSizes(cameraCharacteristics);

      int defaultMaxFps = 0;
      for (CaptureFormat.FramerateRange framerateRange : framerateRanges) {
        defaultMaxFps = Math.max(defaultMaxFps, framerateRange.max);
      }

      final List<CaptureFormat> formatList = new ArrayList<CaptureFormat>();
      for (Size size : sizes) {
        long minFrameDurationNs = 0;
        try {
          minFrameDurationNs = streamMap.getOutputMinFrameDuration(SurfaceTexture.class,
              new android.util.Size(size.width, size.height));
        } catch (Exception e) {
          // getOutputMinFrameDuration() is not supported on all devices. Ignore silently.
        }
        final int maxFps = (minFrameDurationNs == 0)
            ? defaultMaxFps
            : (int) Math.round(NANO_SECONDS_PER_SECOND / minFrameDurationNs) * 1000;
        formatList.add(new CaptureFormat(size.width, size.height, 0, maxFps));
        Logging.d(TAG, "Format: " + size.width + "x" + size.height + "@" + maxFps);
      }

      cachedSupportedFormats.put(cameraId, formatList);
      final long endTimeMs = SystemClock.elapsedRealtime();
      Logging.d(TAG, "Get supported formats for camera index " + cameraId + " done."
          + " Time spent: " + (endTimeMs - startTimeMs) + " ms.");
      return formatList;
    }
  }

  // Convert from android.util.Size to Size.
  private static List<Size> convertSizes(android.util.Size[] cameraSizes) {
    final List<Size> sizes = new ArrayList<Size>();
    for (android.util.Size size : cameraSizes) {
      sizes.add(new Size(size.getWidth(), size.getHeight()));
    }
    return sizes;
  }

  // Convert from android.util.Range<Integer> to CaptureFormat.FramerateRange.
  private static List<CaptureFormat.FramerateRange> convertFramerates(
      Range<Integer>[] arrayRanges, int unitFactor) {
    final List<CaptureFormat.FramerateRange> ranges = new ArrayList<CaptureFormat.FramerateRange>();
    for (Range<Integer> range : arrayRanges) {
      ranges.add(new CaptureFormat.FramerateRange(
          range.getLower() * unitFactor,
          range.getUpper() * unitFactor));
    }
    return ranges;
  }
}
