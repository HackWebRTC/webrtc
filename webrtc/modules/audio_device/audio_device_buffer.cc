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
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_device/audio_device_config.h"
#include "webrtc/system_wrappers/include/metrics.h"

namespace webrtc {

static const char kTimerQueueName[] = "AudioDeviceBufferTimer";

// Time between two sucessive calls to LogStats().
static const size_t kTimerIntervalInSeconds = 10;
static const size_t kTimerIntervalInMilliseconds =
    kTimerIntervalInSeconds * rtc::kNumMillisecsPerSec;

AudioDeviceBuffer::AudioDeviceBuffer()
    : audio_transport_cb_(nullptr),
      task_queue_(kTimerQueueName),
      timer_has_started_(false),
      rec_sample_rate_(0),
      play_sample_rate_(0),
      rec_channels_(0),
      play_channels_(0),
      rec_bytes_per_sample_(0),
      play_bytes_per_sample_(0),
      current_mic_level_(0),
      new_mic_level_(0),
      typing_status_(false),
      play_delay_ms_(0),
      rec_delay_ms_(0),
      clock_drift_(0),
      num_stat_reports_(0),
      rec_callbacks_(0),
      last_rec_callbacks_(0),
      play_callbacks_(0),
      last_play_callbacks_(0),
      rec_samples_(0),
      last_rec_samples_(0),
      play_samples_(0),
      last_play_samples_(0),
      last_log_stat_time_(0),
      max_rec_level_(0),
      max_play_level_(0),
      num_rec_level_is_zero_(0),
      rec_stat_count_(0),
      play_stat_count_(0) {
  LOG(INFO) << "AudioDeviceBuffer::ctor";
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

  // Add UMA histogram to keep track of the case when only zeros have been
  // recorded. Ensure that recording callbacks have started and that at least
  // one timer event has been able to update |num_rec_level_is_zero_|.
  // I am avoiding use of the task queue here since we are under destruction
  // and reading these members on the creating thread feels safe.
  if (rec_callbacks_ > 0 && num_stat_reports_ > 0) {
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.RecordedOnlyZeros",
    static_cast<int>(num_stat_reports_ == num_rec_level_is_zero_));
  }
}

int32_t AudioDeviceBuffer::RegisterAudioCallback(
    AudioTransport* audio_callback) {
  LOG(INFO) << __FUNCTION__;
  rtc::CritScope lock(&lock_cb_);
  audio_transport_cb_ = audio_callback;
  return 0;
}

int32_t AudioDeviceBuffer::InitPlayout() {
  LOG(INFO) << __FUNCTION__;
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  ResetPlayStats();
  if (!timer_has_started_) {
    StartTimer();
    timer_has_started_ = true;
  }
  return 0;
}

int32_t AudioDeviceBuffer::InitRecording() {
  LOG(INFO) << __FUNCTION__;
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  ResetRecStats();
  if (!timer_has_started_) {
    StartTimer();
    timer_has_started_ = true;
  }
  return 0;
}

int32_t AudioDeviceBuffer::SetRecordingSampleRate(uint32_t fsHz) {
  LOG(INFO) << "SetRecordingSampleRate(" << fsHz << ")";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  rec_sample_rate_ = fsHz;
  return 0;
}

int32_t AudioDeviceBuffer::SetPlayoutSampleRate(uint32_t fsHz) {
  LOG(INFO) << "SetPlayoutSampleRate(" << fsHz << ")";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  play_sample_rate_ = fsHz;
  return 0;
}

int32_t AudioDeviceBuffer::RecordingSampleRate() const {
  return rec_sample_rate_;
}

int32_t AudioDeviceBuffer::PlayoutSampleRate() const {
  return play_sample_rate_;
}

int32_t AudioDeviceBuffer::SetRecordingChannels(size_t channels) {
  LOG(INFO) << "SetRecordingChannels(" << channels << ")";
  rtc::CritScope lock(&lock_);
  rec_channels_ = channels;
  rec_bytes_per_sample_ = sizeof(int16_t) * channels;
  return 0;
}

int32_t AudioDeviceBuffer::SetPlayoutChannels(size_t channels) {
  LOG(INFO) << "SetPlayoutChannels(" << channels << ")";
  rtc::CritScope lock(&lock_);
  play_channels_ = channels;
  play_bytes_per_sample_ = sizeof(int16_t) * channels;
  return 0;
}

int32_t AudioDeviceBuffer::SetRecordingChannel(
    const AudioDeviceModule::ChannelType channel) {
  LOG(INFO) << "SetRecordingChannel(" << channel << ")";
  LOG(LS_WARNING) << "Not implemented";
  // Add DCHECK to ensure that user does not try to use this API with a non-
  // default parameter.
  RTC_DCHECK_EQ(channel, AudioDeviceModule::kChannelBoth);
  return -1;
}

int32_t AudioDeviceBuffer::RecordingChannel(
    AudioDeviceModule::ChannelType& channel) const {
  LOG(LS_WARNING) << "Not implemented";
  return -1;
}

size_t AudioDeviceBuffer::RecordingChannels() const {
  return rec_channels_;
}

