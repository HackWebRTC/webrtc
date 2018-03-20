/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/audio_device/build_info.h"

#include "sdk/android/generated_audio_jni/jni/BuildInfo_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {

namespace android_adm {

BuildInfo::BuildInfo() : env_(AttachCurrentThreadIfNeeded()) {}

std::string BuildInfo::GetDeviceModel() {
  thread_checker_.CalledOnValidThread();
  return JavaToStdString(env_, Java_BuildInfo_getDeviceModel(env_));
}

std::string BuildInfo::GetBrand() {
  thread_checker_.CalledOnValidThread();
  return JavaToStdString(env_, Java_BuildInfo_getBrand(env_));
}

std::string BuildInfo::GetDeviceManufacturer() {
  thread_checker_.CalledOnValidThread();
  return JavaToStdString(env_, Java_BuildInfo_getDeviceManufacturer(env_));
}

std::string BuildInfo::GetAndroidBuildId() {
  thread_checker_.CalledOnValidThread();
  return JavaToStdString(env_, Java_BuildInfo_getAndroidBuildId(env_));
}

std::string BuildInfo::GetBuildType() {
  thread_checker_.CalledOnValidThread();
  return JavaToStdString(env_, Java_BuildInfo_getBuildType(env_));
}

std::string BuildInfo::GetBuildRelease() {
  thread_checker_.CalledOnValidThread();
  return JavaToStdString(env_, Java_BuildInfo_getBuildRelease(env_));
}

SdkCode BuildInfo::GetSdkVersion() {
  thread_checker_.CalledOnValidThread();
  return static_cast<SdkCode>(Java_BuildInfo_getSdkVersion(env_));
}

}  // namespace android_adm

}  // namespace webrtc
