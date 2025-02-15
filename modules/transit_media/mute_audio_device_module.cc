#include "modules/transit_media/mute_audio_device_module.h"

#include "api/task_queue/default_task_queue_factory.h"
#include "modules/audio_device/audio_device_config.h"
#include "modules/audio_device/audio_device_generic.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"

#define CHECKinitialized_() \
  {                         \
    if (!initialized_) {    \
      return -1;            \
    };                      \
  }

#define CHECKinitialized__BOOL() \
  {                              \
    if (!initialized_) {         \
      return false;              \
    };                           \
  }

namespace webrtc {

MuteAudioDeviceModule::MuteAudioDeviceModule()
    : initialized_(false),
      task_queue_factory_(CreateDefaultTaskQueueFactory()) {}

int32_t MuteAudioDeviceModule::AttachAudioBuffer() {
  RTC_LOG(INFO) << __FUNCTION__;
  audio_device_->AttachAudioBuffer(audio_device_buffer_.get());
  return 0;
}

MuteAudioDeviceModule::~MuteAudioDeviceModule() {
  RTC_LOG(INFO) << __FUNCTION__;
}

int32_t MuteAudioDeviceModule::ActiveAudioLayer(AudioLayer* audioLayer) const {
  RTC_LOG(INFO) << __FUNCTION__;
  AudioLayer activeAudio;
  if (audio_device_->ActiveAudioLayer(activeAudio) == -1) {
    return -1;
  }
  *audioLayer = activeAudio;
  return 0;
}

int32_t MuteAudioDeviceModule::Init() {
  RTC_LOG(INFO) << __FUNCTION__;
  if (initialized_)
    return 0;

  audio_device_buffer_.reset(
      new webrtc::AudioDeviceBuffer(task_queue_factory_.get()));
  audio_device_.reset(new MuteAudioDevice(task_queue_factory_.get()));
  RTC_CHECK(audio_device_);

  this->AttachAudioBuffer();

  AudioDeviceGeneric::InitStatus status = audio_device_->Init();
  if (status != AudioDeviceGeneric::InitStatus::OK) {
    RTC_LOG(LS_ERROR) << "Audio device initialization failed.";
    return -1;
  }
  initialized_ = true;
  return 0;
}

int32_t MuteAudioDeviceModule::Terminate() {
  RTC_LOG(INFO) << __FUNCTION__;
  if (!initialized_)
    return 0;
  if (audio_device_->Terminate() == -1) {
    return -1;
  }
  initialized_ = false;
  return 0;
}

bool MuteAudioDeviceModule::Initialized() const {
  RTC_LOG(INFO) << __FUNCTION__ << ": " << initialized_;
  return initialized_;
}

int32_t MuteAudioDeviceModule::InitSpeaker() {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  return audio_device_->InitSpeaker();
}

int32_t MuteAudioDeviceModule::InitMicrophone() {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  return audio_device_->InitMicrophone();
}

int32_t MuteAudioDeviceModule::SpeakerVolumeIsAvailable(bool* available) {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->SpeakerVolumeIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  RTC_LOG(INFO) << "output: " << isAvailable;
  return 0;
}

int32_t MuteAudioDeviceModule::SetSpeakerVolume(uint32_t volume) {
  RTC_LOG(INFO) << __FUNCTION__ << "(" << volume << ")";
  CHECKinitialized_();
  return audio_device_->SetSpeakerVolume(volume);
}

int32_t MuteAudioDeviceModule::SpeakerVolume(uint32_t* volume) const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  uint32_t level = 0;
  if (audio_device_->SpeakerVolume(level) == -1) {
    return -1;
  }
  *volume = level;
  RTC_LOG(INFO) << "output: " << *volume;
  return 0;
}

bool MuteAudioDeviceModule::SpeakerIsInitialized() const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized__BOOL();
  bool isInitialized = audio_device_->SpeakerIsInitialized();
  RTC_LOG(INFO) << "output: " << isInitialized;
  return isInitialized;
}

bool MuteAudioDeviceModule::MicrophoneIsInitialized() const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized__BOOL();
  bool isInitialized = audio_device_->MicrophoneIsInitialized();
  RTC_LOG(INFO) << "output: " << isInitialized;
  return isInitialized;
}

