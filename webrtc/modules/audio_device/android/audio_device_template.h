/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_DEVICE_TEMPLATE_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_DEVICE_TEMPLATE_H_

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_device/audio_device_generic.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

// InputType/OutputType can be any class that implements the capturing/rendering
// part of the AudioDeviceGeneric API.
template <class InputType, class OutputType>
class AudioDeviceTemplate : public AudioDeviceGeneric {
 public:
  static void SetAndroidAudioDeviceObjects(void* javaVM,
                                           void* env,
                                           void* context) {
    OutputType::SetAndroidAudioDeviceObjects(javaVM, env, context);
    InputType::SetAndroidAudioDeviceObjects(javaVM, env, context);
  }

  static void ClearAndroidAudioDeviceObjects() {
    OutputType::ClearAndroidAudioDeviceObjects();
    InputType::ClearAndroidAudioDeviceObjects();
  }

  // TODO(henrika): remove id
  explicit AudioDeviceTemplate(const int32_t id)
      : output_(),
        input_(&output_) {
  }

  virtual ~AudioDeviceTemplate() {
  }

  int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer& audioLayer) const { // NOLINT
    audioLayer = AudioDeviceModule::kPlatformDefaultAudio;
    return 0;
  }

  int32_t Init() {
    return output_.Init() | input_.Init();
  }

  int32_t Terminate()  {
    return output_.Terminate() | input_.Terminate();
  }

  bool Initialized() const {
    return true;
  }

  int16_t PlayoutDevices() {
    return 1;
  }

  int16_t RecordingDevices() {
    return 1;
  }

  int32_t PlayoutDeviceName(
      uint16_t index,
      char name[kAdmMaxDeviceNameSize],
      char guid[kAdmMaxGuidSize]) {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t RecordingDeviceName(
      uint16_t index,
      char name[kAdmMaxDeviceNameSize],
      char guid[kAdmMaxGuidSize]) {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetPlayoutDevice(uint16_t index) {
    // OK to use but it has no effect currently since device selection is
    // done using Andoid APIs instead.
    return 0;
  }

  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetRecordingDevice(uint16_t index) {
    // OK to use but it has no effect currently since device selection is
    // done using Andoid APIs instead.
    return 0;
  }

  int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t PlayoutIsAvailable(
      bool& available) {  // NOLINT
    available = true;
    return 0;
  }

  int32_t InitPlayout() {
    return output_.InitPlayout();
  }

  bool PlayoutIsInitialized() const {
    return output_.PlayoutIsInitialized();
  }

  int32_t RecordingIsAvailable(
      bool& available) {  // NOLINT
    available = true;
    return 0;
  }

  int32_t InitRecording() {
    return input_.InitRecording();
  }

  bool RecordingIsInitialized() const {
    return input_.RecordingIsInitialized();
  }

  int32_t StartPlayout() {
    return output_.StartPlayout();
  }

  int32_t StopPlayout() {
    return output_.StopPlayout();
  }

  bool Playing() const {
    return output_.Playing();
  }

  int32_t StartRecording() {
    return input_.StartRecording();
  }

  int32_t StopRecording() {
    return input_.StopRecording();
  }

  bool Recording() const {
    return input_.Recording() ;
  }

  int32_t SetAGC(bool enable) {
    if (enable) {
      FATAL() << "Should never be called";
    }
    return -1;
  }

  bool AGC() const {
    return false;
  }

  int32_t SetWaveOutVolume(uint16_t volumeLeft,
                           uint16_t volumeRight) {
     FATAL() << "Should never be called";
    return -1;
  }

  int32_t WaveOutVolume(
      uint16_t& volumeLeft,           // NOLINT
      uint16_t& volumeRight) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t InitSpeaker() {
    return 0;
  }

  bool SpeakerIsInitialized() const {
    return true;
  }

  int32_t InitMicrophone() {
    return 0;
  }

  bool MicrophoneIsInitialized() const {
    return true;
  }

  int32_t SpeakerVolumeIsAvailable(
      bool& available) {  // NOLINT
    available = false;
    FATAL() << "Should never be called";
    return -1;
  }

  // TODO(henrika): add support  if/when needed.
  int32_t SetSpeakerVolume(uint32_t volume) {
    FATAL() << "Should never be called";
    return -1;
  }

  // TODO(henrika): add support  if/when needed.
  int32_t SpeakerVolume(
      uint32_t& volume) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  // TODO(henrika): add support  if/when needed.
  int32_t MaxSpeakerVolume(
      uint32_t& maxVolume) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  // TODO(henrika): add support  if/when needed.
  int32_t MinSpeakerVolume(
      uint32_t& minVolume) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SpeakerVolumeStepSize(
      uint16_t& stepSize) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MicrophoneVolumeIsAvailable(
      bool& available) {  // NOLINT
    available = false;
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetMicrophoneVolume(uint32_t volume) {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MicrophoneVolume(
      uint32_t& volume) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MaxMicrophoneVolume(
      uint32_t& maxVolume) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MinMicrophoneVolume(
      uint32_t& minVolume) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MicrophoneVolumeStepSize(
      uint16_t& stepSize) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SpeakerMuteIsAvailable(
      bool& available) {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetSpeakerMute(bool enable) {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SpeakerMute(
      bool& enabled) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MicrophoneMuteIsAvailable(
      bool& available) {  // NOLINT
    FATAL() << "Not implemented";
    return -1;
  }

  int32_t SetMicrophoneMute(bool enable) {
    FATAL() << "Not implemented";
    return -1;
  }

  int32_t MicrophoneMute(
      bool& enabled) const {  // NOLINT
    FATAL() << "Not implemented";
    return -1;
  }

  int32_t MicrophoneBoostIsAvailable(
      bool& available) {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetMicrophoneBoost(bool enable) {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MicrophoneBoost(
      bool& enabled) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t StereoPlayoutIsAvailable(
      bool& available) {  // NOLINT
    available = false;
    return 0;
  }

  int32_t SetStereoPlayout(bool enable) {
    return -1;
  }

  int32_t StereoPlayout(
      bool& enabled) const {  // NOLINT
    enabled = false;
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t StereoRecordingIsAvailable(
      bool& available) {  // NOLINT
    available = false;
    return 0;
  }

  int32_t SetStereoRecording(bool enable) {
    return -1;
  }

  int32_t StereoRecording(
      bool& enabled) const {  // NOLINT
    enabled = false;
    return 0;
  }

  int32_t SetPlayoutBuffer(
      const AudioDeviceModule::BufferType type,
      uint16_t sizeMS) {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t PlayoutBuffer(
      AudioDeviceModule::BufferType& type,
      uint16_t& sizeMS) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t PlayoutDelay(
      uint16_t& delayMS) const {  // NOLINT
    return output_.PlayoutDelay(delayMS);
  }

  int32_t RecordingDelay(
      uint16_t& delayMS) const {  // NOLINT
    return input_.RecordingDelay(delayMS);
  }

  int32_t CPULoad(
      uint16_t& load) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  bool PlayoutWarning() const {
    return false;
  }

  bool PlayoutError() const {
    return false;
  }

  bool RecordingWarning() const {
    return false;
  }

  bool RecordingError() const {
    return false;
  }

  void ClearPlayoutWarning() {}

  void ClearPlayoutError() {}

  void ClearRecordingWarning() {}

  void ClearRecordingError() {}

  void AttachAudioBuffer(
      AudioDeviceBuffer* audioBuffer) {
    output_.AttachAudioBuffer(audioBuffer);
    input_.AttachAudioBuffer(audioBuffer);
  }

  // TODO(henrika): remove
  int32_t SetPlayoutSampleRate(
      const uint32_t samplesPerSec) {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetLoudspeakerStatus(bool enable) {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t GetLoudspeakerStatus(
      bool& enable) const {  // NOLINT
    FATAL() << "Should never be called";
    return -1;
  }

  bool BuiltInAECIsAvailable() const {
    return input_.BuiltInAECIsAvailable();
  }

  int32_t EnableBuiltInAEC(bool enable) {
    return input_.EnableBuiltInAEC(enable);
  }

 private:
  OutputType output_;
  InputType input_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_DEVICE_TEMPLATE_H_
