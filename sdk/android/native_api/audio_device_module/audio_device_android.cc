/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/native_api/audio_device_module/audio_device_android.h"

#include <stdlib.h>
#include <utility>

#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "sdk/android/src/jni/audio_device/aaudio_player.h"
#include "sdk/android/src/jni/audio_device/aaudio_recorder.h"
#include "sdk/android/src/jni/audio_device/audio_manager.h"
#include "sdk/android/src/jni/audio_device/audio_record_jni.h"
#include "sdk/android/src/jni/audio_device/audio_track_jni.h"
#include "sdk/android/src/jni/audio_device/opensles_player.h"
#include "sdk/android/src/jni/audio_device/opensles_recorder.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

// This template function takes care of some boiler plate.
template <typename AudioInputT, typename AudioOutputT>
rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceModuleTemplate(
    AudioDeviceModule::AudioLayer audio_layer,
    JNIEnv* env,
    jobject application_context) {
  auto audio_manager = rtc::MakeUnique<android_adm::AudioManager>(
      env, audio_layer, JavaParamRef<jobject>(application_context));
  auto audio_input = rtc::MakeUnique<AudioInputT>(audio_manager.get());
  auto audio_output = rtc::MakeUnique<AudioOutputT>(audio_manager.get());
  return CreateAudioDeviceModuleFromInputAndOutput(
      audio_layer, std::move(audio_manager), std::move(audio_input),
      std::move(audio_output));
}

}  // namespace

#if defined(AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
rtc::scoped_refptr<AudioDeviceModule> CreateAAudioAudioDeviceModule(
    JNIEnv* env,
    jobject application_context) {
  RTC_LOG(INFO) << __FUNCTION__;
  return CreateAudioDeviceModuleTemplate<android_adm::AAudioRecorder,
                                         android_adm::AAudioPlayer>(
      AudioDeviceModule::kAndroidAAudioAudio, env, application_context);
}
#endif

rtc::scoped_refptr<AudioDeviceModule> CreateJavaAudioDeviceModule(
    JNIEnv* env,
    jobject application_context) {
  return CreateAudioDeviceModuleTemplate<android_adm::AudioRecordJni,
                                         android_adm::AudioTrackJni>(
      AudioDeviceModule::kAndroidJavaAudio, env, application_context);
}

rtc::scoped_refptr<AudioDeviceModule> CreateOpenSLESAudioDeviceModule(
    JNIEnv* env,
    jobject application_context) {
  return CreateAudioDeviceModuleTemplate<android_adm::OpenSLESRecorder,
                                         android_adm::OpenSLESPlayer>(
      AudioDeviceModule::kAndroidJavaAudio, env, application_context);
}

rtc::scoped_refptr<AudioDeviceModule>
CreateJavaInputAndOpenSLESOutputAudioDeviceModule(JNIEnv* env,
                                                  jobject application_context) {
  return CreateAudioDeviceModuleTemplate<android_adm::AudioRecordJni,
                                         android_adm::OpenSLESPlayer>(
      AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio, env,
      application_context);
}

}  // namespace webrtc
