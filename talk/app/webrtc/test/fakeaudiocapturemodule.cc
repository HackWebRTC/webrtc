/*
 * libjingle
 * Copyright 2012, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/app/webrtc/test/fakeaudiocapturemodule.h"

#include "talk/base/common.h"
#include "talk/base/refcount.h"
#include "talk/base/thread.h"
#include "talk/base/timeutils.h"

// Audio sample value that is high enough that it doesn't occur naturally when
// frames are being faked. E.g. NetEq will not generate this large sample value
// unless it has received an audio frame containing a sample of this value.
// Even simpler buffers would likely just contain audio sample values of 0.
static const int kHighSampleValue = 10000;

// Same value as src/modules/audio_device/main/source/audio_device_config.h in
// https://code.google.com/p/webrtc/
static const uint32 kAdmMaxIdleTimeProcess = 1000;

// Constants here are derived by running VoE using a real ADM.
// The constants correspond to 10ms of mono audio at 44kHz.
static const int kTimePerFrameMs = 10;
static const int kNumberOfChannels = 1;
static const int kSamplesPerSecond = 44000;
static const int kTotalDelayMs = 0;
static const int kClockDriftMs = 0;
static const uint32_t kMaxVolume = 14392;

enum {
  MSG_RUN_PROCESS,
  MSG_STOP_PROCESS,
};

FakeAudioCaptureModule::FakeAudioCaptureModule(
    talk_base::Thread* process_thread)
    : last_process_time_ms_(0),
      audio_callback_(NULL),
      recording_(false),
      playing_(false),
      play_is_initialized_(false),
      rec_is_initialized_(false),
      current_mic_level_(kMaxVolume),
      started_(false),
      next_frame_time_(0),
      process_thread_(process_thread),
      frames_received_(0) {
}

FakeAudioCaptureModule::~FakeAudioCaptureModule() {
  // Ensure that thread stops calling ProcessFrame().
  process_thread_->Send(this, MSG_STOP_PROCESS);
}

talk_base::scoped_refptr<FakeAudioCaptureModule> FakeAudioCaptureModule::Create(
    talk_base::Thread* process_thread) {
  if (process_thread == NULL) return NULL;

  talk_base::scoped_refptr<FakeAudioCaptureModule> capture_module(
      new talk_base::RefCountedObject<FakeAudioCaptureModule>(process_thread));
  if (!capture_module->Initialize()) {
    return NULL;
  }
  return capture_module;
}

int FakeAudioCaptureModule::frames_received() const {
  return frames_received_;
}

int32_t FakeAudioCaptureModule::Version(char* /*version*/,
                                        uint32_t& /*remaining_buffer_in_bytes*/,
                                        uint32_t& /*position*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::TimeUntilNextProcess() {
  const uint32 current_time = talk_base::Time();
  if (current_time < last_process_time_ms_) {
    // TODO: wraparound could be handled more gracefully.
    return 0;
  }
  const uint32 elapsed_time = current_time - last_process_time_ms_;
  if (kAdmMaxIdleTimeProcess < elapsed_time) {
    return 0;
  }
  return kAdmMaxIdleTimeProcess - elapsed_time;
}

int32_t FakeAudioCaptureModule::Process() {
  last_process_time_ms_ = talk_base::Time();
  return 0;
}

int32_t FakeAudioCaptureModule::ChangeUniqueId(const int32_t /*id*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::ActiveAudioLayer(
    AudioLayer* /*audio_layer*/) const {
  ASSERT(false);
  return 0;
}

webrtc::AudioDeviceModule::ErrorCode FakeAudioCaptureModule::LastError() const {
  ASSERT(false);
  return webrtc::AudioDeviceModule::kAdmErrNone;
}

int32_t FakeAudioCaptureModule::RegisterEventObserver(
    webrtc::AudioDeviceObserver* /*event_callback*/) {
  // Only used to report warnings and errors. This fake implementation won't
  // generate any so discard this callback.
  return 0;
}

int32_t FakeAudioCaptureModule::RegisterAudioCallback(
    webrtc::AudioTransport* audio_callback) {
  audio_callback_ = audio_callback;
  return 0;
}

int32_t FakeAudioCaptureModule::Init() {
  // Initialize is called by the factory method. Safe to ignore this Init call.
  return 0;
}

