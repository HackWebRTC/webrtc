/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_AUDIO_MANAGER_H_
#define SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_AUDIO_MANAGER_H_

#include <SLES/OpenSLES.h>
#include <jni.h>
#include <memory>

#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "rtc_base/thread_checker.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"
#include "sdk/android/src/jni/audio_device/audio_common.h"
#include "sdk/android/src/jni/audio_device/opensles_common.h"

namespace webrtc {

namespace android_adm {

// Implements support for functions in the WebRTC audio stack for Android that
// relies on the AudioManager in android.media. It also populates an
// AudioParameter structure with native audio parameters detected at
// construction. This class does not make any audio-related modifications
// unless Init() is called.
class AudioManager {
 public:
  AudioManager(JNIEnv* env,
               AudioDeviceModule::AudioLayer audio_layer,
               const JavaParamRef<jobject>& application_context);
  ~AudioManager();

  // Initializes the audio manager and stores the current audio mode.
  bool Init();
  // Revert any setting done by Init().
  bool Close();

  // Returns true if current audio mode is AudioManager.MODE_IN_COMMUNICATION.
  bool IsCommunicationModeEnabled() const;

  // Native audio parameters stored during construction.
  const AudioParameters& GetPlayoutAudioParameters();
  const AudioParameters& GetRecordAudioParameters();

  // Returns true if the device supports built-in audio effects for AEC, AGC
  // and NS. Some devices can also be blacklisted for use in combination with
  // platform effects and these devices will return false.
  // Can currently only be used in combination with a Java based audio backend
  // for the recoring side (i.e. using the android.media.AudioRecord API).
  bool IsAcousticEchoCancelerSupported() const;
  bool IsNoiseSuppressorSupported() const;

  // Returns true if the device supports (and has been configured for) stereo.
  // Call the Java API WebRtcAudioManager.setStereoOutput/Input() with true as
  // paramter to enable stereo. Default is mono in both directions and the
  // setting is set once and for all when the audio manager object is created.
  // TODO(henrika): stereo is not supported in combination with OpenSL ES.
  bool IsStereoPlayoutSupported() const;
  bool IsStereoRecordSupported() const;

  // Returns the estimated total delay of this device. Unit is in milliseconds.
  // The vaule is set once at construction and never changes after that.
  // Possible values are webrtc::kLowLatencyModeDelayEstimateInMilliseconds and
  // webrtc::kHighLatencyModeDelayEstimateInMilliseconds.
  int GetDelayEstimateInMilliseconds() const;

 private:
  // This class is single threaded except that construction might happen on a
  // different thread.
  rtc::ThreadChecker thread_checker_;

  // Wraps the Java specific parts of the AudioManager.
  ScopedJavaGlobalRef<jobject> j_audio_manager_;

  // Contains the selected audio layer specified by the AudioLayer enumerator
  // in the AudioDeviceModule class.
  const AudioDeviceModule::AudioLayer audio_layer_;

  // Set to true by Init() and false by Close().
  bool initialized_;

  // Contains native parameters (e.g. sample rate, channel configuration). Set
  // at construction.
  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;
};

}  // namespace android_adm

}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_AUDIO_MANAGER_H_
