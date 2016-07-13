/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/audio_device_buffer.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/modules/audio_device/audio_device_config.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {

static const int kHighDelayThresholdMs = 300;
static const int kLogHighDelayIntervalFrames = 500;  // 5 seconds.

AudioDeviceBuffer::AudioDeviceBuffer()
    : _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
      _critSectCb(*CriticalSectionWrapper::CreateCriticalSection()),
      _ptrCbAudioTransport(nullptr),
      _recSampleRate(0),
      _playSampleRate(0),
      _recChannels(0),
      _playChannels(0),
      _recChannel(AudioDeviceModule::kChannelBoth),
      _recBytesPerSample(0),
      _playBytesPerSample(0),
      _recSamples(0),
      _recSize(0),
      _playSamples(0),
      _playSize(0),
      _recFile(*FileWrapper::Create()),
      _playFile(*FileWrapper::Create()),
      _currentMicLevel(0),
      _newMicLevel(0),
      _typingStatus(false),
      _playDelayMS(0),
      _recDelayMS(0),
      _clockDrift(0),
      // Set to the interval in order to log on the first occurrence.
      high_delay_counter_(kLogHighDelayIntervalFrames) {
  LOG(INFO) << "AudioDeviceBuffer::ctor";
  memset(_recBuffer, 0, kMaxBufferSizeBytes);
  memset(_playBuffer, 0, kMaxBufferSizeBytes);
}

AudioDeviceBuffer::~AudioDeviceBuffer() {
  LOG(INFO) << "AudioDeviceBuffer::~dtor";
  {
    CriticalSectionScoped lock(&_critSect);

    _recFile.Flush();
    _recFile.CloseFile();
    delete &_recFile;

    _playFile.Flush();
    _playFile.CloseFile();
    delete &_playFile;
  }

  delete &_critSect;
  delete &_critSectCb;
}

int32_t AudioDeviceBuffer::RegisterAudioCallback(
    AudioTransport* audioCallback) {
  LOG(INFO) << __FUNCTION__;
  CriticalSectionScoped lock(&_critSectCb);
  _ptrCbAudioTransport = audioCallback;
  return 0;
}

int32_t AudioDeviceBuffer::InitPlayout() {
  LOG(INFO) << __FUNCTION__;
  return 0;
}

int32_t AudioDeviceBuffer::InitRecording() {
  LOG(INFO) << __FUNCTION__;
  return 0;
}

int32_t AudioDeviceBuffer::SetRecordingSampleRate(uint32_t fsHz) {
  LOG(INFO) << "SetRecordingSampleRate(" << fsHz << ")";
  CriticalSectionScoped lock(&_critSect);
  _recSampleRate = fsHz;
  return 0;
}

int32_t AudioDeviceBuffer::SetPlayoutSampleRate(uint32_t fsHz) {
  LOG(INFO) << "SetPlayoutSampleRate(" << fsHz << ")";
  CriticalSectionScoped lock(&_critSect);
  _playSampleRate = fsHz;
  return 0;
}

int32_t AudioDeviceBuffer::RecordingSampleRate() const {
  return _recSampleRate;
}

int32_t AudioDeviceBuffer::PlayoutSampleRate() const {
  return _playSampleRate;
}

int32_t AudioDeviceBuffer::SetRecordingChannels(size_t channels) {
  CriticalSectionScoped lock(&_critSect);
  _recChannels = channels;
  _recBytesPerSample =
      2 * channels;  // 16 bits per sample in mono, 32 bits in stereo
  return 0;
}

int32_t AudioDeviceBuffer::SetPlayoutChannels(size_t channels) {
  CriticalSectionScoped lock(&_critSect);
  _playChannels = channels;
  // 16 bits per sample in mono, 32 bits in stereo
  _playBytesPerSample = 2 * channels;
  return 0;
}

int32_t AudioDeviceBuffer::SetRecordingChannel(
    const AudioDeviceModule::ChannelType channel) {
  CriticalSectionScoped lock(&_critSect);

  if (_recChannels == 1) {
    return -1;
  }

  if (channel == AudioDeviceModule::kChannelBoth) {
    // two bytes per channel
    _recBytesPerSample = 4;
  } else {
    // only utilize one out of two possible channels (left or right)
    _recBytesPerSample = 2;
  }
  _recChannel = channel;

  return 0;
}

int32_t AudioDeviceBuffer::RecordingChannel(
    AudioDeviceModule::ChannelType& channel) const {
  channel = _recChannel;
  return 0;
}

size_t AudioDeviceBuffer::RecordingChannels() const {
  return _recChannels;
}

size_t AudioDeviceBuffer::PlayoutChannels() const {
  return _playChannels;
}

int32_t AudioDeviceBuffer::SetCurrentMicLevel(uint32_t level) {
  _currentMicLevel = level;
  return 0;
}

int32_t AudioDeviceBuffer::SetTypingStatus(bool typingStatus) {
  _typingStatus = typingStatus;
  return 0;
}

uint32_t AudioDeviceBuffer::NewMicLevel() const {
  return _newMicLevel;
}

void AudioDeviceBuffer::SetVQEData(int playDelayMs,
                                   int recDelayMs,
                                   int clockDrift) {
  if (high_delay_counter_ < kLogHighDelayIntervalFrames) {
    ++high_delay_counter_;
  } else {
    if (playDelayMs + recDelayMs > kHighDelayThresholdMs) {
      high_delay_counter_ = 0;
      LOG(LS_WARNING) << "High audio device delay reported (render="
                      << playDelayMs << " ms, capture=" << recDelayMs << " ms)";
    }
  }

  _playDelayMS = playDelayMs;
  _recDelayMS = recDelayMs;
  _clockDrift = clockDrift;
}

int32_t AudioDeviceBuffer::StartInputFileRecording(
    const char fileName[kAdmMaxFileNameSize]) {
  CriticalSectionScoped lock(&_critSect);

  _recFile.Flush();
  _recFile.CloseFile();

  return _recFile.OpenFile(fileName, false) ? 0 : -1;
}

int32_t AudioDeviceBuffer::StopInputFileRecording() {
  CriticalSectionScoped lock(&_critSect);

  _recFile.Flush();
  _recFile.CloseFile();

  return 0;
}

int32_t AudioDeviceBuffer::StartOutputFileRecording(
    const char fileName[kAdmMaxFileNameSize]) {
  CriticalSectionScoped lock(&_critSect);

  _playFile.Flush();
  _playFile.CloseFile();

  return _playFile.OpenFile(fileName, false) ? 0 : -1;
}

int32_t AudioDeviceBuffer::StopOutputFileRecording() {
  CriticalSectionScoped lock(&_critSect);

  _playFile.Flush();
  _playFile.CloseFile();

  return 0;
}

int32_t AudioDeviceBuffer::SetRecordedBuffer(const void* audioBuffer,
                                             size_t nSamples) {
  CriticalSectionScoped lock(&_critSect);

  if (_recBytesPerSample == 0) {
    assert(false);
    return -1;
  }

  _recSamples = nSamples;
  _recSize = _recBytesPerSample * nSamples;  // {2,4}*nSamples
  if (_recSize > kMaxBufferSizeBytes) {
    assert(false);
    return -1;
  }

  if (_recChannel == AudioDeviceModule::kChannelBoth) {
    // (default) copy the complete input buffer to the local buffer
    memcpy(&_recBuffer[0], audioBuffer, _recSize);
  } else {
    int16_t* ptr16In = (int16_t*)audioBuffer;
    int16_t* ptr16Out = (int16_t*)&_recBuffer[0];

    if (AudioDeviceModule::kChannelRight == _recChannel) {
      ptr16In++;
    }

    // exctract left or right channel from input buffer to the local buffer
    for (size_t i = 0; i < _recSamples; i++) {
      *ptr16Out = *ptr16In;
      ptr16Out++;
      ptr16In++;
      ptr16In++;
    }
  }

  if (_recFile.is_open()) {
    // write to binary file in mono or stereo (interleaved)
    _recFile.Write(&_recBuffer[0], _recSize);
  }

  return 0;
}

int32_t AudioDeviceBuffer::DeliverRecordedData() {
  CriticalSectionScoped lock(&_critSectCb);
  // Ensure that user has initialized all essential members
  if ((_recSampleRate == 0) || (_recSamples == 0) ||
      (_recBytesPerSample == 0) || (_recChannels == 0)) {
    RTC_NOTREACHED();
    return -1;
  }

  if (!_ptrCbAudioTransport) {
    LOG(LS_WARNING) << "Invalid audio transport";
    return 0;
  }

  int32_t res(0);
  uint32_t newMicLevel(0);
  uint32_t totalDelayMS = _playDelayMS + _recDelayMS;
  res = _ptrCbAudioTransport->RecordedDataIsAvailable(
      &_recBuffer[0], _recSamples, _recBytesPerSample, _recChannels,
      _recSampleRate, totalDelayMS, _clockDrift, _currentMicLevel,
      _typingStatus, newMicLevel);
  if (res != -1) {
    _newMicLevel = newMicLevel;
  }

  return 0;
}

int32_t AudioDeviceBuffer::RequestPlayoutData(size_t nSamples) {
  uint32_t playSampleRate = 0;
  size_t playBytesPerSample = 0;
  size_t playChannels = 0;

  // TOOD(henrika): improve bad locking model and make it more clear that only
  // 10ms buffer sizes is supported in WebRTC.
  {
    CriticalSectionScoped lock(&_critSect);

    // Store copies under lock and use copies hereafter to avoid race with
    // setter methods.
    playSampleRate = _playSampleRate;
    playBytesPerSample = _playBytesPerSample;
    playChannels = _playChannels;

    // Ensure that user has initialized all essential members
    if ((playBytesPerSample == 0) || (playChannels == 0) ||
        (playSampleRate == 0)) {
      RTC_NOTREACHED();
      return -1;
    }

    _playSamples = nSamples;
    _playSize = playBytesPerSample * nSamples;  // {2,4}*nSamples
    RTC_CHECK_LE(_playSize, kMaxBufferSizeBytes);
    RTC_CHECK_EQ(nSamples, _playSamples);
  }

  size_t nSamplesOut(0);

  CriticalSectionScoped lock(&_critSectCb);

  // It is currently supported to start playout without a valid audio
  // transport object. Leads to warning and silence.
  if (!_ptrCbAudioTransport) {
    LOG(LS_WARNING) << "Invalid audio transport";
    return 0;
  }

  uint32_t res(0);
  int64_t elapsed_time_ms = -1;
  int64_t ntp_time_ms = -1;
  res = _ptrCbAudioTransport->NeedMorePlayData(
      _playSamples, playBytesPerSample, playChannels, playSampleRate,
      &_playBuffer[0], nSamplesOut, &elapsed_time_ms, &ntp_time_ms);
  if (res != 0) {
    LOG(LS_ERROR) << "NeedMorePlayData() failed";
  }

  return static_cast<int32_t>(nSamplesOut);
}

int32_t AudioDeviceBuffer::GetPlayoutData(void* audioBuffer) {
  CriticalSectionScoped lock(&_critSect);
  RTC_CHECK_LE(_playSize, kMaxBufferSizeBytes);

  memcpy(audioBuffer, &_playBuffer[0], _playSize);

  if (_playFile.is_open()) {
    // write to binary file in mono or stereo (interleaved)
    _playFile.Write(&_playBuffer[0], _playSize);
  }

  return static_cast<int32_t>(_playSamples);
}

}  // namespace webrtc
