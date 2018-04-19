/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/fine_audio_buffer.h"

#include <memory.h>
#include <stdio.h>
#include <algorithm>

#include "modules/audio_device/audio_device_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

FineAudioBuffer::FineAudioBuffer(AudioDeviceBuffer* device_buffer,
                                 int sample_rate,
                                 size_t capacity)
    : device_buffer_(device_buffer),
      sample_rate_(sample_rate),
      samples_per_10_ms_(static_cast<size_t>(sample_rate_ * 10 / 1000)),
      playout_buffer_(0, capacity),
      record_buffer_(0, capacity) {
  RTC_LOG(INFO) << "samples_per_10_ms_: " << samples_per_10_ms_;
}

FineAudioBuffer::~FineAudioBuffer() {}

void FineAudioBuffer::ResetPlayout() {
  playout_buffer_.Clear();
}

void FineAudioBuffer::ResetRecord() {
  record_buffer_.Clear();
}

void FineAudioBuffer::GetPlayoutData(rtc::ArrayView<int16_t> audio_buffer,
                                     int playout_delay_ms) {
  // Ask WebRTC for new data in chunks of 10ms until we have enough to
  // fulfill the request. It is possible that the buffer already contains
  // enough samples from the last round.
  while (playout_buffer_.size() < audio_buffer.size()) {
    // Get 10ms decoded audio from WebRTC.
    device_buffer_->RequestPlayoutData(samples_per_10_ms_);
    // Append |bytes_per_10_ms_| elements to the end of the buffer.
    const size_t samples_written = playout_buffer_.AppendData(
        samples_per_10_ms_, [&](rtc::ArrayView<int16_t> buf) {
          const size_t samples_per_channel =
              device_buffer_->GetPlayoutData(buf.data());
          // TODO(henrika): this class is only used on mobile devices and is
          // currently limited to mono. Modifications are needed for stereo.
          return samples_per_channel;
        });
    RTC_DCHECK_EQ(samples_per_10_ms_, samples_written);
  }

  const size_t num_bytes = audio_buffer.size() * sizeof(int16_t);
  // Provide the requested number of bytes to the consumer.
  memcpy(audio_buffer.data(), playout_buffer_.data(), num_bytes);
  // Move remaining samples to start of buffer to prepare for next round.
  memmove(playout_buffer_.data(), playout_buffer_.data() + audio_buffer.size(),
          (playout_buffer_.size() - audio_buffer.size()) * sizeof(int16_t));
  playout_buffer_.SetSize(playout_buffer_.size() - audio_buffer.size());
  // Cache playout latency for usage in DeliverRecordedData();
  playout_delay_ms_ = playout_delay_ms;
}

void FineAudioBuffer::DeliverRecordedData(
    rtc::ArrayView<const int16_t> audio_buffer,
    int record_delay_ms) {
  // Always append new data and grow the buffer if needed.
  record_buffer_.AppendData(audio_buffer.data(), audio_buffer.size());
  // Consume samples from buffer in chunks of 10ms until there is not
  // enough data left. The number of remaining samples in the cache is given by
  // the new size of the buffer.
  while (record_buffer_.size() >= samples_per_10_ms_) {
    device_buffer_->SetRecordedBuffer(record_buffer_.data(),
                                      samples_per_10_ms_);
    device_buffer_->SetVQEData(playout_delay_ms_, record_delay_ms);
    device_buffer_->DeliverRecordedData();
    memmove(record_buffer_.data(), record_buffer_.data() + samples_per_10_ms_,
            (record_buffer_.size() - samples_per_10_ms_) * sizeof(int16_t));
    record_buffer_.SetSize(record_buffer_.size() - samples_per_10_ms_);
  }
}

}  // namespace webrtc
