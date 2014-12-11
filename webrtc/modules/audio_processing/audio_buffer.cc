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
    num_channels_(num_process_channels),
    num_bands_(1),
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
  assert(num_proc_channels_ <= num_input_channels_);

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
    num_bands_ = proc_samples_per_channel_ / samples_per_split_channel_;
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
  bands_.reset(new int16_t*[num_proc_channels_ * kMaxNumBands]);
  bands_f_.reset(new float*[num_proc_channels_ * kMaxNumBands]);
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
  assert(ChannelsFromLayout(layout) == num_channels_);

  // Convert to the float range.
  float* const* data_ptr = data;
  if (output_samples_per_channel_ != proc_samples_per_channel_) {
    // Convert to an intermediate buffer for subsequent resampling.
    data_ptr = process_buffer_->channels();
  }
  for (int i = 0; i < num_channels_; ++i) {
    FloatS16ToFloat(channels_->fbuf()->channel(i), proc_samples_per_channel_,
                    data_ptr[i]);
  }

  // Resample.
  if (output_samples_per_channel_ != proc_samples_per_channel_) {
    for (int i = 0; i < num_channels_; ++i) {
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
  num_channels_ = num_proc_channels_;
}

const int16_t* AudioBuffer::data_const(int channel) const {
  return channels_const()[channel];
}

int16_t* AudioBuffer::data(int channel) {
  return channels()[channel];
}

const int16_t* const* AudioBuffer::channels_const() const {
  return channels_->ibuf_const()->channels();
}

int16_t* const* AudioBuffer::channels() {
  mixed_low_pass_valid_ = false;
  return channels_->ibuf()->channels();
}

const int16_t* const* AudioBuffer::split_bands_const(int channel) const {
  // This is necessary to make sure that the int16_t data is up to date in the
  // IFChannelBuffer.
  // TODO(aluebs): Having to depend on this to get the updated data is bug
  // prone. One solution is to have ChannelBuffer track the bands as well.
  for (int i = 0; i < kMaxNumBands; ++i) {
    int16_t* const* channels =
        const_cast<int16_t* const*>(split_channels_const(static_cast<Band>(i)));
    bands_[kMaxNumBands * channel + i] = channels ? channels[channel] : NULL;
  }
  return &bands_[kMaxNumBands * channel];
}

int16_t* const* AudioBuffer::split_bands(int channel) {
  mixed_low_pass_valid_ = false;
  // This is necessary to make sure that the int16_t data is up to date and the
  // float data is marked as invalid in the IFChannelBuffer.
  for (int i = 0; i < kMaxNumBands; ++i) {
    int16_t* const* channels = split_channels(static_cast<Band>(i));
    bands_[kMaxNumBands * channel + i] = channels ? channels[channel] : NULL;
  }
  return &bands_[kMaxNumBands * channel];
}

const int16_t* const* AudioBuffer::split_channels_const(Band band) const {
  if (split_channels_.size() > static_cast<size_t>(band)) {
    return split_channels_[band]->ibuf_const()->channels();
  } else {
    return band == kBand0To8kHz ? channels_->ibuf_const()->channels() : NULL;
  }
}

int16_t* const* AudioBuffer::split_channels(Band band) {
  mixed_low_pass_valid_ = false;
  if (split_channels_.size() > static_cast<size_t>(band)) {
    return split_channels_[band]->ibuf()->channels();
  } else {
    return band == kBand0To8kHz ? channels_->ibuf()->channels() : NULL;
  }
}

const float* AudioBuffer::data_const_f(int channel) const {
  return channels_const_f()[channel];
}

float* AudioBuffer::data_f(int channel) {
  return channels_f()[channel];
}

const float* const* AudioBuffer::channels_const_f() const {
  return channels_->fbuf_const()->channels();
}

float* const* AudioBuffer::channels_f() {
  mixed_low_pass_valid_ = false;
  return channels_->fbuf()->channels();
}

const float* const* AudioBuffer::split_bands_const_f(int channel) const {
  // This is necessary to make sure that the float data is up to date in the
  // IFChannelBuffer.
  for (int i = 0; i < kMaxNumBands; ++i) {
    float* const* channels =
        const_cast<float* const*>(split_channels_const_f(static_cast<Band>(i)));
    bands_f_[kMaxNumBands * channel + i] = channels ? channels[channel] : NULL;

  }
  return &bands_f_[kMaxNumBands * channel];
}

float* const* AudioBuffer::split_bands_f(int channel) {
  mixed_low_pass_valid_ = false;
  // This is necessary to make sure that the float data is up to date and the
  // int16_t data is marked as invalid in the IFChannelBuffer.
  for (int i = 0; i < kMaxNumBands; ++i) {
    float* const* channels = split_channels_f(static_cast<Band>(i));
    bands_f_[kMaxNumBands * channel + i] = channels ? channels[channel] : NULL;

  }
  return &bands_f_[kMaxNumBands * channel];
}

const float* const* AudioBuffer::split_channels_const_f(Band band) const {
  if (split_channels_.size() > static_cast<size_t>(band)) {
    return split_channels_[band]->fbuf_const()->channels();
  } else {
    return band == kBand0To8kHz ? channels_->fbuf_const()->channels() : NULL;
  }
}

float* const* AudioBuffer::split_channels_f(Band band) {
  mixed_low_pass_valid_ = false;
  if (split_channels_.size() > static_cast<size_t>(band)) {
    return split_channels_[band]->fbuf()->channels();
  } else {
    return band == kBand0To8kHz ? channels_->fbuf()->channels() : NULL;
  }
}

const int16_t* AudioBuffer::mixed_low_pass_data() {
  // Currently only mixing stereo to mono is supported.
  assert(num_proc_channels_ == 1 || num_proc_channels_ == 2);

  if (num_proc_channels_ == 1) {
    return split_bands_const(0)[kBand0To8kHz];
  }

  if (!mixed_low_pass_valid_) {
    if (!mixed_low_pass_channels_.get()) {
      mixed_low_pass_channels_.reset(
          new ChannelBuffer<int16_t>(samples_per_split_channel_, 1));
    }
    StereoToMono(split_bands_const(0)[kBand0To8kHz],
                 split_bands_const(1)[kBand0To8kHz],
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
  return num_channels_;
}

void AudioBuffer::set_num_channels(int num_channels) {
  num_channels_ = num_channels;
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

int AudioBuffer::num_bands() const {
  return num_bands_;
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
  assert(num_channels_ == num_input_channels_);
  assert(frame->num_channels_ == num_channels_);
  assert(frame->samples_per_channel_ == proc_samples_per_channel_);
  frame->vad_activity_ = activity_;

  if (!data_changed) {
    return;
  }

  int16_t* interleaved = frame->data_;
  for (int i = 0; i < num_channels_; i++) {
    int16_t* deinterleaved = channels_->ibuf()->channel(i);
    int interleaved_idx = i;
    for (int j = 0; j < proc_samples_per_channel_; j++) {
      interleaved[interleaved_idx] = deinterleaved[j];
      interleaved_idx += num_channels_;
    }
  }
}

void AudioBuffer::CopyLowPassToReference() {
  reference_copied_ = true;
  if (!low_pass_reference_channels_.get() ||
      low_pass_reference_channels_->num_channels() != num_channels_) {
    low_pass_reference_channels_.reset(
        new ChannelBuffer<int16_t>(samples_per_split_channel_,
                                   num_proc_channels_));
  }
  for (int i = 0; i < num_proc_channels_; i++) {
    low_pass_reference_channels_->CopyFrom(split_bands_const(i)[kBand0To8kHz],
                                           i);
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
