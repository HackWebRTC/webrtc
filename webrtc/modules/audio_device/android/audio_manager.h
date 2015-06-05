/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_MANAGER_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_MANAGER_H_

#include <jni.h>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/audio_device/android/audio_common.h"
#include "webrtc/modules/audio_device/include/audio_device_defines.h"
#include "webrtc/modules/audio_device/audio_device_generic.h"
#include "webrtc/modules/utility/interface/helpers_android.h"
#include "webrtc/modules/utility/interface/jvm_android.h"

namespace webrtc {

class AudioParameters {
 public:
  enum { kBitsPerSample = 16 };
  AudioParameters()
      : sample_rate_(0),
        channels_(0),
        frames_per_buffer_(0),
        frames_per_10ms_buffer_(0),
        bits_per_sample_(kBitsPerSample) {}
  AudioParameters(int sample_rate, int channels, int frames_per_buffer)
      : sample_rate_(sample_rate),
        channels_(channels),
        frames_per_buffer_(frames_per_buffer),
        frames_per_10ms_buffer_(sample_rate / 100),
        bits_per_sample_(kBitsPerSample) {}
  void reset(int sample_rate, int channels, int frames_per_buffer) {
    sample_rate_ = sample_rate;
    channels_ = channels;
    frames_per_buffer_ = frames_per_buffer;
    frames_per_10ms_buffer_ = (sample_rate / 100);
  }
  int sample_rate() const { return sample_rate_; }
  int channels() const { return channels_; }
  int frames_per_buffer() const { return frames_per_buffer_; }
  int frames_per_10ms_buffer() const { return frames_per_10ms_buffer_; }
  int bits_per_sample() const { return bits_per_sample_; }
  bool is_valid() const {
    return ((sample_rate_ > 0) && (channels_ > 0) && (frames_per_buffer_ > 0));
  }
  int GetBytesPerFrame() const { return channels_ * bits_per_sample_ / 8; }
  int GetBytesPerBuffer() const {
    return frames_per_buffer_ * GetBytesPerFrame();
  }
  int GetBytesPer10msBuffer() const {
    return frames_per_10ms_buffer_ * GetBytesPerFrame();
  }
  float GetBufferSizeInMilliseconds() const {
    if (sample_rate_ == 0)
      return 0.0f;
    return frames_per_buffer_ / (sample_rate_ / 1000.0f);
  }

 private:
  int sample_rate_;
  int channels_;
  // Lowest possible size of native audio buffer. Measured in number of frames.
  // This size is injected into the OpenSL ES output (since it does not "talk
  // Java") implementation but is currently not utilized by the Java
  // implementation since it aquires the same value internally.
  int frames_per_buffer_;
  int frames_per_10ms_buffer_;
  int bits_per_sample_;
};

// Implements support for functions in the WebRTC audio stack for Android that
// relies on the AudioManager in android.media. It also populates an
// AudioParameter structure with native audio parameters detected at
// construction. This class does not make any audio-related modifications
// unless Init() is called. Caching audio parameters makes no changes but only
// reads data from the Java side.
class AudioManager {
 public:
  // Wraps the Java specific parts of the AudioManager into one helper class.
  // Stores method IDs for all supported methods at construction and then
  // allows calls like JavaAudioManager::Close() while hiding the Java/JNI
  // parts that are associated with this call.
  class JavaAudioManager {
   public:
    JavaAudioManager(NativeRegistration* native_registration,
                     rtc::scoped_ptr<GlobalRef> audio_manager);
    ~JavaAudioManager();

    bool Init();
    void Close();
    bool IsCommunicationModeEnabled();

   private:
    rtc::scoped_ptr<GlobalRef> audio_manager_;
    jmethodID init_;
    jmethodID dispose_;
    jmethodID is_communication_mode_enabled_;
  };

  AudioManager();
  ~AudioManager();

  // Sets the currently active audio layer combination. Must be called before
  // Init().
  void SetActiveAudioLayer(AudioDeviceModule::AudioLayer audio_layer);

  // Initializes the audio manager and stores the current audio mode.
  bool Init();
  // Revert any setting done by Init().
  bool Close();

  // Returns true if current audio mode is AudioManager.MODE_IN_COMMUNICATION.
  bool IsCommunicationModeEnabled() const;

  // Native audio parameters stored during construction.
  const AudioParameters& GetPlayoutAudioParameters();
  const AudioParameters& GetRecordAudioParameters();

  // Returns true if the device supports a built-in Acoustic Echo Canceler.
  // Some devices can also be blacklisted for use in combination with an AEC
  // and these devices will return false.
  // Can currently only be used in combination with a Java based audio backend
  // for the recoring side (i.e. using the android.media.AudioRecord API).
  bool IsAcousticEchoCancelerSupported() const;

  // Returns true if the device supports the low-latency audio paths in
  // combination with OpenSL ES.
  bool IsLowLatencyPlayoutSupported() const;

  // Returns the estimated total delay of this device. Unit is in milliseconds.
  // The vaule is set once at construction and never changes after that.
  // Possible values are webrtc::kLowLatencyModeDelayEstimateInMilliseconds and
  // webrtc::kHighLatencyModeDelayEstimateInMilliseconds.
  int GetDelayEstimateInMilliseconds() const;

 private:
  // Called from Java side so we can cache the native audio parameters.
  // This method will be called by the WebRtcAudioManager constructor, i.e.
  // on the same thread that this object is created on.
  static void JNICALL CacheAudioParameters(JNIEnv* env,
                                           jobject obj,
                                           jint sample_rate,
                                           jint channels,
                                           jboolean hardware_aec,
                                           jboolean low_latency_output,
                                           jint output_buffer_size,
                                           jint input_buffer_size,
                                           jlong native_audio_manager);
  void OnCacheAudioParameters(JNIEnv* env,
                              jint sample_rate,
                              jint channels,
                              jboolean hardware_aec,
                              jboolean low_latency_output,
                              jint output_buffer_size,
                              jint input_buffer_size);

  // Stores thread ID in the constructor.
  // We can then use ThreadChecker::CalledOnValidThread() to ensure that
  // other methods are called from the same thread.
  rtc::ThreadChecker thread_checker_;

  // Calls AttachCurrentThread() if this thread is not attached at construction.
  // Also ensures that DetachCurrentThread() is called at destruction.
  AttachCurrentThreadIfNeeded attach_thread_if_needed_;

  // Wraps the JNI interface pointer and methods associated with it.
  rtc::scoped_ptr<JNIEnvironment> j_environment_;

  // Contains factory method for creating the Java object.
  rtc::scoped_ptr<NativeRegistration> j_native_registration_;

  // Wraps the Java specific parts of the AudioManager.
  rtc::scoped_ptr<AudioManager::JavaAudioManager> j_audio_manager_;

  AudioDeviceModule::AudioLayer audio_layer_;

  // Set to true by Init() and false by Close().
  bool initialized_;

  // True if device supports hardware (or built-in) AEC.
  bool hardware_aec_;

  // True if device supports the low-latency OpenSL ES audio path.
  bool low_latency_playout_;

  // The delay estimate can take one of two fixed values depending on if the
  // device supports low-latency output or not.
  int delay_estimate_in_milliseconds_;

  // Contains native parameters (e.g. sample rate, channel configuration).
  // Set at construction in OnCacheAudioParameters() which is called from
  // Java on the same thread as this object is created on.
  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_MANAGER_H_
