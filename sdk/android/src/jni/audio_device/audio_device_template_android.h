/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_AUDIO_DEVICE_TEMPLATE_ANDROID_H_
#define SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_AUDIO_DEVICE_TEMPLATE_ANDROID_H_

#include <memory>

#include "modules/audio_device/audio_device_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread_checker.h"
#include "sdk/android/src/jni/audio_device/audio_manager.h"
#include "system_wrappers/include/metrics.h"

#define CHECKinitialized_() \
  {                         \
    if (!initialized_) {    \
      return -1;            \
    }                       \
  }

#define CHECKinitialized__BOOL() \
  {                              \
    if (!initialized_) {         \
      return false;              \
    }                            \
  }

namespace webrtc {

namespace android_adm {

// InputType/OutputType can be any class that implements the capturing/rendering
// part of the AudioDeviceGeneric API.
// Construction and destruction must be done on one and the same thread. Each
// internal implementation of InputType and OutputType will RTC_DCHECK if that
// is not the case. All implemented methods must also be called on the same
// thread. See comments in each InputType/OutputType class for more info.
// It is possible to call the two static methods (SetAndroidAudioDeviceObjects
// and ClearAndroidAudioDeviceObjects) from a different thread but both will
// RTC_CHECK that the calling thread is attached to a Java VM.

template <class InputType, class OutputType>
class AudioDeviceTemplateAndroid : public AudioDeviceModule {
 public:
  // For use with UMA logging. Must be kept in sync with histograms.xml in
  // Chrome, located at
  // https://cs.chromium.org/chromium/src/tools/metrics/histograms/histograms.xml
  enum class InitStatus {
    OK = 0,
    PLAYOUT_ERROR = 1,
    RECORDING_ERROR = 2,
    OTHER_ERROR = 3,
    NUM_STATUSES = 4
  };

  explicit AudioDeviceTemplateAndroid(AudioDeviceModule::AudioLayer audio_layer)
      : audio_layer_(audio_layer), initialized_(false) {
    RTC_LOG(INFO) << __FUNCTION__;
    thread_checker_.DetachFromThread();
  }

  virtual ~AudioDeviceTemplateAndroid() { RTC_LOG(INFO) << __FUNCTION__; }

  int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer* audioLayer) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    *audioLayer = audio_layer_;
    return 0;
  }

  int32_t RegisterAudioCallback(AudioTransport* audioCallback) override {
    RTC_LOG(INFO) << __FUNCTION__;
    return audio_device_buffer_->RegisterAudioCallback(audioCallback);
  }