int32_t FakeAudioCaptureModule::Terminate() {
  // Clean up in the destructor. No action here, just success.
  return 0;
}

bool FakeAudioCaptureModule::Initialized() const {
  ASSERT(false);
  return 0;
}

int16_t FakeAudioCaptureModule::PlayoutDevices() {
  ASSERT(false);
  return 0;
}

int16_t FakeAudioCaptureModule::RecordingDevices() {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::PlayoutDeviceName(
    uint16_t /*index*/,
    char /*name*/[webrtc::kAdmMaxDeviceNameSize],
    char /*guid*/[webrtc::kAdmMaxGuidSize]) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::RecordingDeviceName(
    uint16_t /*index*/,
    char /*name*/[webrtc::kAdmMaxDeviceNameSize],
    char /*guid*/[webrtc::kAdmMaxGuidSize]) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SetPlayoutDevice(uint16_t /*index*/) {
  // No playout device, just playing from file. Return success.
  return 0;
}

int32_t FakeAudioCaptureModule::SetPlayoutDevice(WindowsDeviceType /*device*/) {
  if (play_is_initialized_) {
    return -1;
  }
  return 0;
}

int32_t FakeAudioCaptureModule::SetRecordingDevice(uint16_t /*index*/) {
  // No recording device, just dropping audio. Return success.
  return 0;
}

int32_t FakeAudioCaptureModule::SetRecordingDevice(
    WindowsDeviceType /*device*/) {
  if (rec_is_initialized_) {
    return -1;
  }
  return 0;
}

int32_t FakeAudioCaptureModule::PlayoutIsAvailable(bool* /*available*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::InitPlayout() {
  play_is_initialized_ = true;
  return 0;
}

bool FakeAudioCaptureModule::PlayoutIsInitialized() const {
  return play_is_initialized_;
}

int32_t FakeAudioCaptureModule::RecordingIsAvailable(bool* /*available*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::InitRecording() {
  rec_is_initialized_ = true;
  return 0;
}

bool FakeAudioCaptureModule::RecordingIsInitialized() const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::StartPlayout() {
  if (!play_is_initialized_) {
    return -1;
  }
  playing_ = true;
  UpdateProcessing();
  return 0;
}

int32_t FakeAudioCaptureModule::StopPlayout() {
  playing_ = false;
  UpdateProcessing();
  return 0;
}

bool FakeAudioCaptureModule::Playing() const {
  return playing_;
}

int32_t FakeAudioCaptureModule::StartRecording() {
  if (!rec_is_initialized_) {
    return -1;
  }
  recording_ = true;
  UpdateProcessing();
  return 0;
}

int32_t FakeAudioCaptureModule::StopRecording() {
  recording_ = false;
  UpdateProcessing();
  return 0;
}

bool FakeAudioCaptureModule::Recording() const {
  return recording_;
}

int32_t FakeAudioCaptureModule::SetAGC(bool /*enable*/) {
  // No AGC but not needed since audio is pregenerated. Return success.
  return 0;
}

bool FakeAudioCaptureModule::AGC() const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SetWaveOutVolume(uint16_t /*volume_left*/,
                                                 uint16_t /*volume_right*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::WaveOutVolume(
    uint16_t* /*volume_left*/,
    uint16_t* /*volume_right*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SpeakerIsAvailable(bool* available) {
  // No speaker, just dropping audio. Return success.
  *available = true;
  return 0;
}

int32_t FakeAudioCaptureModule::InitSpeaker() {
  // No speaker, just playing from file. Return success.
  return 0;
}

bool FakeAudioCaptureModule::SpeakerIsInitialized() const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::MicrophoneIsAvailable(bool* available) {
  // No microphone, just playing from file. Return success.
  *available = true;
  return 0;
}

int32_t FakeAudioCaptureModule::InitMicrophone() {
  // No microphone, just playing from file. Return success.
  return 0;
}

bool FakeAudioCaptureModule::MicrophoneIsInitialized() const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SpeakerVolumeIsAvailable(bool* /*available*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SetSpeakerVolume(uint32_t /*volume*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SpeakerVolume(uint32_t* /*volume*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::MaxSpeakerVolume(
    uint32_t* /*max_volume*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::MinSpeakerVolume(
    uint32_t* /*min_volume*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SpeakerVolumeStepSize(
    uint16_t* /*step_size*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::MicrophoneVolumeIsAvailable(
    bool* /*available*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SetMicrophoneVolume(uint32_t /*volume*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::MicrophoneVolume(uint32_t* volume) const {
  *volume = current_mic_level_;
  return 0;
}

int32_t FakeAudioCaptureModule::MaxMicrophoneVolume(
    uint32_t* max_volume) const {
  *max_volume = kMaxVolume;
  return 0;
}

int32_t FakeAudioCaptureModule::MinMicrophoneVolume(
    uint32_t* /*min_volume*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::MicrophoneVolumeStepSize(
    uint16_t* /*step_size*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SpeakerMuteIsAvailable(bool* /*available*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SetSpeakerMute(bool /*enable*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SpeakerMute(bool* /*enabled*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::MicrophoneMuteIsAvailable(bool* /*available*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SetMicrophoneMute(bool /*enable*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::MicrophoneMute(bool* /*enabled*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::MicrophoneBoostIsAvailable(
    bool* /*available*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SetMicrophoneBoost(bool /*enable*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::MicrophoneBoost(bool* /*enabled*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::StereoPlayoutIsAvailable(
    bool* available) const {
  // No recording device, just dropping audio. Stereo can be dropped just
  // as easily as mono.
  *available = true;
  return 0;
}

int32_t FakeAudioCaptureModule::SetStereoPlayout(bool /*enable*/) {
  // No recording device, just dropping audio. Stereo can be dropped just
  // as easily as mono.
  return 0;
}

int32_t FakeAudioCaptureModule::StereoPlayout(bool* /*enabled*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::StereoRecordingIsAvailable(
    bool* available) const {
  // Keep thing simple. No stereo recording.
  *available = false;
  return 0;
}

int32_t FakeAudioCaptureModule::SetStereoRecording(bool enable) {
  if (!enable) {
    return 0;
  }
  return -1;
}

int32_t FakeAudioCaptureModule::StereoRecording(bool* /*enabled*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SetRecordingChannel(
    const ChannelType channel) {
  if (channel != AudioDeviceModule::kChannelBoth) {
    // There is no right or left in mono. I.e. kChannelBoth should be used for
    // mono.
    ASSERT(false);
    return -1;
  }
  return 0;
}

int32_t FakeAudioCaptureModule::RecordingChannel(ChannelType* channel) const {
  // Stereo recording not supported. However, WebRTC ADM returns kChannelBoth
  // in that case. Do the same here.
  *channel = AudioDeviceModule::kChannelBoth;
  return 0;
}

int32_t FakeAudioCaptureModule::SetPlayoutBuffer(const BufferType /*type*/,
                                                 uint16_t /*size_ms*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::PlayoutBuffer(BufferType* /*type*/,
                                              uint16_t* /*size_ms*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::PlayoutDelay(uint16_t* delay_ms) const {
  // No delay since audio frames are dropped.
  *delay_ms = 0;
  return 0;
}

int32_t FakeAudioCaptureModule::RecordingDelay(uint16_t* /*delay_ms*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::CPULoad(uint16_t* /*load*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::StartRawOutputFileRecording(
    const char /*pcm_file_name_utf8*/[webrtc::kAdmMaxFileNameSize]) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::StopRawOutputFileRecording() {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::StartRawInputFileRecording(
    const char /*pcm_file_name_utf8*/[webrtc::kAdmMaxFileNameSize]) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::StopRawInputFileRecording() {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SetRecordingSampleRate(
    const uint32_t /*samples_per_sec*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::RecordingSampleRate(
    uint32_t* /*samples_per_sec*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SetPlayoutSampleRate(
    const uint32_t /*samples_per_sec*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::PlayoutSampleRate(
    uint32_t* /*samples_per_sec*/) const {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::ResetAudioDevice() {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::SetLoudspeakerStatus(bool /*enable*/) {
  ASSERT(false);
  return 0;
}

int32_t FakeAudioCaptureModule::GetLoudspeakerStatus(bool* /*enabled*/) const {
  ASSERT(false);
  return 0;
}

void FakeAudioCaptureModule::OnMessage(talk_base::Message* msg) {
  switch (msg->message_id) {
    case MSG_RUN_PROCESS:
      ProcessFrameP();
      break;
    case MSG_STOP_PROCESS:
      StopProcessP();
      break;
    default:
      // All existing messages should be caught. Getting here should never
      // happen.
      ASSERT(false);
  }
}

bool FakeAudioCaptureModule::Initialize() {
  // Set the send buffer samples high enough that it would not occur on the
  // remote side unless a packet containing a sample of that magnitude has been
  // sent to it. Note that the audio processing pipeline will likely distort the
  // original signal.
  SetSendBuffer(kHighSampleValue);
  last_process_time_ms_ = talk_base::Time();
  return true;
}

void FakeAudioCaptureModule::SetSendBuffer(int value) {
  Sample* buffer_ptr = reinterpret_cast<Sample*>(send_buffer_);
  const int buffer_size_in_samples = sizeof(send_buffer_) /
      kNumberBytesPerSample;
  for (int i = 0; i < buffer_size_in_samples; ++i) {
    buffer_ptr[i] = value;
  }
}

void FakeAudioCaptureModule::ResetRecBuffer() {
  memset(rec_buffer_, 0, sizeof(rec_buffer_));
}

bool FakeAudioCaptureModule::CheckRecBuffer(int value) {
  const Sample* buffer_ptr = reinterpret_cast<const Sample*>(rec_buffer_);
  const int buffer_size_in_samples = sizeof(rec_buffer_) /
      kNumberBytesPerSample;
  for (int i = 0; i < buffer_size_in_samples; ++i) {
    if (buffer_ptr[i] >= value) return true;
  }
  return false;
}

void FakeAudioCaptureModule::UpdateProcessing() {
  const bool process = recording_ || playing_;
  if (process) {
    if (started_) {
      // Already started.
      return;
    }
    process_thread_->Post(this, MSG_RUN_PROCESS);
  } else {
    process_thread_->Send(this, MSG_STOP_PROCESS);
  }
}

void FakeAudioCaptureModule::ProcessFrameP() {
  ASSERT(talk_base::Thread::Current() == process_thread_);
  if (!started_) {
    next_frame_time_ = talk_base::Time();
    started_ = true;
  }
  // Receive and send frames every kTimePerFrameMs.
  if (audio_callback_ != NULL) {
    if (playing_) {
      ReceiveFrameP();
    }
    if (recording_) {
      SendFrameP();
    }
  }

  next_frame_time_ += kTimePerFrameMs;
  const uint32 current_time = talk_base::Time();
  const uint32 wait_time = (next_frame_time_ > current_time) ?
      next_frame_time_ - current_time : 0;
  process_thread_->PostDelayed(wait_time, this, MSG_RUN_PROCESS);
}

void FakeAudioCaptureModule::ReceiveFrameP() {
  ASSERT(talk_base::Thread::Current() == process_thread_);
  ResetRecBuffer();
  uint32_t nSamplesOut = 0;
  if (audio_callback_->NeedMorePlayData(kNumberSamples, kNumberBytesPerSample,
                                       kNumberOfChannels, kSamplesPerSecond,
                                       rec_buffer_, nSamplesOut) != 0) {
    ASSERT(false);
  }
  ASSERT(nSamplesOut == kNumberSamples);
  // The SetBuffer() function ensures that after decoding, the audio buffer
  // should contain samples of similar magnitude (there is likely to be some
  // distortion due to the audio pipeline). If one sample is detected to
  // have the same or greater magnitude somewhere in the frame, an actual frame
  // has been received from the remote side (i.e. faked frames are not being
  // pulled).
  if (CheckRecBuffer(kHighSampleValue)) ++frames_received_;
}

void FakeAudioCaptureModule::SendFrameP() {
  ASSERT(talk_base::Thread::Current() == process_thread_);
  bool key_pressed = false;
  if (audio_callback_->RecordedDataIsAvailable(send_buffer_, kNumberSamples,
                                              kNumberBytesPerSample,
                                              kNumberOfChannels,
                                              kSamplesPerSecond, kTotalDelayMs,
                                              kClockDriftMs, current_mic_level_,
                                              key_pressed,
                                              current_mic_level_) != 0) {
    ASSERT(false);
  }
}

void FakeAudioCaptureModule::StopProcessP() {
  ASSERT(talk_base::Thread::Current() == process_thread_);
  started_ = false;
  process_thread_->Clear(this);
}
