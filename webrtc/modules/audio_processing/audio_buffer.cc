/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/audio_buffer.h"

#include "webrtc/common_audio/resampler/push_sinc_resampler.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_processing/channel_buffer.h"
#include "webrtc/modules/audio_processing/common.h"

namespace webrtc {
namespace {

bool HasKeyboardChannel(AudioProcessing::ChannelLayout layout) {
  switch (layout) {
    case AudioProcessing::kMono:
    case AudioProcessing::kStereo:
      return false;
    case AudioProcessing::kMonoAndKeyboard:
    case AudioProcessing::kStereoAndKeyboard:
      return true;
  }
  assert(false);
  return false;
}

int KeyboardChannelIndex(AudioProcessing::ChannelLayout layout) {
  switch (layout) {
    case AudioProcessing::kMono:
    case AudioProcessing::kStereo:
      assert(false);
      return -1;
    case AudioProcessing::kMonoAndKeyboard:
      return 1;
    case AudioProcessing::kStereoAndKeyboard:
      return 2;
  }
  assert(false);
  return -1;
}

template <typename T>
void StereoToMono(const T* left, const T* right, T* out,
                  int samples_per_channel) {
  for (int i = 0; i < samples_per_channel; ++i)
    out[i] = (left[i] + right[i]) / 2;
}

}  // namespace

AudioBuffer::AudioBuffer(int input_samples_per_channel,
                         int num_input_channels,
                         int process_samples_per_channel,
                         int num_process_channels,
                         int output_samples_per_channel)
  : input_samples_per_channel_(input_samples_per_channel),
    num_input_channels_(num_input_channels),
    proc_samples_per_channel_(process_samples_per_channel),
    num_proc_channels_(num_process_channels),
    output_samples_per_channel_(output_samples_per_channel),
    samples_per_split_channel_(proc_samples_per_channel_),
    mixed_low_pass_valid_(false),
    reference_copied_(false),
    activity_(AudioFrame::kVadUnknown),
    keyboard_data_(NULL),
    channels_(new IFChannelBuffer(proc_samples_per_channel_,
                                  num_proc_channels_)) {
  assert(input_samples_per_channel_ > 0);
  assert(proc_samples_per_channel_ > 0);
  assert(output_samples_per_channel_ > 0);
  assert(num_input_channels_ > 0 && num_input_channels_ <= 2);
  assert(num_proc_channels_ <= num_input_channels);

  if (num_input_channels_ == 2 && num_proc_channels_ == 1) {
    input_buffer_.reset(new ChannelBuffer<float>(input_samples_per_channel_,
                                                 num_proc_channels_));
  }

  if (input_samples_per_channel_ != proc_samples_per_channel_ ||
      output_samples_per_channel_ != proc_samples_per_channel_) {
    // Create an intermediate buffer for resampling.
    process_buffer_.reset(new ChannelBuffer<float>(proc_samples_per_channel_,
                                                   num_proc_channels_));
  }

  if (input_samples_per_channel_ != proc_samples_per_channel_) {
    input_resamplers_.reserve(num_proc_channels_);
    for (int i = 0; i < num_proc_channels_; ++i) {
      input_resamplers_.push_back(
          new PushSincResampler(input_samples_per_channel_,
                                proc_samples_per_channel_));
    }
  }

  if (output_samples_per_channel_ != proc_samples_per_channel_) {
    output_resamplers_.reserve(num_proc_channels_);
    for (int i = 0; i < num_proc_channels_; ++i) {
      output_resamplers_.push_back(
          new PushSincResampler(proc_samples_per_channel_,
                                output_samples_per_channel_));
    }
  }

  if (proc_samples_per_channel_ == kSamplesPer32kHzChannel ||
      proc_samples_per_channel_ == kSamplesPer48kHzChannel) {
    samples_per_split_channel_ = kSamplesPer16kHzChannel;
    split_channels_.push_back(new IFChannelBuffer(samples_per_split_channel_,
                                                  num_proc_channels_));
    split_channels_.push_back(new IFChannelBuffer(samples_per_split_channel_,
                                                  num_proc_channels_));
    splitting_filter_.reset(new SplittingFilter(num_proc_channels_));
    if (proc_samples_per_channel_ == kSamplesPer48kHzChannel) {
      split_channels_.push_back(new IFChannelBuffer(samples_per_split_channel_,
                                                    num_proc_channels_));
    }
  }
}

AudioBuffer::~AudioBuffer() {}

void AudioBuffer::CopyFrom(const float* const* data,
                           int samples_per_channel,
                           AudioProcessing::ChannelLayout layout) {
  assert(samples_per_channel == input_samples_per_channel_);
  assert(ChannelsFromLayout(layout) == num_input_channels_);
  InitForNewData();

  if (HasKeyboardChannel(layout)) {
    keyboard_data_ = data[KeyboardChannelIndex(layout)];
  }

  // Downmix.
  const float* const* data_ptr = data;
  if (num_input_channels_ == 2 && num_proc_channels_ == 1) {
    StereoToMono(data[0],
                 data[1],
                 input_buffer_->channel(0),
                 input_samples_per_channel_);
    data_ptr = input_buffer_->channels();
  }

  // Resample.
  if (input_samples_per_channel_ != proc_samples_per_channel_) {
    for (int i = 0; i < num_proc_channels_; ++i) {
      input_resamplers_[i]->Resample(data_ptr[i],
                                     input_samples_per_channel_,
                                     process_buffer_->channel(i),
                                     proc_samples_per_channel_);
    }
    data_ptr = process_buffer_->channels();
  }

  // Convert to the S16 range.
  for (int i = 0; i < num_proc_channels_; ++i) {
    FloatToFloatS16(data_ptr[i], proc_samples_per_channel_,
                    channels_->fbuf()->channel(i));
  }
}

void AudioBuffer::CopyTo(int samples_per_channel,
                         AudioProcessing::ChannelLayout layout,
                         float* const* data) {
  assert(samples_per_channel == output_samples_per_channel_);
  assert(ChannelsFromLayout(layout) == num_proc_channels_);

  // Convert to the float range.
  float* const* data_ptr = data;
  if (output_samples_per_channel_ != proc_samples_per_channel_) {
    // Convert to an intermediate buffer for subsequent resampling.
    data_ptr = process_buffer_->channels();
  }
  for (int i = 0; i < num_proc_channels_; ++i) {
    FloatS16ToFloat(channels_->fbuf()->channel(i), proc_samples_per_channel_,
                    data_ptr[i]);
  }

  // Resample.
  if (output_samples_per_channel_ != proc_samples_per_channel_) {
    for (int i = 0; i < num_proc_channels_; ++i) {
      output_resamplers_[i]->Resample(data_ptr[i],
                                      proc_samples_per_channel_,
                                      data[i],
                                      output_samples_per_channel_);
    }
  }
}

void AudioBuffer::InitForNewData() {
  keyboard_data_ = NULL;
  mixed_low_pass_valid_ = false;
  reference_copied_ = false;
  activity_ = AudioFrame::kVadUnknown;
}

const int16_t* AudioBuffer::data(int channel) const {
  return channels_->ibuf_const()->channel(channel);
}

int16_t* AudioBuffer::data(int channel) {
  mixed_low_pass_valid_ = false;
  return channels_->ibuf()->channel(channel);
}

const int16_t* const* AudioBuffer::channels() const {
  return channels_->ibuf_const()->channels();
}

int16_t* const* AudioBuffer::channels() {
  mixed_low_pass_valid_ = false;
  return channels_->ibuf()->channels();
}

const float* AudioBuffer::data_f(int channel) const {
  return channels_->fbuf_const()->channel(channel);
}

float* AudioBuffer::data_f(int channel) {
  mixed_low_pass_valid_ = false;
  return channels_->fbuf()->channel(channel);
}

const float* const* AudioBuffer::channels_f() const {
  return channels_->fbuf_const()->channels();
}

float* const* AudioBuffer::channels_f() {
  mixed_low_pass_valid_ = false;
  return channels_->fbuf()->channels();
}

const int16_t* AudioBuffer::low_pass_split_data(int channel) const {
  return split_channels_.size() > 0
      ? split_channels_[0]->ibuf_const()->channel(channel)
      : data(channel);
}

int16_t* AudioBuffer::low_pass_split_data(int channel) {
  mixed_low_pass_valid_ = false;
  return split_channels_.size() > 0
      ? split_channels_[0]->ibuf()->channel(channel)
      : data(channel);
}

const int16_t* const* AudioBuffer::low_pass_split_channels() const {
  return split_channels_.size() > 0
             ? split_channels_[0]->ibuf_const()->channels()
             : channels();
}

int16_t* const* AudioBuffer::low_pass_split_channels() {
  mixed_low_pass_valid_ = false;
  return split_channels_.size() > 0 ? split_channels_[0]->ibuf()->channels()
                                   : channels();
}

const float* AudioBuffer::low_pass_split_data_f(int channel) const {
  return split_channels_.size() > 0
      ? split_channels_[0]->fbuf_const()->channel(channel)
      : data_f(channel);
}

float* AudioBuffer::low_pass_split_data_f(int channel) {
  mixed_low_pass_valid_ = false;
  return split_channels_.size() > 0
      ? split_channels_[0]->fbuf()->channel(channel)
      : data_f(channel);
}

const float* const* AudioBuffer::low_pass_split_channels_f() const {
  return split_channels_.size() > 0
      ? split_channels_[0]->fbuf_const()->channels()
      : channels_f();
}

float* const* AudioBuffer::low_pass_split_channels_f() {
  mixed_low_pass_valid_ = false;
  return split_channels_.size() > 0
      ? split_channels_[0]->fbuf()->channels()
      : channels_f();
}

const int16_t* AudioBuffer::high_pass_split_data(int channel) const {
  return split_channels_.size() > 1
      ? split_channels_[1]->ibuf_const()->channel(channel)
      : NULL;
}

int16_t* AudioBuffer::high_pass_split_data(int channel) {
  return split_channels_.size() > 1
      ? split_channels_[1]->ibuf()->channel(channel)
      : NULL;
}

const int16_t* const* AudioBuffer::high_pass_split_channels() const {
  return split_channels_.size() > 1
             ? split_channels_[1]->ibuf_const()->channels()
             : NULL;
}

int16_t* const* AudioBuffer::high_pass_split_channels() {
  return split_channels_.size() > 1 ? split_channels_[1]->ibuf()->channels()
                                    : NULL;
}

const float* AudioBuffer::high_pass_split_data_f(int channel) const {
  return split_channels_.size() > 1
      ? split_channels_[1]->fbuf_const()->channel(channel)
      : NULL;
}

float* AudioBuffer::high_pass_split_data_f(int channel) {
  return split_channels_.size() > 1
      ? split_channels_[1]->fbuf()->channel(channel)
      : NULL;
}

const float* const* AudioBuffer::high_pass_split_channels_f() const {
  return split_channels_.size() > 1
      ? split_channels_[1]->fbuf_const()->channels()
      : NULL;
}

float* const* AudioBuffer::high_pass_split_channels_f() {
  return split_channels_.size() > 1
      ? split_channels_[1]->fbuf()->channels()
      : NULL;
}

const float* const* AudioBuffer::super_high_pass_split_channels_f() const {
  return split_channels_.size() > 2
      ? split_channels_[2]->fbuf_const()->channels()
      : NULL;
}

float* const* AudioBuffer::super_high_pass_split_channels_f() {
  return split_channels_.size() > 2
      ? split_channels_[2]->fbuf()->channels()
      : NULL;
}

const int16_t* AudioBuffer::mixed_low_pass_data() {
  // Currently only mixing stereo to mono is supported.
  assert(num_proc_channels_ == 1 || num_proc_channels_ == 2);

  if (num_proc_channels_ == 1) {
    return low_pass_split_data(0);
  }

  if (!mixed_low_pass_valid_) {
    if (!mixed_low_pass_channels_.get()) {
      mixed_low_pass_channels_.reset(
          new ChannelBuffer<int16_t>(samples_per_split_channel_, 1));
    }
    StereoToMono(low_pass_split_data(0),
                 low_pass_split_data(1),
                 mixed_low_pass_channels_->data(),
                 samples_per_split_channel_);
    mixed_low_pass_valid_ = true;
  }
  return mixed_low_pass_channels_->data();
}

const int16_t* AudioBuffer::low_pass_reference(int channel) const {
  if (!reference_copied_) {
    return NULL;
  }

  return low_pass_reference_channels_->channel(channel);
}

const float* AudioBuffer::keyboard_data() const {
  return keyboard_data_;
}

void AudioBuffer::set_activity(AudioFrame::VADActivity activity) {
  activity_ = activity;
}

AudioFrame::VADActivity AudioBuffer::activity() const {
  return activity_;
}

int AudioBuffer::num_channels() const {
  return num_proc_channels_;
}

int AudioBuffer::samples_per_channel() const {
  return proc_samples_per_channel_;
}

int AudioBuffer::samples_per_split_channel() const {
  return samples_per_split_channel_;
}

int AudioBuffer::samples_per_keyboard_channel() const {
  // We don't resample the keyboard channel.
  return input_samples_per_channel_;
}

// TODO(andrew): Do deinterleaving and mixing in one step?
void AudioBuffer::DeinterleaveFrom(AudioFrame* frame) {
  assert(proc_samples_per_channel_ == input_samples_per_channel_);
  assert(frame->num_channels_ == num_input_channels_);
  assert(frame->samples_per_channel_ ==  proc_samples_per_channel_);
  InitForNewData();
  activity_ = frame->vad_activity_;

  if (num_input_channels_ == 2 && num_proc_channels_ == 1) {
    // Downmix directly; no explicit deinterleaving needed.
    int16_t* downmixed = channels_->ibuf()->channel(0);
    for (int i = 0; i < input_samples_per_channel_; ++i) {
      downmixed[i] = (frame->data_[i * 2] + frame->data_[i * 2 + 1]) / 2;
    }
  } else {
    assert(num_proc_channels_ == num_input_channels_);
    int16_t* interleaved = frame->data_;
    for (int i = 0; i < num_proc_channels_; ++i) {
      int16_t* deinterleaved = channels_->ibuf()->channel(i);
      int interleaved_idx = i;
      for (int j = 0; j < proc_samples_per_channel_; ++j) {
        deinterleaved[j] = interleaved[interleaved_idx];
        interleaved_idx += num_proc_channels_;
      }
    }
  }
}

void AudioBuffer::InterleaveTo(AudioFrame* frame, bool data_changed) const {
  assert(proc_samples_per_channel_ == output_samples_per_channel_);
  assert(num_proc_channels_ == num_input_channels_);
  assert(frame->num_channels_ == num_proc_channels_);
  assert(frame->samples_per_channel_ == proc_samples_per_channel_);
  frame->vad_activity_ = activity_;

  if (!data_changed) {
    return;
  }

  int16_t* interleaved = frame->data_;
  for (int i = 0; i < num_proc_channels_; i++) {
    int16_t* deinterleaved = channels_->ibuf()->channel(i);
    int interleaved_idx = i;
    for (int j = 0; j < proc_samples_per_channel_; j++) {
      interleaved[interleaved_idx] = deinterleaved[j];
      interleaved_idx += num_proc_channels_;
    }
  }
}

void AudioBuffer::CopyLowPassToReference() {
  reference_copied_ = true;
  if (!low_pass_reference_channels_.get()) {
    low_pass_reference_channels_.reset(
        new ChannelBuffer<int16_t>(samples_per_split_channel_,
                                   num_proc_channels_));
  }
  for (int i = 0; i < num_proc_channels_; i++) {
    low_pass_reference_channels_->CopyFrom(low_pass_split_data(i), i);
  }
}

void AudioBuffer::SplitIntoFrequencyBands() {
  splitting_filter_->Analysis(channels_.get(),
                              split_channels_.get());
}

void AudioBuffer::MergeFrequencyBands() {
  splitting_filter_->Synthesis(split_channels_.get(),
                               channels_.get());
}

}  // namespace webrtc