  int32_t Init() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(thread_checker_.CalledOnValidThread());
    audio_manager_ = rtc::MakeUnique<AudioManager>();
    output_ = rtc::MakeUnique<OutputType>(audio_manager_.get());
    input_ = rtc::MakeUnique<InputType>(audio_manager_.get());
    audio_manager_->SetActiveAudioLayer(audio_layer_);
    audio_device_buffer_ = rtc::MakeUnique<AudioDeviceBuffer>();
    AttachAudioBuffer();
    if (initialized_) {
      return 0;
    }
    InitStatus status;
    if (!audio_manager_->Init()) {
      status = InitStatus::OTHER_ERROR;
    } else if (output_->Init() != 0) {
      audio_manager_->Close();
      status = InitStatus::PLAYOUT_ERROR;
    } else if (input_->Init() != 0) {
      output_->Terminate();
      audio_manager_->Close();
      status = InitStatus::RECORDING_ERROR;
    } else {
      initialized_ = true;
      status = InitStatus::OK;
    }
    RTC_HISTOGRAM_ENUMERATION("WebRTC.Audio.InitializationResult",
                              static_cast<int>(status),
                              static_cast<int>(InitStatus::NUM_STATUSES));
    if (status != InitStatus::OK) {
      RTC_LOG(LS_ERROR) << "Audio device initialization failed.";
      return -1;
    }
    return 0;
  }

  int32_t Terminate() override {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_)
      return 0;
    RTC_DCHECK(thread_checker_.CalledOnValidThread());
    int32_t err = input_->Terminate();
    err |= output_->Terminate();
    err |= !audio_manager_->Close();
    initialized_ = false;
    RTC_DCHECK_EQ(err, 0);
    return err;
  }

  bool Initialized() const override {
    RTC_LOG(INFO) << __FUNCTION__ << ":" << initialized_;
    RTC_DCHECK(thread_checker_.CalledOnValidThread());  // not done in _impl
    return initialized_;
  }

  int16_t PlayoutDevices() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_LOG(INFO) << "output: " << 1;
    return 1;
  }

  int16_t RecordingDevices() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_LOG(INFO) << "output: " << 1;
    return 1;
  }

  int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetPlayoutDevice(uint16_t index) override {
    // OK to use but it has no effect currently since device selection is
    // done using Andoid APIs instead.
    RTC_LOG(INFO) << __FUNCTION__ << "(" << index << ")";
    return 0;
  }

  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetRecordingDevice(uint16_t index) override {
    // OK to use but it has no effect currently since device selection is
    // done using Andoid APIs instead.
    RTC_LOG(INFO) << __FUNCTION__ << "(" << index << ")";
    return 0;
  }

  int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t PlayoutIsAvailable(bool* available) override {
    RTC_LOG(INFO) << __FUNCTION__;
    *available = true;
    RTC_LOG(INFO) << "output: " << *available;
    return 0;
  }

  int32_t InitPlayout() override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    if (PlayoutIsInitialized()) {
      return 0;
    }
    int32_t result = output_->InitPlayout();
    RTC_LOG(INFO) << "output: " << result;
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.InitPlayoutSuccess",
                          static_cast<int>(result == 0));
    return result;
  }

  bool PlayoutIsInitialized() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized__BOOL();
    return output_->PlayoutIsInitialized();
  }

  int32_t RecordingIsAvailable(bool* available) override {
    RTC_LOG(INFO) << __FUNCTION__;
    *available = true;
    RTC_LOG(INFO) << "output: " << *available;
    return 0;
  }

  int32_t InitRecording() override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    if (RecordingIsInitialized()) {
      return 0;
    }
    int32_t result = input_->InitRecording();
    RTC_LOG(INFO) << "output: " << result;
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.InitRecordingSuccess",
                          static_cast<int>(result == 0));
    return result;
  }

  bool RecordingIsInitialized() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized__BOOL();
    return input_->RecordingIsInitialized();
  }

  int32_t StartPlayout() override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    if (Playing()) {
      return 0;
    }
    audio_device_buffer_->StartPlayout();
    if (!audio_manager_->IsCommunicationModeEnabled()) {
      RTC_LOG(WARNING)
          << "The application should use MODE_IN_COMMUNICATION audio mode!";
    }
    int32_t result = output_->StartPlayout();
    RTC_LOG(INFO) << "output: " << result;
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.StartPlayoutSuccess",
                          static_cast<int>(result == 0));
    return result;
  }

  int32_t StopPlayout() override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    // Avoid using audio manger (JNI/Java cost) if playout was inactive.
    if (!Playing())
      return 0;
    RTC_LOG(INFO) << __FUNCTION__;
    audio_device_buffer_->StopPlayout();
    int32_t result = output_->StopPlayout();
    RTC_LOG(INFO) << "output: " << result;
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.StopPlayoutSuccess",
                          static_cast<int>(result == 0));
    return result;
  }

  bool Playing() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized__BOOL();
    return output_->Playing();
  }

  int32_t StartRecording() override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    if (Recording()) {
      return 0;
    }
    if (!audio_manager_->IsCommunicationModeEnabled()) {
      RTC_LOG(WARNING)
          << "The application should use MODE_IN_COMMUNICATION audio mode!";
    }
    audio_device_buffer_->StartRecording();
    int32_t result = input_->StartRecording();
    RTC_LOG(INFO) << "output: " << result;
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.StartRecordingSuccess",
                          static_cast<int>(result == 0));
    return result;
  }

  int32_t StopRecording() override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    // Avoid using audio manger (JNI/Java cost) if recording was inactive.
    if (!Recording())
      return 0;
    audio_device_buffer_->StopRecording();
    int32_t result = input_->StopRecording();
    RTC_LOG(INFO) << "output: " << result;
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.StopRecordingSuccess",
                          static_cast<int>(result == 0));
    return result;
  }

  bool Recording() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized__BOOL();
    return input_->Recording();
  }

  int32_t InitSpeaker() override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    return 0;
  }

  bool SpeakerIsInitialized() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized__BOOL();
    RTC_LOG(INFO) << "output: " << true;
    return true;
  }

  int32_t InitMicrophone() override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    return 0;
  }

  bool MicrophoneIsInitialized() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized__BOOL();
    RTC_LOG(INFO) << "output: " << true;
    return true;
  }

  int32_t SpeakerVolumeIsAvailable(bool* available) override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    if (output_->SpeakerVolumeIsAvailable(available) == -1) {
      return -1;
    }
    RTC_LOG(INFO) << "output: " << *available;
    return 0;
  }

  int32_t SetSpeakerVolume(uint32_t volume) override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    return output_->SetSpeakerVolume(volume);
  }

  int32_t SpeakerVolume(uint32_t* volume) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    if (output_->SpeakerVolume(volume) == -1) {
      return -1;
    }
    RTC_LOG(INFO) << "output: " << *volume;
    return 0;
  }

  int32_t MaxSpeakerVolume(uint32_t* maxVolume) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    if (output_->MaxSpeakerVolume(maxVolume) == -1) {
      return -1;
    }
    return 0;
  }

  int32_t MinSpeakerVolume(uint32_t* minVolume) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    if (output_->MinSpeakerVolume(minVolume) == -1) {
      return -1;
    }
    return 0;
  }

  int32_t MicrophoneVolumeIsAvailable(bool* available) override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    *available = false;
    RTC_LOG(INFO) << "output: " << *available;
    return -1;
  }

  int32_t SetMicrophoneVolume(uint32_t volume) override {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << volume << ")";
    CHECKinitialized_();
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MicrophoneVolume(uint32_t* volume) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MinMicrophoneVolume(uint32_t* minVolume) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SpeakerMuteIsAvailable(bool* available) override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetSpeakerMute(bool enable) override {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    CHECKinitialized_();
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SpeakerMute(bool* enabled) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MicrophoneMuteIsAvailable(bool* available) override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    FATAL() << "Not implemented";
    return -1;
  }

  int32_t SetMicrophoneMute(bool enable) override {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    CHECKinitialized_();
    FATAL() << "Not implemented";
    return -1;
  }

  int32_t MicrophoneMute(bool* enabled) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    FATAL() << "Not implemented";
    return -1;
  }

  // Returns true if the audio manager has been configured to support stereo
  // and false otherwised. Default is mono.
  int32_t StereoPlayoutIsAvailable(bool* available) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    *available = audio_manager_->IsStereoPlayoutSupported();
    RTC_LOG(INFO) << "output: " << *available;
    return 0;
  }

  int32_t SetStereoPlayout(bool enable) override {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    CHECKinitialized_();
    if (PlayoutIsInitialized()) {
      RTC_LOG(WARNING) << "recording in stereo is not supported";
      return -1;
    }
    bool available = audio_manager_->IsStereoPlayoutSupported();
    // Android does not support changes between mono and stero on the fly.
    // Instead, the native audio layer is configured via the audio manager
    // to either support mono or stereo. It is allowed to call this method
    // if that same state is not modified.
    if (enable != available) {
      RTC_LOG(WARNING) << "failed to change stereo recording";
      return -1;
    }
    int8_t nChannels(1);
    if (enable) {
      nChannels = 2;
    }
    audio_device_buffer_->SetPlayoutChannels(nChannels);
    return 0;
  }

  int32_t StereoPlayout(bool* enabled) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    *enabled = audio_manager_->IsStereoPlayoutSupported();
    RTC_LOG(INFO) << "output: " << *enabled;
    return 0;
  }

  int32_t StereoRecordingIsAvailable(bool* available) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    bool isAvailable = false;
    if (audio_manager_->IsStereoRecordSupported() == -1) {
      return -1;
    }
    *available = isAvailable;
    RTC_LOG(INFO) << "output: " << isAvailable;
    return 0;
  }

  int32_t SetStereoRecording(bool enable) override {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    CHECKinitialized_();
    if (RecordingIsInitialized()) {
      RTC_LOG(WARNING) << "recording in stereo is not supported";
      return -1;
    }
    bool available = audio_manager_->IsStereoRecordSupported();
    // Android does not support changes between mono and stero on the fly.
    // Instead, the native audio layer is configured via the audio manager
    // to either support mono or stereo. It is allowed to call this method
    // if that same state is not modified.
    if (enable != available) {
      RTC_LOG(WARNING) << "failed to change stereo recording";
      return -1;
    }
    int8_t nChannels(1);
    if (enable) {
      nChannels = 2;
    }
    audio_device_buffer_->SetRecordingChannels(nChannels);
    return 0;
  }

  int32_t StereoRecording(bool* enabled) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized_();
    *enabled = audio_manager_->IsStereoRecordSupported();
    RTC_LOG(INFO) << "output: " << *enabled;
    return 0;
  }

  int32_t PlayoutDelay(uint16_t* delay_ms) const override {
    CHECKinitialized_();
    // Best guess we can do is to use half of the estimated total delay.
    *delay_ms = audio_manager_->GetDelayEstimateInMilliseconds() / 2;
    RTC_DCHECK_GT(*delay_ms, 0);
    return 0;
  }

  // Returns true if the device both supports built in AEC and the device
  // is not blacklisted.
  // Currently, if OpenSL ES is used in both directions, this method will still
  // report the correct value and it has the correct effect. As an example:
  // a device supports built in AEC and this method returns true. Libjingle
  // will then disable the WebRTC based AEC and that will work for all devices
  // (mainly Nexus) even when OpenSL ES is used for input since our current
  // implementation will enable built-in AEC by default also for OpenSL ES.
  // The only "bad" thing that happens today is that when Libjingle calls
  // OpenSLESRecorder::EnableBuiltInAEC() it will not have any real effect and
  // a "Not Implemented" log will be filed. This non-perfect state will remain
  // until I have added full support for audio effects based on OpenSL ES APIs.
  bool BuiltInAECIsAvailable() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized__BOOL();
    bool isAvailable = audio_manager_->IsAcousticEchoCancelerSupported();
    RTC_LOG(INFO) << "output: " << isAvailable;
    return isAvailable;
  }

  // Returns true if the device both supports built in AGC and the device
  // is not blacklisted.
  // TODO(henrika): add implementation for OpenSL ES based audio as well.
  // In addition, see comments for BuiltInAECIsAvailable().
  bool BuiltInAGCIsAvailable() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized__BOOL();
    bool isAvailable = audio_manager_->IsAutomaticGainControlSupported();
    RTC_LOG(INFO) << "output: " << isAvailable;
    return isAvailable;
  }

  // Returns true if the device both supports built in NS and the device
  // is not blacklisted.
  // TODO(henrika): add implementation for OpenSL ES based audio as well.
  // In addition, see comments for BuiltInAECIsAvailable().
  bool BuiltInNSIsAvailable() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    CHECKinitialized__BOOL();
    bool isAvailable = audio_manager_->IsNoiseSuppressorSupported();
    RTC_LOG(INFO) << "output: " << isAvailable;
    return isAvailable;
  }

  // TODO(henrika): add implementation for OpenSL ES based audio as well.
  int32_t EnableBuiltInAEC(bool enable) override {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    CHECKinitialized_();
    RTC_CHECK(BuiltInAECIsAvailable()) << "HW AEC is not available";
    int32_t result = input_->EnableBuiltInAEC(enable);
    RTC_LOG(INFO) << "output: " << result;
    return result;
  }

  // TODO(henrika): add implementation for OpenSL ES based audio as well.
  int32_t EnableBuiltInAGC(bool enable) override {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    CHECKinitialized_();
    RTC_CHECK(BuiltInAGCIsAvailable()) << "HW AGC is not available";
    int32_t result = input_->EnableBuiltInAGC(enable);
    RTC_LOG(INFO) << "output: " << result;
    return result;
  }

  // TODO(henrika): add implementation for OpenSL ES based audio as well.
  int32_t EnableBuiltInNS(bool enable) override {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    CHECKinitialized_();
    RTC_CHECK(BuiltInNSIsAvailable()) << "HW NS is not available";
    int32_t result = input_->EnableBuiltInNS(enable);
    RTC_LOG(INFO) << "output: " << result;
    return result;
  }

  AudioDeviceModule::AudioLayer PlatformAudioLayer() const {
    RTC_LOG(INFO) << __FUNCTION__;
    return audio_layer_;
  }

  int32_t AttachAudioBuffer() {
    RTC_LOG(INFO) << __FUNCTION__;
    output_->AttachAudioBuffer(audio_device_buffer_.get());
    input_->AttachAudioBuffer(audio_device_buffer_.get());
    return 0;
  }

  AudioDeviceBuffer* GetAudioDeviceBuffer() {
    return audio_device_buffer_.get();
  }

 private:
  rtc::ThreadChecker thread_checker_;

  AudioDeviceModule::AudioLayer audio_layer_;

  std::unique_ptr<AudioManager> audio_manager_;
  std::unique_ptr<OutputType> output_;
  std::unique_ptr<InputType> input_;
  std::unique_ptr<AudioDeviceBuffer> audio_device_buffer_;

  bool initialized_;
};

}  // namespace android_adm

}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_AUDIO_DEVICE_TEMPLATE_ANDROID_H_
