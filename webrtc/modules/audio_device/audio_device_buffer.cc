/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "webrtc/modules/audio_device/audio_device_buffer.h"

#include "webrtc/base/arraysize.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/audio_device/audio_device_config.h"

namespace webrtc {

static const int kHighDelayThresholdMs = 300;
static const int kLogHighDelayIntervalFrames = 500;  // 5 seconds.

static const char kTimerQueueName[] = "AudioDeviceBufferTimer";

// Time between two sucessive calls to LogStats().
static const size_t kTimerIntervalInSeconds = 10;
static const size_t kTimerIntervalInMilliseconds =
    kTimerIntervalInSeconds * rtc::kNumMillisecsPerSec;

AudioDeviceBuffer::AudioDeviceBuffer()
    : _ptrCbAudioTransport(nullptr),
      task_queue_(kTimerQueueName),
      timer_has_started_(false),
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
      high_delay_counter_(kLogHighDelayIntervalFrames),
      num_stat_reports_(0),
      rec_callbacks_(0),
      last_rec_callbacks_(0),
      play_callbacks_(0),
      last_play_callbacks_(0),
      rec_samples_(0),
      last_rec_samples_(0),
      play_samples_(0),
      last_play_samples_(0),
      last_log_stat_time_(0) {
  LOG(INFO) << "AudioDeviceBuffer::ctor";
  memset(_recBuffer, 0, kMaxBufferSizeBytes);
  memset(_playBuffer, 0, kMaxBufferSizeBytes);
}

AudioDeviceBuffer::~AudioDeviceBuffer() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  LOG(INFO) << "AudioDeviceBuffer::~dtor";

  size_t total_diff_time = 0;
  int num_measurements = 0;
  LOG(INFO) << "[playout diff time => #measurements]";
  for (size_t diff = 0; diff < arraysize(playout_diff_times_); ++diff) {
    uint32_t num_elements = playout_diff_times_[diff];
    if (num_elements > 0) {
      total_diff_time += num_elements * diff;
      num_measurements += num_elements;
      LOG(INFO) << "[" << diff << " => " << num_elements << "]";
    }
  }
  if (num_measurements > 0) {
    LOG(INFO) << "total_diff_time: " << total_diff_time;
    LOG(INFO) << "num_measurements: " << num_measurements;
    LOG(INFO) << "average: "
             << static_cast<float>(total_diff_time) / num_measurements;
  }

  _recFile.Flush();
  _recFile.CloseFile();
  delete &_recFile;

  _playFile.Flush();
  _playFile.CloseFile();
  delete &_playFile;
}

int32_t AudioDeviceBuffer::RegisterAudioCallback(
    AudioTransport* audioCallback) {
  LOG(INFO) << __FUNCTION__;
  rtc::CritScope lock(&_critSectCb);
  _ptrCbAudioTransport = audioCallback;
  return 0;
}

int32_t AudioDeviceBuffer::InitPlayout() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  LOG(INFO) << __FUNCTION__;
  last_playout_time_ = rtc::TimeMillis();
  if (!timer_has_started_) {
    StartTimer();
    timer_has_started_ = true;
  }
  return 0;
}

int32_t AudioDeviceBuffer::InitRecording() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  LOG(INFO) << __FUNCTION__;
  if (!timer_has_started_) {
    StartTimer();
    timer_has_started_ = true;
  }
  return 0;
}

int32_t AudioDeviceBuffer::SetRecordingSampleRate(uint32_t fsHz) {
  LOG(INFO) << "SetRecordingSampleRate(" << fsHz << ")";
  rtc::CritScope lock(&_critSect);
  _recSampleRate = fsHz;
  return 0;
}

int32_t AudioDeviceBuffer::SetPlayoutSampleRate(uint32_t fsHz) {
  LOG(INFO) << "SetPlayoutSampleRate(" << fsHz << ")";
  rtc::CritScope lock(&_critSect);
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
  rtc::CritScope lock(&_critSect);
  _recChannels = channels;
  _recBytesPerSample =
      2 * channels;  // 16 bits per sample in mono, 32 bits in stereo
  return 0;
}

int32_t AudioDeviceBuffer::SetPlayoutChannels(size_t channels) {
  rtc::CritScope lock(&_critSect);
  _playChannels = channels;
  // 16 bits per sample in mono, 32 bits in stereo
  _playBytesPerSample = 2 * channels;
  return 0;
}

int32_t AudioDeviceBuffer::SetRecordingChannel(
    const AudioDeviceModule::ChannelType channel) {
  rtc::CritScope lock(&_critSect);

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
  rtc::CritScope lock(&_critSect);

  _recFile.Flush();
  _recFile.CloseFile();

  return _recFile.OpenFile(fileName, false) ? 0 : -1;
}

int32_t AudioDeviceBuffer::StopInputFileRecording() {
  rtc::CritScope lock(&_critSect);

  _recFile.Flush();
  _recFile.CloseFile();

  return 0;
}

int32_t AudioDeviceBuffer::StartOutputFileRecording(
    const char fileName[kAdmMaxFileNameSize]) {
  rtc::CritScope lock(&_critSect);

  _playFile.Flush();
  _playFile.CloseFile();

  return _playFile.OpenFile(fileName, false) ? 0 : -1;
}