size_t AudioDeviceBuffer::PlayoutChannels() const {
  return play_channels_;
}

int32_t AudioDeviceBuffer::SetCurrentMicLevel(uint32_t level) {
  current_mic_level_ = level;
  return 0;
}

int32_t AudioDeviceBuffer::SetTypingStatus(bool typing_status) {
  typing_status_ = typing_status;
  return 0;
}

uint32_t AudioDeviceBuffer::NewMicLevel() const {
  return new_mic_level_;
}

void AudioDeviceBuffer::SetVQEData(int play_delay_ms,
                                   int rec_delay_ms,
                                   int clock_drift) {
  play_delay_ms_ = play_delay_ms;
  rec_delay_ms_ = rec_delay_ms;
  clock_drift_ = clock_drift;
}

int32_t AudioDeviceBuffer::StartInputFileRecording(
    const char fileName[kAdmMaxFileNameSize]) {
  LOG(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceBuffer::StopInputFileRecording() {
  LOG(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceBuffer::StartOutputFileRecording(
    const char fileName[kAdmMaxFileNameSize]) {
  LOG(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceBuffer::StopOutputFileRecording() {
  LOG(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceBuffer::SetRecordedBuffer(const void* audio_buffer,
                                             size_t num_samples) {
  const size_t rec_channels = [&] {
    rtc::CritScope lock(&lock_);
    return rec_channels_;
  }();
  // Copy the complete input buffer to the local buffer.
  const size_t size_in_bytes = num_samples * rec_channels * sizeof(int16_t);
  const size_t old_size = rec_buffer_.size();
  rec_buffer_.SetData(static_cast<const uint8_t*>(audio_buffer), size_in_bytes);
  // Keep track of the size of the recording buffer. Only updated when the
  // size changes, which is a rare event.
  if (old_size != rec_buffer_.size()) {
    LOG(LS_INFO) << "Size of recording buffer: " << rec_buffer_.size();
  }
  // Derive a new level value twice per second.
  int16_t max_abs = 0;
  RTC_DCHECK_LT(rec_stat_count_, 50);
  if (++rec_stat_count_ >= 50) {
    const size_t size = num_samples * rec_channels;
    // Returns the largest absolute value in a signed 16-bit vector.
    max_abs = WebRtcSpl_MaxAbsValueW16(
        reinterpret_cast<const int16_t*>(rec_buffer_.data()), size);
    rec_stat_count_ = 0;
  }
  // Update some stats but do it on the task queue to ensure that the members
  // are modified and read on the same thread. Note that |max_abs| will be
  // zero in most calls and then have no effect of the stats. It is only updated
  // approximately two times per second and can then change the stats.
  task_queue_.PostTask(rtc::Bind(&AudioDeviceBuffer::UpdateRecStats, this,
                                 max_abs, num_samples));
  return 0;
}

int32_t AudioDeviceBuffer::DeliverRecordedData() {
  rtc::CritScope lock(&lock_cb_);
  if (!audio_transport_cb_) {
    LOG(LS_WARNING) << "Invalid audio transport";
    return 0;
  }
  const size_t rec_bytes_per_sample = [&] {
    rtc::CritScope lock(&lock_);
    return rec_bytes_per_sample_;
  }();
  uint32_t new_mic_level(0);
  uint32_t total_delay_ms = play_delay_ms_ + rec_delay_ms_;
  size_t num_samples = rec_buffer_.size() / rec_bytes_per_sample;
  int32_t res = audio_transport_cb_->RecordedDataIsAvailable(
      rec_buffer_.data(), num_samples, rec_bytes_per_sample_, rec_channels_,
      rec_sample_rate_, total_delay_ms, clock_drift_, current_mic_level_,
      typing_status_, new_mic_level);
  if (res != -1) {
    new_mic_level_ = new_mic_level;
  } else {
    LOG(LS_ERROR) << "RecordedDataIsAvailable() failed";
  }
  return 0;
}

int32_t AudioDeviceBuffer::RequestPlayoutData(size_t num_samples) {
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

  const size_t play_channels = [&] {
    rtc::CritScope lock(&lock_);
    return play_channels_;
  }();

  // The consumer can change the request size on the fly and we therefore
  // resize the buffer accordingly. Also takes place at the first call to this
  // method.
  const size_t play_bytes_per_sample = play_channels * sizeof(int16_t);
  const size_t size_in_bytes = num_samples * play_bytes_per_sample;
  if (play_buffer_.size() != size_in_bytes) {
    play_buffer_.SetSize(size_in_bytes);
    LOG(LS_INFO) << "Size of playout buffer: " << play_buffer_.size();
  }

  rtc::CritScope lock(&lock_cb_);

  // It is currently supported to start playout without a valid audio
  // transport object. Leads to warning and silence.
  if (!audio_transport_cb_) {
    LOG(LS_WARNING) << "Invalid audio transport";
    return 0;
  }

  // Retrieve new 16-bit PCM audio data using the audio transport instance.
  int64_t elapsed_time_ms = -1;
  int64_t ntp_time_ms = -1;
  size_t num_samples_out(0);
  uint32_t res = audio_transport_cb_->NeedMorePlayData(
      num_samples, play_bytes_per_sample_, play_channels, play_sample_rate_,
      play_buffer_.data(), num_samples_out, &elapsed_time_ms, &ntp_time_ms);
  if (res != 0) {
    LOG(LS_ERROR) << "NeedMorePlayData() failed";
  }

  // Derive a new level value twice per second.
  int16_t max_abs = 0;
  RTC_DCHECK_LT(play_stat_count_, 50);
  if (++play_stat_count_ >= 50) {
    const size_t size = num_samples * play_channels;
    // Returns the largest absolute value in a signed 16-bit vector.
    max_abs = WebRtcSpl_MaxAbsValueW16(
        reinterpret_cast<const int16_t*>(play_buffer_.data()), size);
    play_stat_count_ = 0;
  }
  // Update some stats but do it on the task queue to ensure that the members
  // are modified and read on the same thread. Note that |max_abs| will be
  // zero in most calls and then have no effect of the stats. It is only updated
  // approximately two times per second and can then change the stats.
  task_queue_.PostTask(rtc::Bind(&AudioDeviceBuffer::UpdatePlayStats, this,
                                 max_abs, num_samples_out));
  return static_cast<int32_t>(num_samples_out);
}

int32_t AudioDeviceBuffer::GetPlayoutData(void* audio_buffer) {
  RTC_DCHECK_GT(play_buffer_.size(), 0u);
  const size_t play_bytes_per_sample = [&] {
    rtc::CritScope lock(&lock_);
    return play_bytes_per_sample_;
  }();
  memcpy(audio_buffer, play_buffer_.data(), play_buffer_.size());
  return static_cast<int32_t>(play_buffer_.size() / play_bytes_per_sample);
}

void AudioDeviceBuffer::StartTimer() {
  num_stat_reports_ = 0;
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
  if (++num_stat_reports_ > 1 && time_since_last > 0) {
    uint32_t diff_samples = rec_samples_ - last_rec_samples_;
    float rate = diff_samples / (static_cast<float>(time_since_last) / 1000.0);
    LOG(INFO) << "[REC : " << time_since_last << "msec, "
              << rec_sample_rate_ / 1000
              << "kHz] callbacks: " << rec_callbacks_ - last_rec_callbacks_
              << ", "
              << "samples: " << diff_samples << ", "
              << "rate: " << static_cast<int>(rate + 0.5) << ", "
              << "level: " << max_rec_level_;

    diff_samples = play_samples_ - last_play_samples_;
    rate = diff_samples / (static_cast<float>(time_since_last) / 1000.0);
    LOG(INFO) << "[PLAY: " << time_since_last << "msec, "
              << play_sample_rate_ / 1000
              << "kHz] callbacks: " << play_callbacks_ - last_play_callbacks_
              << ", "
              << "samples: " << diff_samples << ", "
              << "rate: " << static_cast<int>(rate + 0.5) << ", "
              << "level: " << max_play_level_;
  }

  // Count number of times we detect "no audio" corresponding to a case where
  // all level measurements have been zero.
  if (max_rec_level_ == 0) {
    ++num_rec_level_is_zero_;
  }

  last_rec_callbacks_ = rec_callbacks_;
  last_play_callbacks_ = play_callbacks_;
  last_rec_samples_ = rec_samples_;
  last_play_samples_ = play_samples_;
  max_rec_level_ = 0;
  max_play_level_ = 0;

  int64_t time_to_wait_ms = next_callback_time - rtc::TimeMillis();
  RTC_DCHECK_GT(time_to_wait_ms, 0) << "Invalid timer interval";

  // Update some stats but do it on the task queue to ensure that access of
  // members is serialized hence avoiding usage of locks.
  task_queue_.PostDelayedTask(rtc::Bind(&AudioDeviceBuffer::LogStats, this),
                              time_to_wait_ms);
}

void AudioDeviceBuffer::ResetRecStats() {
  rec_callbacks_ = 0;
  last_rec_callbacks_ = 0;
  rec_samples_ = 0;
  last_rec_samples_ = 0;
  max_rec_level_ = 0;
  num_rec_level_is_zero_ = 0;
}

void AudioDeviceBuffer::ResetPlayStats() {
  last_playout_time_ = rtc::TimeMillis();
  play_callbacks_ = 0;
  last_play_callbacks_ = 0;
  play_samples_ = 0;
  last_play_samples_ = 0;
  max_play_level_ = 0;
}

void AudioDeviceBuffer::UpdateRecStats(int16_t max_abs, size_t num_samples) {
  RTC_DCHECK(task_queue_.IsCurrent());
  ++rec_callbacks_;
  rec_samples_ += num_samples;
  if (max_abs > max_rec_level_) {
    max_rec_level_ = max_abs;
  }
}

void AudioDeviceBuffer::UpdatePlayStats(int16_t max_abs, size_t num_samples) {
  RTC_DCHECK(task_queue_.IsCurrent());
  ++play_callbacks_;
  play_samples_ += num_samples;
  if (max_abs > max_play_level_) {
    max_play_level_ = max_abs;
  }
}

}  // namespace webrtc
