#pragma once

#include <memory>

#include "modules/audio_device/audio_device_generic.h"
#include "rtc_base/buffer.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread.h"

namespace webrtc {

class MuteAudioDevice : public AudioDeviceGeneric {
 public:
  MuteAudioDevice(TaskQueueFactory* task_queue_factory);
  ~MuteAudioDevice() override;

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) override;

  InitStatus Init() override;
  int32_t Terminate() override;
  bool Initialized() const override;

  int32_t InitPlayout() override;
  bool PlayoutIsInitialized() const override;

  int32_t InitRecording() override;
  bool RecordingIsInitialized() const override;

  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  bool Playing() const override;

  int32_t StartRecording() override;
  int32_t StopRecording() override;
  bool Recording() const override;

  int32_t PlayoutDelay(uint16_t& delayMS) const override;

  int32_t GetPlayoutUnderrunCount() const override { return -1; }

  // These methods are currently not fully implemented

  // See audio_device_not_implemented.cc for trivial implementations.
  int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer& audioLayer) const override;
  int32_t PlayoutIsAvailable(bool& available) override;
  int32_t RecordingIsAvailable(bool& available) override;
  int16_t PlayoutDevices() override;
  int16_t RecordingDevices() override;
  int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]) override;
  int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]) override;
  int32_t SetPlayoutDevice(uint16_t index) override;
  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) override;
  int32_t SetRecordingDevice(uint16_t index) override;
  int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) override;
  int32_t InitSpeaker() override;
  bool SpeakerIsInitialized() const override;
  int32_t InitMicrophone() override;
  bool MicrophoneIsInitialized() const override;
  int32_t SpeakerVolumeIsAvailable(bool& available) override;
  int32_t SetSpeakerVolume(uint32_t volume) override;
  int32_t SpeakerVolume(uint32_t& volume) const override;
  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const override;
  int32_t MinSpeakerVolume(uint32_t& minVolume) const override;
  int32_t MicrophoneVolumeIsAvailable(bool& available) override;
  int32_t SetMicrophoneVolume(uint32_t volume) override;
  int32_t MicrophoneVolume(uint32_t& volume) const override;
  int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const override;
  int32_t MinMicrophoneVolume(uint32_t& minVolume) const override;
  int32_t MicrophoneMuteIsAvailable(bool& available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool& enabled) const override;
  int32_t SpeakerMuteIsAvailable(bool& available) override;
  int32_t SetSpeakerMute(bool enable) override;
  int32_t SpeakerMute(bool& enabled) const override;
  int32_t StereoPlayoutIsAvailable(bool& available) override;
  int32_t SetStereoPlayout(bool enable) override;
  int32_t StereoPlayout(bool& enabled) const override;
  int32_t StereoRecordingIsAvailable(bool& available) override;
  int32_t SetStereoRecording(bool enable) override;
  int32_t StereoRecording(bool& enabled) const override;

 private:
  void UpdateAudioDeviceBuffer();

  bool InitPlayOrRecord();

  void ShutdownPlayOrRecord();

  void DeliverData();

  rtc::TaskQueue queue_;
  int64_t next_deliver_ms_;

  // Raw pointer handle provided to us in AttachAudioBuffer(). Owned by the
  // AudioDeviceModuleImpl class and called by AudioDeviceModule::Create().
  // The AudioDeviceBuffer is a member of the AudioDeviceModuleImpl instance
  // and therefore outlives this object.
  AudioDeviceBuffer* audio_device_buffer_;
  size_t playout_samples_per_channel_10ms_;
  size_t record_samples_per_channel_10ms_;

  // Temporary storage for recorded data. AudioUnitRender() renders into this
  // array as soon as a frame of the desired buffer size has been recorded.
  // On real iOS devices, the size will be fixed and set once. For iOS
  // simulators, the size can vary from callback to callback and the size
  // will be changed dynamically to account for this behavior.
  rtc::BufferT<int16_t, true> record_buffer_;

  // Set to 1 when recording is active and 0 otherwise.
  volatile int recording_;

  // Set to 1 when playout is active and 0 otherwise.
  volatile int playing_;

  // Set to true after successful call to Init(), false otherwise.
  volatile bool initialized_;

  // Set to true after successful call to InitRecording() or InitPlayout(),
  // false otherwise.
  bool audio_is_initialized_;
};

}  // namespace webrtc
