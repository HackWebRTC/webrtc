#include "modules/transit_media/mute_audio_device.h"

#include <cmath>

#include "rtc_base/atomic_ops.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

#define LOGI() RTC_LOG(LS_INFO) << "MuteAudioDevice::"

#define LOG_AND_RETURN_IF_ERROR(error, message)    \
  do {                                             \
    OSStatus err = error;                          \
    if (err) {                                     \
      RTC_LOG(LS_ERROR) << message << ": " << err; \
      return false;                                \
    }                                              \
  } while (0)

#define LOG_IF_ERROR(error, message)               \
  do {                                             \
    OSStatus err = error;                          \
    if (err) {                                     \
      RTC_LOG(LS_ERROR) << message << ": " << err; \
    }                                              \
  } while (0)

// Hardcoded delay estimates based on real measurements.
// TODO(henrika): these value is not used in combination with built-in AEC.
// Can most likely be removed.
const uint16_t kFixedPlayoutDelayEstimate = 30;
const uint16_t kFixedRecordDelayEstimate = 30;

MuteAudioDevice::MuteAudioDevice(TaskQueueFactory* task_queue_factory)
    : queue_(task_queue_factory->CreateTaskQueue(
          "MuteAudioDevice",
          TaskQueueFactory::Priority::NORMAL)),
      next_deliver_ms_(0),
      audio_device_buffer_(nullptr),
      playout_samples_per_channel_10ms_(0),
      record_samples_per_channel_10ms_(0),
      recording_(0),
      playing_(0),
      initialized_(false),
      audio_is_initialized_(false) {
  LOGI() << "ctor";
}

MuteAudioDevice::~MuteAudioDevice() {
  LOGI() << "~dtor";
  Terminate();
}

void MuteAudioDevice::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  LOGI() << "AttachAudioBuffer";
  RTC_DCHECK(audioBuffer);

  audio_device_buffer_ = audioBuffer;
}

AudioDeviceGeneric::InitStatus MuteAudioDevice::Init() {
  LOGI() << "Init";
  if (initialized_) {
    return InitStatus::OK;
  }
  UpdateAudioDeviceBuffer();
  initialized_ = true;

  queue_.PostDelayedTask([this]() { DeliverData(); }, 10);

  return InitStatus::OK;
}

int32_t MuteAudioDevice::Terminate() {
  LOGI() << "Terminate";

  if (!initialized_) {
    return 0;
  }
  StopPlayout();
  StopRecording();
  initialized_ = false;
  return 0;
}

bool MuteAudioDevice::Initialized() const {
  return initialized_;
}

int32_t MuteAudioDevice::InitPlayout() {
  LOGI() << "InitPlayout";

  RTC_DCHECK(initialized_);
  RTC_DCHECK(!audio_is_initialized_);
  RTC_DCHECK(!playing_);
  if (!audio_is_initialized_) {
    if (!InitPlayOrRecord()) {
      RTC_LOG_F(LS_ERROR) << "InitPlayOrRecord failed for InitPlayout!";
      return -1;
    }
  }
  audio_is_initialized_ = true;
  return 0;
}

bool MuteAudioDevice::PlayoutIsInitialized() const {
  return audio_is_initialized_;
}

bool MuteAudioDevice::RecordingIsInitialized() const {
  return audio_is_initialized_;
}

int32_t MuteAudioDevice::InitRecording() {
  LOGI() << "InitRecording";

  RTC_DCHECK(initialized_);
  RTC_DCHECK(!audio_is_initialized_);
  RTC_DCHECK(!recording_);
  if (!audio_is_initialized_) {
    if (!InitPlayOrRecord()) {
      RTC_LOG_F(LS_ERROR) << "InitPlayOrRecord failed for InitRecording!";
      return -1;
    }
  }
  audio_is_initialized_ = true;
  return 0;
}

int32_t MuteAudioDevice::StartPlayout() {
  LOGI() << "StartPlayout";

  RTC_DCHECK(audio_is_initialized_);
  RTC_DCHECK(!playing_);
  rtc::AtomicOps::ReleaseStore(&playing_, 1);
  return 0;
}

int32_t MuteAudioDevice::StopPlayout() {
  LOGI() << "StopPlayout";

  if (!audio_is_initialized_ || !playing_) {
    return 0;
  }
  if (!recording_) {
    ShutdownPlayOrRecord();
    audio_is_initialized_ = false;
  }
  rtc::AtomicOps::ReleaseStore(&playing_, 0);
  return 0;
}

bool MuteAudioDevice::Playing() const {
  return playing_;
}

int32_t MuteAudioDevice::StartRecording() {
  LOGI() << "StartRecording";

  RTC_DCHECK(audio_is_initialized_);
  RTC_DCHECK(!recording_);
  rtc::AtomicOps::ReleaseStore(&recording_, 1);
  return 0;
}

int32_t MuteAudioDevice::StopRecording() {
  LOGI() << "StopRecording";

  if (!audio_is_initialized_ || !recording_) {
    return 0;
  }
  if (!playing_) {
    ShutdownPlayOrRecord();
    audio_is_initialized_ = false;
  }
  rtc::AtomicOps::ReleaseStore(&recording_, 0);
  return 0;
}

bool MuteAudioDevice::Recording() const {
  return recording_;
}

int32_t MuteAudioDevice::PlayoutDelay(uint16_t& delayMS) const {
  delayMS = kFixedPlayoutDelayEstimate;
  return 0;
}