int32_t MuteAudioDeviceModule::MaxSpeakerVolume(uint32_t* maxVolume) const {
  CHECKinitialized_();
  uint32_t maxVol = 0;
  if (audio_device_->MaxSpeakerVolume(maxVol) == -1) {
    return -1;
  }
  *maxVolume = maxVol;
  return 0;
}

int32_t MuteAudioDeviceModule::MinSpeakerVolume(uint32_t* minVolume) const {
  CHECKinitialized_();
  uint32_t minVol = 0;
  if (audio_device_->MinSpeakerVolume(minVol) == -1) {
    return -1;
  }
  *minVolume = minVol;
  return 0;
}

int32_t MuteAudioDeviceModule::SpeakerMuteIsAvailable(bool* available) {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->SpeakerMuteIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  RTC_LOG(INFO) << "output: " << isAvailable;
  return 0;
}

int32_t MuteAudioDeviceModule::SetSpeakerMute(bool enable) {
  RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
  CHECKinitialized_();
  return audio_device_->SetSpeakerMute(enable);
}

int32_t MuteAudioDeviceModule::SpeakerMute(bool* enabled) const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  bool muted = false;
  if (audio_device_->SpeakerMute(muted) == -1) {
    return -1;
  }
  *enabled = muted;
  RTC_LOG(INFO) << "output: " << muted;
  return 0;
}

int32_t MuteAudioDeviceModule::MicrophoneMuteIsAvailable(bool* available) {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->MicrophoneMuteIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  RTC_LOG(INFO) << "output: " << isAvailable;
  return 0;
}

int32_t MuteAudioDeviceModule::SetMicrophoneMute(bool enable) {
  RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
  CHECKinitialized_();
  return (audio_device_->SetMicrophoneMute(enable));
}

int32_t MuteAudioDeviceModule::MicrophoneMute(bool* enabled) const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  bool muted = false;
  if (audio_device_->MicrophoneMute(muted) == -1) {
    return -1;
  }
  *enabled = muted;
  RTC_LOG(INFO) << "output: " << muted;
  return 0;
}

int32_t MuteAudioDeviceModule::MicrophoneVolumeIsAvailable(bool* available) {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->MicrophoneVolumeIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  RTC_LOG(INFO) << "output: " << isAvailable;
  return 0;
}

int32_t MuteAudioDeviceModule::SetMicrophoneVolume(uint32_t volume) {
  RTC_LOG(INFO) << __FUNCTION__ << "(" << volume << ")";
  CHECKinitialized_();
  return (audio_device_->SetMicrophoneVolume(volume));
}

int32_t MuteAudioDeviceModule::MicrophoneVolume(uint32_t* volume) const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  uint32_t level = 0;
  if (audio_device_->MicrophoneVolume(level) == -1) {
    return -1;
  }
  *volume = level;
  RTC_LOG(INFO) << "output: " << *volume;
  return 0;
}

int32_t MuteAudioDeviceModule::StereoRecordingIsAvailable(
    bool* available) const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->StereoRecordingIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  RTC_LOG(INFO) << "output: " << isAvailable;
  return 0;
}

int32_t MuteAudioDeviceModule::SetStereoRecording(bool enable) {
  RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
  CHECKinitialized_();
  if (enable) {
    RTC_LOG(WARNING) << "recording in stereo is not supported";
  }
  return -1;
}

int32_t MuteAudioDeviceModule::StereoRecording(bool* enabled) const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  bool stereo = false;
  if (audio_device_->StereoRecording(stereo) == -1) {
    return -1;
  }
  *enabled = stereo;
  RTC_LOG(INFO) << "output: " << stereo;
  return 0;
}

int32_t MuteAudioDeviceModule::StereoPlayoutIsAvailable(bool* available) const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->StereoPlayoutIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  RTC_LOG(INFO) << "output: " << isAvailable;
  return 0;
}

int32_t MuteAudioDeviceModule::SetStereoPlayout(bool enable) {
  RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
  CHECKinitialized_();
  if (audio_device_->PlayoutIsInitialized()) {
    RTC_LOG(LERROR)
        << "unable to set stereo mode while playing side is initialized";
    return -1;
  }
  if (audio_device_->SetStereoPlayout(enable)) {
    RTC_LOG(WARNING) << "stereo playout is not supported";
    return -1;
  }
  int8_t nChannels(1);
  if (enable) {
    nChannels = 2;
  }
  audio_device_buffer_.get()->SetPlayoutChannels(nChannels);
  return 0;
}

