/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.voiceengine;

import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.AudioEffect;
import android.media.audiofx.AudioEffect.Descriptor;
import android.media.AudioManager;
import android.os.Build;
import android.util.Log;

import java.lang.Thread;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class WebRtcAudioUtils {
  // List of devices where it has been verified that the built-in AEC performs
  // bad and where it makes sense to avoid using it and instead rely on the
  // native WebRTC AEC instead. The device name is given by Build.MODEL.
  private static final String[] BLACKLISTED_AEC_MODELS = new String[] {
      "Nexus 5", // Nexus 5
      "D6503",   // Sony Xperia Z2 D6503
  };

  // Use 44.1kHz as the default sampling rate.
  private static final int SAMPLE_RATE_HZ = 44100;

  public static boolean runningOnGingerBreadOrHigher() {
    // November 2010: Android 2.3, API Level 9.
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD;
  }

  public static boolean runningOnJellyBeanOrHigher() {
    // June 2012: Android 4.1. API Level 16.
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN;
  }

  public static boolean runningOnJellyBeanMR1OrHigher() {
    // November 2012: Android 4.2. API Level 17.
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1;
  }

  public static boolean runningOnLollipopOrHigher() {
    // API Level 21.
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
  }

  // Helper method for building a string of thread information.
  public static String getThreadInfo() {
    return "@[name=" + Thread.currentThread().getName()
        + ", id=" + Thread.currentThread().getId() + "]";
  }

  // Returns true if the device is blacklisted for HW AEC usage.
  public static boolean deviceIsBlacklistedForHwAecUsage() {
    List<String> blackListedModels = Arrays.asList(BLACKLISTED_AEC_MODELS);
    return blackListedModels.contains(Build.MODEL);
  }

  // Returns true if the device supports Acoustic Echo Canceler (AEC).
  public static boolean isAcousticEchoCancelerSupported() {
    // AcousticEchoCanceler was added in API level 16 (Jelly Bean).
    if (!WebRtcAudioUtils.runningOnJellyBeanOrHigher()) {
      return false;
    }
    // Check if the device implements acoustic echo cancellation.
    return AcousticEchoCanceler.isAvailable();
  }

  // Returns true if the device supports AEC and it not blacklisted.
  public static boolean isAcousticEchoCancelerApproved() {
    if (deviceIsBlacklistedForHwAecUsage())
      return false;
    return isAcousticEchoCancelerSupported();
  }

  // Information about the current build, taken from system properties.
  public static void logDeviceInfo(String tag) {
    Log.d(tag, "Android SDK: " + Build.VERSION.SDK_INT + ", "
        + "Release: " + Build.VERSION.RELEASE + ", "
        + "Brand: " + Build.BRAND + ", "
        + "Device: " + Build.DEVICE + ", "
        + "Id: " + Build.ID + ", "
        + "Hardware: " + Build.HARDWARE + ", "
        + "Manufacturer: " + Build.MANUFACTURER + ", "
        + "Model: " + Build.MODEL + ", "
        + "Product: " + Build.PRODUCT);
  }
}