void MuteAudioDevice::UpdateAudioDeviceBuffer() {
  LOGI() << "UpdateAudioDevicebuffer";
  // AttachAudioBuffer() is called at construction by the main class but check
  // just in case.
  RTC_DCHECK(audio_device_buffer_) << "AttachAudioBuffer must be called first";
  // Inform the audio device buffer (ADB) about the new audio format.
  audio_device_buffer_->SetPlayoutSampleRate(48000);
  audio_device_buffer_->SetPlayoutChannels(1);
  audio_device_buffer_->SetRecordingSampleRate(48000);
  audio_device_buffer_->SetRecordingChannels(1);

  playout_samples_per_channel_10ms_ = rtc::dchecked_cast<size_t>(
      audio_device_buffer_->PlayoutSampleRate() * 10 / 1000);
  record_samples_per_channel_10ms_ = rtc::dchecked_cast<size_t>(
      audio_device_buffer_->RecordingSampleRate() * 10 / 1000);
  record_buffer_.SetSize(record_samples_per_channel_10ms_ *
                         audio_device_buffer_->RecordingChannels());
}

bool MuteAudioDevice::InitPlayOrRecord() {
  LOGI() << "InitPlayOrRecord";

  return true;
}

void MuteAudioDevice::ShutdownPlayOrRecord() {
  LOGI() << "ShutdownPlayOrRecord";
}

void MuteAudioDevice::DeliverData() {
  if (!initialized_) {
    return;
  }

  if (recording_) {
    audio_device_buffer_->SetRecordedBuffer(record_buffer_.data(),
                                            record_samples_per_channel_10ms_);
    audio_device_buffer_->SetVQEData(kFixedPlayoutDelayEstimate,
                                     kFixedRecordDelayEstimate);
    audio_device_buffer_->DeliverRecordedData();
  }
  if (playing_) {
    audio_device_buffer_->RequestPlayoutData(playout_samples_per_channel_10ms_);
  }
  int64_t now_ms = rtc::TimeMicros() / 1000;
  if (next_deliver_ms_ == 0) {
    next_deliver_ms_ = now_ms + 10;
  } else {
    next_deliver_ms_ += 10 + next_deliver_ms_ - now_ms;
  }
  int64_t delay_ms = next_deliver_ms_ - now_ms;
  if (delay_ms < 3) {
    queue_.PostTask([this]() { DeliverData(); });
  } else {
    queue_.PostDelayedTask([this]() { DeliverData(); }, delay_ms);
  }
}

int32_t MuteAudioDevice::ActiveAudioLayer(
    AudioDeviceModule::AudioLayer& audioLayer) const {
  audioLayer = AudioDeviceModule::kDummyAudio;
  return 0;
}

int16_t MuteAudioDevice::PlayoutDevices() {
  // TODO(henrika): improve.
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return (int16_t)1;
}

int16_t MuteAudioDevice::RecordingDevices() {
  // TODO(henrika): improve.
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return (int16_t)1;
}

int32_t MuteAudioDevice::InitSpeaker() {
  return 0;
}

bool MuteAudioDevice::SpeakerIsInitialized() const {
  return true;
}

int32_t MuteAudioDevice::SpeakerVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t MuteAudioDevice::SetSpeakerVolume(uint32_t volume) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::SpeakerVolume(uint32_t& volume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::MaxSpeakerVolume(uint32_t& maxVolume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::MinSpeakerVolume(uint32_t& minVolume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::SpeakerMuteIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t MuteAudioDevice::SetSpeakerMute(bool enable) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::SpeakerMute(bool& enabled) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::SetPlayoutDevice(uint16_t index) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t MuteAudioDevice::SetPlayoutDevice(
    AudioDeviceModule::WindowsDeviceType) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::InitMicrophone() {
  return 0;
}

bool MuteAudioDevice::MicrophoneIsInitialized() const {
  return true;
}

int32_t MuteAudioDevice::MicrophoneMuteIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t MuteAudioDevice::SetMicrophoneMute(bool enable) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::MicrophoneMute(bool& enabled) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::StereoRecordingIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t MuteAudioDevice::SetStereoRecording(bool enable) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::StereoRecording(bool& enabled) const {
  enabled = false;
  return 0;
}

int32_t MuteAudioDevice::StereoPlayoutIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t MuteAudioDevice::SetStereoPlayout(bool enable) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::StereoPlayout(bool& enabled) const {
  enabled = false;
  return 0;
}

int32_t MuteAudioDevice::MicrophoneVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t MuteAudioDevice::SetMicrophoneVolume(uint32_t volume) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::MicrophoneVolume(uint32_t& volume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::MaxMicrophoneVolume(uint32_t& maxVolume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::MinMicrophoneVolume(uint32_t& minVolume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::PlayoutDeviceName(uint16_t index,
                                           char name[kAdmMaxDeviceNameSize],
                                           char guid[kAdmMaxGuidSize]) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::RecordingDeviceName(uint16_t index,
                                             char name[kAdmMaxDeviceNameSize],
                                             char guid[kAdmMaxGuidSize]) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::SetRecordingDevice(uint16_t index) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t MuteAudioDevice::SetRecordingDevice(
    AudioDeviceModule::WindowsDeviceType) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t MuteAudioDevice::PlayoutIsAvailable(bool& available) {
  available = true;
  return 0;
}

int32_t MuteAudioDevice::RecordingIsAvailable(bool& available) {
  available = true;
  return 0;
}

}  // namespace webrtc
