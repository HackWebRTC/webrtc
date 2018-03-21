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

#include "sdk/android/generated_audio_jni/jni/WebRtcAudioManager_jni.h"
#include "sdk/android/src/jni/audio_device/audio_common.h"
#include "sdk/android/src/jni/jni_helpers.h"

#if defined(AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
#include "sdk/android/src/jni/audio_device/aaudio_player.h"
#include "sdk/android/src/jni/audio_device/aaudio_recorder.h"
#endif
#include "sdk/android/src/jni/audio_device/audio_device_template_android.h"
#include "sdk/android/src/jni/audio_device/audio_manager.h"
#include "sdk/android/src/jni/audio_device/audio_record_jni.h"
#include "sdk/android/src/jni/audio_device/audio_track_jni.h"
#include "sdk/android/src/jni/audio_device/opensles_player.h"
#include "sdk/android/src/jni/audio_device/opensles_recorder.h"

namespace webrtc {

namespace android_adm {

#if defined(AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
rtc::scoped_refptr<AudioDeviceModule>
AudioManager::CreateAAudioAudioDeviceModule(
    JNIEnv* env,
    const JavaParamRef<jobject>& application_context) {
  RTC_LOG(INFO) << __FUNCTION__;
  return new rtc::RefCountedObject<android_adm::AudioDeviceTemplateAndroid<
      android_adm::AAudioRecorder, android_adm::AAudioPlayer>>(
      env, AudioDeviceModule::kAndroidAAudioAudio);
}
#endif

rtc::scoped_refptr<AudioDeviceModule> AudioManager::CreateAudioDeviceModule(
    JNIEnv* env,
    const JavaParamRef<jobject>& application_context) {
  const bool use_opensles_output =
      !Java_WebRtcAudioManager_isDeviceBlacklistedForOpenSLESUsage(env) &&
      Java_WebRtcAudioManager_isLowLatencyOutputSupported(env,
                                                          application_context);
  const bool use_opensles_input =
      use_opensles_output && Java_WebRtcAudioManager_isLowLatencyInputSupported(
                                 env, application_context);
  return CreateAudioDeviceModule(env, application_context, use_opensles_input,
                                 use_opensles_output);
}

rtc::scoped_refptr<AudioDeviceModule> AudioManager::CreateAudioDeviceModule(
    JNIEnv* env,
    const JavaParamRef<jobject>& application_context,
    bool use_opensles_input,
    bool use_opensles_output) {
  RTC_LOG(INFO) << __FUNCTION__;

  if (use_opensles_output) {
    if (use_opensles_input) {
      // Use OpenSL ES for both playout and recording.
      return new rtc::RefCountedObject<android_adm::AudioDeviceTemplateAndroid<
          android_adm::OpenSLESRecorder, android_adm::OpenSLESPlayer>>(
          env, application_context, AudioDeviceModule::kAndroidOpenSLESAudio);
    } else {
      // Use OpenSL ES for output and AudioRecord API for input. This
      // combination provides low-latency output audio and at the same
      // time support for HW AEC using the AudioRecord Java API.
      return new rtc::RefCountedObject<android_adm::AudioDeviceTemplateAndroid<
          android_adm::AudioRecordJni, android_adm::OpenSLESPlayer>>(
          env, application_context,
          AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio);
    }
  } else {
    RTC_DCHECK(!use_opensles_input)
        << "Combination of OpenSLES input and Java-based output not supported";
    // Use Java-based audio in both directions.
    return new rtc::RefCountedObject<android_adm::AudioDeviceTemplateAndroid<
        android_adm::AudioRecordJni, android_adm::AudioTrackJni>>(
        env, application_context, AudioDeviceModule::kAndroidJavaAudio);
  }
}

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
  thread_checker_.DetachFromThread();
}

AudioManager::~AudioManager() {
  RTC_LOG(INFO) << "dtor";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  Close();
}

SLObjectItf AudioManager::GetOpenSLEngine() {
  RTC_LOG(INFO) << "GetOpenSLEngine";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // Only allow usage of OpenSL ES if such an audio layer has been specified.
  if (audio_layer_ != AudioDeviceModule::kAndroidOpenSLESAudio &&
      audio_layer_ !=
          AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio) {
    RTC_LOG(INFO)
        << "Unable to create OpenSL engine for the current audio layer: "
        << audio_layer_;
    return nullptr;
  }
  // OpenSL ES for Android only supports a single engine per application.
  // If one already has been created, return existing object instead of
  // creating a new.
  if (engine_object_.Get() != nullptr) {
    RTC_LOG(WARNING) << "The OpenSL ES engine object has already been created";
    return engine_object_.Get();
  }
  // Create the engine object in thread safe mode.
  const SLEngineOption option[] = {
      {SL_ENGINEOPTION_THREADSAFE, static_cast<SLuint32>(SL_BOOLEAN_TRUE)}};
  SLresult result =
      slCreateEngine(engine_object_.Receive(), 1, option, 0, NULL, NULL);
  if (result != SL_RESULT_SUCCESS) {
    RTC_LOG(LS_ERROR) << "slCreateEngine() failed: "
                      << GetSLErrorString(result);
    engine_object_.Reset();
    return nullptr;
  }
  // Realize the SL Engine in synchronous mode.
  result = engine_object_->Realize(engine_object_.Get(), SL_BOOLEAN_FALSE);
  if (result != SL_RESULT_SUCCESS) {
    RTC_LOG(LS_ERROR) << "Realize() failed: " << GetSLErrorString(result);
    engine_object_.Reset();
    return nullptr;
  }
  // Finally return the SLObjectItf interface of the engine object.
  return engine_object_.Get();
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

bool AudioManager::IsCommunicationModeEnabled() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  return Java_WebRtcAudioManager_isCommunicationModeEnabled(env,
                                                            j_audio_manager_);
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
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return (playout_parameters_.channels() == 2);
}

bool AudioManager::IsStereoRecordSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return (record_parameters_.channels() == 2);
}

int AudioManager::GetDelayEstimateInMilliseconds() const {
  return audio_layer_ == AudioDeviceModule::kAndroidJavaAudio
             ? kHighLatencyModeDelayEstimateInMilliseconds
             : kLowLatencyModeDelayEstimateInMilliseconds;
}

const AudioParameters& AudioManager::GetPlayoutAudioParameters() {
  RTC_CHECK(playout_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return playout_parameters_;
}

const AudioParameters& AudioManager::GetRecordAudioParameters() {
  RTC_CHECK(record_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return record_parameters_;
}

}  // namespace android_adm

}  // namespace webrtc
