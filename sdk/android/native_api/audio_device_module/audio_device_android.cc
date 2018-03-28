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

#if defined(AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
rtc::scoped_refptr<AudioDeviceModule> CreateAAudioAudioDeviceModule(
    JNIEnv* env,
    jobject application_context) {
  RTC_LOG(INFO) << __FUNCTION__;
  const AudioDeviceModule::AudioLayer audio_layer =
      AudioDeviceModule::kAndroidAAudioAudio;
  auto audio_manager = rtc::MakeUnique<android_adm::AudioManager>(
      env, audio_layer, JavaParamRef<jobject>(application_context));
  auto audio_input =
      rtc::MakeUnique<android_adm::AAudioRecorder>(audio_manager.get());
  auto audio_output =
      rtc::MakeUnique<android_adm::AAudioPlayer>(audio_manager.get());
  return CreateAudioDeviceModuleFromInputAndOutput(
      audio_layer, std::move(audio_manager), std::move(audio_input),
      std::move(audio_output));
}
#endif

rtc::scoped_refptr<AudioDeviceModule> CreateJavaAudioDeviceModule(
    JNIEnv* env,
    jobject application_context) {
  const AudioDeviceModule::AudioLayer audio_layer =
      AudioDeviceModule::kAndroidJavaAudio;
  auto audio_manager = rtc::MakeUnique<android_adm::AudioManager>(
      env, audio_layer, JavaParamRef<jobject>(application_context));
  auto audio_input =
      rtc::MakeUnique<android_adm::AudioRecordJni>(audio_manager.get());
  auto audio_output =
      rtc::MakeUnique<android_adm::AudioTrackJni>(audio_manager.get());
  return CreateAudioDeviceModuleFromInputAndOutput(
      audio_layer, std::move(audio_manager), std::move(audio_input),
      std::move(audio_output));
}

rtc::scoped_refptr<AudioDeviceModule> CreateOpenSLESAudioDeviceModule(
    JNIEnv* env,
    jobject application_context) {
  const AudioDeviceModule::AudioLayer audio_layer =
      AudioDeviceModule::kAndroidOpenSLESAudio;
  auto engine_manager = rtc::MakeUnique<android_adm::OpenSLEngineManager>();
  auto audio_manager = rtc::MakeUnique<android_adm::AudioManager>(
      env, audio_layer, JavaParamRef<jobject>(application_context));
  auto audio_input = rtc::MakeUnique<android_adm::OpenSLESRecorder>(
      audio_manager.get(), engine_manager.get());
  auto audio_output = rtc::MakeUnique<android_adm::OpenSLESPlayer>(
      audio_manager.get(), std::move(engine_manager));
  return CreateAudioDeviceModuleFromInputAndOutput(
      audio_layer, std::move(audio_manager), std::move(audio_input),
      std::move(audio_output));
}

rtc::scoped_refptr<AudioDeviceModule>
CreateJavaInputAndOpenSLESOutputAudioDeviceModule(JNIEnv* env,
                                                  jobject application_context) {
  const AudioDeviceModule::AudioLayer audio_layer =
      AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio;
  auto audio_manager = rtc::MakeUnique<android_adm::AudioManager>(
      env, audio_layer, JavaParamRef<jobject>(application_context));
  auto audio_input =
      rtc::MakeUnique<android_adm::AudioRecordJni>(audio_manager.get());
  auto audio_output = rtc::MakeUnique<android_adm::OpenSLESPlayer>(
      audio_manager.get(), rtc::MakeUnique<android_adm::OpenSLEngineManager>());
  return CreateAudioDeviceModuleFromInputAndOutput(
      audio_layer, std::move(audio_manager), std::move(audio_input),
      std::move(audio_output));
}

}  // namespace webrtc