int32_t MuteAudioDeviceModule::StereoPlayout(bool* enabled) const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  bool stereo = false;
  if (audio_device_->StereoPlayout(stereo) == -1) {
    return -1;
  }
  *enabled = stereo;
  RTC_LOG(INFO) << "output: " << stereo;
  return 0;
}

int32_t MuteAudioDeviceModule::PlayoutIsAvailable(bool* available) {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->PlayoutIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  RTC_LOG(INFO) << "output: " << isAvailable;
  return 0;
}

int32_t MuteAudioDeviceModule::RecordingIsAvailable(bool* available) {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->RecordingIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  RTC_LOG(INFO) << "output: " << isAvailable;
  return 0;
}

int32_t MuteAudioDeviceModule::MaxMicrophoneVolume(uint32_t* maxVolume) const {
  CHECKinitialized_();
  uint32_t maxVol(0);
  if (audio_device_->MaxMicrophoneVolume(maxVol) == -1) {
    return -1;
  }
  *maxVolume = maxVol;
  return 0;
}

int32_t MuteAudioDeviceModule::MinMicrophoneVolume(uint32_t* minVolume) const {
  CHECKinitialized_();
  uint32_t minVol(0);
  if (audio_device_->MinMicrophoneVolume(minVol) == -1) {
    return -1;
  }
  *minVolume = minVol;
  return 0;
}

int16_t MuteAudioDeviceModule::PlayoutDevices() {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  uint16_t nPlayoutDevices = audio_device_->PlayoutDevices();
  RTC_LOG(INFO) << "output: " << nPlayoutDevices;
  return (int16_t)(nPlayoutDevices);
}

int32_t MuteAudioDeviceModule::SetPlayoutDevice(uint16_t index) {
  RTC_LOG(INFO) << __FUNCTION__ << "(" << index << ")";
  CHECKinitialized_();
  return audio_device_->SetPlayoutDevice(index);
}

int32_t MuteAudioDeviceModule::SetPlayoutDevice(WindowsDeviceType device) {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  return audio_device_->SetPlayoutDevice(device);
}

int32_t MuteAudioDeviceModule::PlayoutDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize]) {
  RTC_LOG(INFO) << __FUNCTION__ << "(" << index << ", ...)";
  CHECKinitialized_();
  if (name == NULL) {
    return -1;
  }
  if (audio_device_->PlayoutDeviceName(index, name, guid) == -1) {
    return -1;
  }
  if (name != NULL) {
    RTC_LOG(INFO) << "output: name = " << name;
  }
  if (guid != NULL) {
    RTC_LOG(INFO) << "output: guid = " << guid;
  }
  return 0;
}

int32_t MuteAudioDeviceModule::RecordingDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize]) {
  RTC_LOG(INFO) << __FUNCTION__ << "(" << index << ", ...)";
  CHECKinitialized_();
  if (name == NULL) {
    return -1;
  }
  if (audio_device_->RecordingDeviceName(index, name, guid) == -1) {
    return -1;
  }
  if (name != NULL) {
    RTC_LOG(INFO) << "output: name = " << name;
  }
  if (guid != NULL) {
    RTC_LOG(INFO) << "output: guid = " << guid;
  }
  return 0;
}

int16_t MuteAudioDeviceModule::RecordingDevices() {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  uint16_t nRecordingDevices = audio_device_->RecordingDevices();
  RTC_LOG(INFO) << "output: " << nRecordingDevices;
  return (int16_t)nRecordingDevices;
}

int32_t MuteAudioDeviceModule::SetRecordingDevice(uint16_t index) {
  RTC_LOG(INFO) << __FUNCTION__ << "(" << index << ")";
  CHECKinitialized_();
  return audio_device_->SetRecordingDevice(index);
}

int32_t MuteAudioDeviceModule::SetRecordingDevice(WindowsDeviceType device) {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  return audio_device_->SetRecordingDevice(device);
}

int32_t MuteAudioDeviceModule::InitPlayout() {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  if (PlayoutIsInitialized()) {
    return 0;
  }
  int32_t result = audio_device_->InitPlayout();
  RTC_LOG(INFO) << "output: " << result;
  return result;
}

int32_t MuteAudioDeviceModule::InitRecording() {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  if (RecordingIsInitialized()) {
    return 0;
  }
  int32_t result = audio_device_->InitRecording();
  RTC_LOG(INFO) << "output: " << result;
  return result;
}