int32_t AudioDeviceBuffer::StopOutputFileRecording() {
  rtc::CritScope lock(&_critSect);

  _playFile.Flush();
  _playFile.CloseFile();

  return 0;
}

int32_t AudioDeviceBuffer::SetRecordedBuffer(const void* audioBuffer,
                                             size_t nSamples) {
  rtc::CritScope lock(&_critSect);

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

  // Update some stats but do it on the task queue to ensure that the members
  // are modified and read on the same thread.
  task_queue_.PostTask(
      rtc::Bind(&AudioDeviceBuffer::UpdateRecStats, this, nSamples));

  return 0;
}

int32_t AudioDeviceBuffer::DeliverRecordedData() {
  rtc::CritScope lock(&_critSectCb);
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

  // Measure time since last function call and update an array where the
  // position/index corresponds to time differences (in milliseconds) between
  // two successive playout callbacks, and the stored value is the number of
  // times a given time difference was found.
  int64_t now_time = rtc::TimeMillis();
  size_t diff_time = rtc::TimeDiff(now_time, last_playout_time_);
  // Truncate at 500ms to limit the size of the array.
  diff_time = std::min(kMaxDeltaTimeInMs, diff_time);
  last_playout_time_ = now_time;
  playout_diff_times_[diff_time]++;

  // TOOD(henrika): improve bad locking model and make it more clear that only
  // 10ms buffer sizes is supported in WebRTC.
  {
    rtc::CritScope lock(&_critSect);

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

  rtc::CritScope lock(&_critSectCb);

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

  // Update some stats but do it on the task queue to ensure that access of
  // members is serialized hence avoiding usage of locks.
  task_queue_.PostTask(
      rtc::Bind(&AudioDeviceBuffer::UpdatePlayStats, this, nSamplesOut));

  return static_cast<int32_t>(nSamplesOut);
}

int32_t AudioDeviceBuffer::GetPlayoutData(void* audioBuffer) {
  rtc::CritScope lock(&_critSect);
  RTC_CHECK_LE(_playSize, kMaxBufferSizeBytes);

  memcpy(audioBuffer, &_playBuffer[0], _playSize);

  if (_playFile.is_open()) {
    // write to binary file in mono or stereo (interleaved)
    _playFile.Write(&_playBuffer[0], _playSize);
  }

  return static_cast<int32_t>(_playSamples);
}

void AudioDeviceBuffer::StartTimer() {
  last_log_stat_time_ = rtc::TimeMillis();
  task_queue_.PostDelayedTask(rtc::Bind(&AudioDeviceBuffer::LogStats, this),
                              kTimerIntervalInMilliseconds);
}

void AudioDeviceBuffer::LogStats() {
  RTC_DCHECK(task_queue_.IsCurrent());

  int64_t now_time = rtc::TimeMillis();
  int64_t next_callback_time = now_time + kTimerIntervalInMilliseconds;
  int64_t time_since_last = rtc::TimeDiff(now_time, last_log_stat_time_);
  last_log_stat_time_ = now_time;

  // Log the latest statistics but skip the first 10 seconds since we are not
  // sure of the exact starting point. I.e., the first log printout will be
  // after ~20 seconds.
  if (++num_stat_reports_ > 1) {
    uint32_t diff_samples = rec_samples_ - last_rec_samples_;
    uint32_t rate = diff_samples / kTimerIntervalInSeconds;
    LOG(INFO) << "[REC : " << time_since_last << "msec, "
              << _recSampleRate / 1000
              << "kHz] callbacks: " << rec_callbacks_ - last_rec_callbacks_
              << ", "
              << "samples: " << diff_samples << ", "
              << "rate: " << rate;

    diff_samples = play_samples_ - last_play_samples_;
    rate = diff_samples / kTimerIntervalInSeconds;
    LOG(INFO) << "[PLAY: " << time_since_last << "msec, "
              << _playSampleRate / 1000
              << "kHz] callbacks: " << play_callbacks_ - last_play_callbacks_
              << ", "
              << "samples: " << diff_samples << ", "
              << "rate: " << rate;
  }

  last_rec_callbacks_ = rec_callbacks_;
  last_play_callbacks_ = play_callbacks_;
  last_rec_samples_ = rec_samples_;
  last_play_samples_ = play_samples_;

  int64_t time_to_wait_ms = next_callback_time - rtc::TimeMillis();
  RTC_DCHECK_GT(time_to_wait_ms, 0) << "Invalid timer interval";

  // Update some stats but do it on the task queue to ensure that access of
  // members is serialized hence avoiding usage of locks.
  task_queue_.PostDelayedTask(rtc::Bind(&AudioDeviceBuffer::LogStats, this),
                              time_to_wait_ms);
}

void AudioDeviceBuffer::UpdateRecStats(size_t num_samples) {
  RTC_DCHECK(task_queue_.IsCurrent());
  ++rec_callbacks_;
  rec_samples_ += num_samples;
}

void AudioDeviceBuffer::UpdatePlayStats(size_t num_samples) {
  RTC_DCHECK(task_queue_.IsCurrent());
  ++play_callbacks_;
  play_samples_ += num_samples;
}

}  // namespace webrtc
