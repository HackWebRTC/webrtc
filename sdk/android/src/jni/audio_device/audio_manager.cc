/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/audio_device/audio_manager.h"

#include <utility>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"

#include "sdk/android/generated_audio_device_base_jni/jni/WebRtcAudioManager_jni.h"
#include "sdk/android/src/jni/audio_device/audio_common.h"
#include "sdk/android/src/jni/audio_device/audio_device_module.h"
#include "sdk/android/src/jni/audio_device/audio_manager.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {

namespace android_adm {

// AudioManager implementation
AudioManager::AudioManager(JNIEnv* env,
                           AudioDeviceModule::AudioLayer audio_layer,
                           const JavaParamRef<jobject>& application_context)
    : j_audio_manager_(
          Java_WebRtcAudioManager_Constructor(env, application_context)),
      audio_layer_(audio_layer),
      initialized_(false) {
  RTC_LOG(INFO) << "ctor";
  const int sample_rate =
      Java_WebRtcAudioManager_getSampleRate(env, j_audio_manager_);
  const size_t output_channels =
      Java_WebRtcAudioManager_getStereoOutput(env, j_audio_manager_) ? 2 : 1;
  const size_t input_channels =
      Java_WebRtcAudioManager_getStereoInput(env, j_audio_manager_) ? 2 : 1;
  const size_t output_buffer_size =
      Java_WebRtcAudioManager_getOutputBufferSize(env, j_audio_manager_);
  const size_t input_buffer_size =
      Java_WebRtcAudioManager_getInputBufferSize(env, j_audio_manager_);
  playout_parameters_.reset(sample_rate, static_cast<size_t>(output_channels),
                            static_cast<size_t>(output_buffer_size));
  record_parameters_.reset(sample_rate, static_cast<size_t>(input_channels),
                           static_cast<size_t>(input_buffer_size));
  RTC_CHECK(playout_parameters_.is_valid());
  RTC_CHECK(record_parameters_.is_valid());
  thread_checker_.DetachFromThread();
}

AudioManager::~AudioManager() {
  RTC_LOG(INFO) << "dtor";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  Close();
}

bool AudioManager::Init() {
  RTC_LOG(INFO) << "Init";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(!initialized_);
  RTC_DCHECK_NE(audio_layer_, AudioDeviceModule::kPlatformDefaultAudio);
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  if (!Java_WebRtcAudioManager_init(env, j_audio_manager_)) {
    RTC_LOG(LS_ERROR) << "Init() failed";
    return false;
  }
  initialized_ = true;
  return true;
}

bool AudioManager::Close() {
  RTC_LOG(INFO) << "Close";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_)
    return true;
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_WebRtcAudioManager_dispose(env, j_audio_manager_);
  initialized_ = false;
  return true;
}

bool AudioManager::IsAcousticEchoCancelerSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  return Java_WebRtcAudioManager_isAcousticEchoCancelerSupported(
      env, j_audio_manager_);
}

bool AudioManager::IsNoiseSuppressorSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  return Java_WebRtcAudioManager_isNoiseSuppressorSupported(env,
                                                            j_audio_manager_);
}

bool AudioManager::IsStereoPlayoutSupported() const {
  return (playout_parameters_.channels() == 2);
}

bool AudioManager::IsStereoRecordSupported() const {
  return (record_parameters_.channels() == 2);
}

int AudioManager::GetDelayEstimateInMilliseconds() const {
  return audio_layer_ == AudioDeviceModule::kAndroidJavaAudio
             ? kHighLatencyModeDelayEstimateInMilliseconds
             : kLowLatencyModeDelayEstimateInMilliseconds;
}

const AudioParameters& AudioManager::GetPlayoutAudioParameters() {
  RTC_CHECK(playout_parameters_.is_valid());
  return playout_parameters_;
}

const AudioParameters& AudioManager::GetRecordAudioParameters() {
  RTC_CHECK(record_parameters_.is_valid());
  return record_parameters_;
}

}  // namespace android_adm

}  // namespace webrtc
