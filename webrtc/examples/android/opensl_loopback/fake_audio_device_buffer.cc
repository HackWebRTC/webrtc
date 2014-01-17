/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/examples/android/opensl_loopback/fake_audio_device_buffer.h"

#include <assert.h>

#include "webrtc/modules/audio_device/android/opensles_common.h"
#include "webrtc/modules/audio_device/android/audio_common.h"

namespace webrtc {

FakeAudioDeviceBuffer::FakeAudioDeviceBuffer()
    : fifo_(kNumBuffers),
      next_available_buffer_(0),
      record_channels_(0),
      play_channels_(0) {
  buf_.reset(new scoped_array<int8_t>[kNumBuffers]);
  for (int i = 0; i < kNumBuffers; ++i) {
    buf_[i].reset(new int8_t[buffer_size_bytes()]);
  }
}

int32_t FakeAudioDeviceBuffer::SetRecordingSampleRate(uint32_t fsHz) {
  assert(static_cast<int>(fsHz) == sample_rate());
  return 0;
}

int32_t FakeAudioDeviceBuffer::SetPlayoutSampleRate(uint32_t fsHz) {
  assert(static_cast<int>(fsHz) == sample_rate());
  return 0;
}

int32_t FakeAudioDeviceBuffer::SetRecordingChannels(uint8_t channels) {
  assert(channels > 0);
  record_channels_ = channels;
  assert((play_channels_ == 0) ||
         (record_channels_ == play_channels_));
  return 0;
}

int32_t FakeAudioDeviceBuffer::SetPlayoutChannels(uint8_t channels) {
  assert(channels > 0);
  play_channels_ = channels;
  assert((record_channels_ == 0) ||
         (record_channels_ == play_channels_));
  return 0;
}

int32_t FakeAudioDeviceBuffer::SetRecordedBuffer(const void* audioBuffer,
                                                 uint32_t nSamples) {
  assert(audioBuffer);
  assert(fifo_.size() < fifo_.capacity());
  assert(nSamples == kDefaultBufSizeInSamples);
  int8_t* buffer = buf_[next_available_buffer_].get();
  next_available_buffer_ = (next_available_buffer_ + 1) % kNumBuffers;
  memcpy(buffer, audioBuffer, nSamples * sizeof(int16_t));
  fifo_.Push(buffer);
  return 0;
}

int32_t FakeAudioDeviceBuffer::RequestPlayoutData(uint32_t nSamples) {
  assert(nSamples == kDefaultBufSizeInSamples);
  return 0;
}

int32_t FakeAudioDeviceBuffer::GetPlayoutData(void* audioBuffer) {
  assert(audioBuffer);
  if (fifo_.size() < 1) {
    // Playout silence until there is data available.
    memset(audioBuffer, 0, buffer_size_bytes());
    return buffer_size_samples();
  }
  int8_t* buffer = fifo_.Pop();
  memcpy(audioBuffer, buffer, buffer_size_bytes());
  return buffer_size_samples();
}

int FakeAudioDeviceBuffer::sample_rate() const {
  return audio_manager_.low_latency_supported() ?
      audio_manager_.native_output_sample_rate() : kDefaultSampleRate;
}

int FakeAudioDeviceBuffer::buffer_size_samples() const {
  return sample_rate() * 10 / 1000;
}

int FakeAudioDeviceBuffer::buffer_size_bytes() const {
  return buffer_size_samples() * kNumChannels * sizeof(int16_t);
}


void FakeAudioDeviceBuffer::ClearBuffer() {
  while (fifo_.size() != 0) {
    fifo_.Pop();
  }
  next_available_buffer_ = 0;
}

}  // namespace webrtc