bool MuteAudioDeviceModule::PlayoutIsInitialized() const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized__BOOL();
  return audio_device_->PlayoutIsInitialized();
}

bool MuteAudioDeviceModule::RecordingIsInitialized() const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized__BOOL();
  return audio_device_->RecordingIsInitialized();
}

int32_t MuteAudioDeviceModule::StartPlayout() {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  if (Playing()) {
    return 0;
  }
  audio_device_buffer_.get()->StartPlayout();
  int32_t result = audio_device_->StartPlayout();
  RTC_LOG(INFO) << "output: " << result;
  return result;
}

int32_t MuteAudioDeviceModule::StopPlayout() {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  int32_t result = audio_device_->StopPlayout();
  audio_device_buffer_.get()->StopPlayout();
  RTC_LOG(INFO) << "output: " << result;
  return result;
}

bool MuteAudioDeviceModule::Playing() const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized__BOOL();
  return audio_device_->Playing();
}

int32_t MuteAudioDeviceModule::StartRecording() {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  if (Recording()) {
    return 0;
  }
  audio_device_buffer_.get()->StartRecording();
  int32_t result = audio_device_->StartRecording();
  RTC_LOG(INFO) << "output: " << result;
  return result;
}

int32_t MuteAudioDeviceModule::StopRecording() {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized_();
  int32_t result = audio_device_->StopRecording();
  audio_device_buffer_.get()->StopRecording();
  RTC_LOG(INFO) << "output: " << result;
  return result;
}

bool MuteAudioDeviceModule::Recording() const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized__BOOL();
  return audio_device_->Recording();
}

int32_t MuteAudioDeviceModule::RegisterAudioCallback(
    AudioTransport* audioCallback) {
  RTC_LOG(INFO) << __FUNCTION__;
  return audio_device_buffer_.get()->RegisterAudioCallback(audioCallback);
}

int32_t MuteAudioDeviceModule::PlayoutDelay(uint16_t* delayMS) const {
  CHECKinitialized_();
  uint16_t delay = 0;
  if (audio_device_->PlayoutDelay(delay) == -1) {
    RTC_LOG(LERROR) << "failed to retrieve the playout delay";
    return -1;
  }
  *delayMS = delay;
  return 0;
}

bool MuteAudioDeviceModule::BuiltInAECIsAvailable() const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized__BOOL();
  bool isAvailable = audio_device_->BuiltInAECIsAvailable();
  RTC_LOG(INFO) << "output: " << isAvailable;
  return isAvailable;
}

int32_t MuteAudioDeviceModule::EnableBuiltInAEC(bool enable) {
  RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
  CHECKinitialized_();
  int32_t ok = audio_device_->EnableBuiltInAEC(enable);
  RTC_LOG(INFO) << "output: " << ok;
  return ok;
}

bool MuteAudioDeviceModule::BuiltInAGCIsAvailable() const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized__BOOL();
  bool isAvailable = audio_device_->BuiltInAGCIsAvailable();
  RTC_LOG(INFO) << "output: " << isAvailable;
  return isAvailable;
}

int32_t MuteAudioDeviceModule::EnableBuiltInAGC(bool enable) {
  RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
  CHECKinitialized_();
  int32_t ok = audio_device_->EnableBuiltInAGC(enable);
  RTC_LOG(INFO) << "output: " << ok;
  return ok;
}

bool MuteAudioDeviceModule::BuiltInNSIsAvailable() const {
  RTC_LOG(INFO) << __FUNCTION__;
  CHECKinitialized__BOOL();
  bool isAvailable = audio_device_->BuiltInNSIsAvailable();
  RTC_LOG(INFO) << "output: " << isAvailable;
  return isAvailable;
}

int32_t MuteAudioDeviceModule::EnableBuiltInNS(bool enable) {
  RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
  CHECKinitialized_();
  int32_t ok = audio_device_->EnableBuiltInNS(enable);
  RTC_LOG(INFO) << "output: " << ok;
  return ok;
}

int32_t MuteAudioDeviceModule::GetPlayoutUnderrunCount() const {
  // Don't log here, as this method can be called very often.
  CHECKinitialized_();
  int32_t ok = audio_device_->GetPlayoutUnderrunCount();
  return ok;
}

}  // namespace webrtc
