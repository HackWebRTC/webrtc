/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_BUILD_INFO_H_
#define SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_BUILD_INFO_H_

#include <jni.h>
#include <memory>
#include <string>

#include "rtc_base/thread_checker.h"

namespace webrtc {

namespace android_adm {

// This enumeration maps to the values returned by BuildInfo::GetSdkVersion(),
// indicating the Android release associated with a given SDK version.
// See https://developer.android.com/guide/topics/manifest/uses-sdk-element.html
// for details.
enum SdkCode {
  SDK_CODE_JELLY_BEAN = 16,      // Android 4.1
  SDK_CODE_JELLY_BEAN_MR1 = 17,  // Android 4.2
  SDK_CODE_JELLY_BEAN_MR2 = 18,  // Android 4.3
  SDK_CODE_KITKAT = 19,          // Android 4.4
  SDK_CODE_WATCH = 20,           // Android 4.4W
  SDK_CODE_LOLLIPOP = 21,        // Android 5.0
  SDK_CODE_LOLLIPOP_MR1 = 22,    // Android 5.1
  SDK_CODE_MARSHMALLOW = 23,     // Android 6.0
  SDK_CODE_N = 24,
};

// Utility class used to query the Java class (org/webrtc/audio/BuildInfo)
// for device and Android build information.
// The calling thread is attached to the JVM at construction if needed and a
// valid Java environment object is also created.
// All Get methods must be called on the creating thread. If not, the code will
// hit RTC_DCHECKs when calling JNIEnvironment::JavaToStdString().
class BuildInfo {
 public:
  BuildInfo();
  ~BuildInfo() {}

  // End-user-visible name for the end product (e.g. "Nexus 6").
  std::string GetDeviceModel();
  // Consumer-visible brand (e.g. "google").
  std::string GetBrand();
  // Manufacturer of the product/hardware (e.g. "motorola").
  std::string GetDeviceManufacturer();
  // Android build ID (e.g. LMY47D).
  std::string GetAndroidBuildId();
  // The type of build (e.g. "user" or "eng").
  std::string GetBuildType();
  // The user-visible version string (e.g. "5.1").
  std::string GetBuildRelease();
  // The user-visible SDK version of the framework (e.g. 21). See SdkCode enum
  // for translation.
  SdkCode GetSdkVersion();

 private:
  JNIEnv* const env_;
  rtc::ThreadChecker thread_checker_;
};

}  // namespace android_adm

}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_BUILD_INFO_H_
