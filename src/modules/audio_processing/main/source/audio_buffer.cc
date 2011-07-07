/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio_buffer.h"

#include "module_common_types.h"

namespace webrtc {
namespace {

enum {
  kSamplesPer8kHzChannel = 80,
  kSamplesPer16kHzChannel = 160,
  kSamplesPer32kHzChannel = 320
};

void StereoToMono(const WebRtc_Word16* left, const WebRtc_Word16* right,
                  WebRtc_Word16* out, int samples_per_channel) {
  WebRtc_Word32 data_int32 = 0;
  for (int i = 0; i < samples_per_channel; i++) {
    data_int32 = (left[i] + right[i]) >> 1;
    if (data_int32 > 32767) {
      data_int32 = 32767;
    } else if (data_int32 < -32768) {
      data_int32 = -32768;
    }

    out[i] = static_cast<WebRtc_Word16>(data_int32);
  }
}
}  // namespace

struct AudioChannel {
  AudioChannel() {
    memset(data, 0, sizeof(data));
  }

  WebRtc_Word16 data[kSamplesPer32kHzChannel];
};

struct SplitAudioChannel {
  SplitAudioChannel() {
    memset(low_pass_data, 0, sizeof(low_pass_data));
    memset(high_pass_data, 0, sizeof(high_pass_data));
    memset(analysis_filter_state1, 0, sizeof(analysis_filter_state1));
    memset(analysis_filter_state2, 0, sizeof(analysis_filter_state2));
    memset(synthesis_filter_state1, 0, sizeof(synthesis_filter_state1));
    memset(synthesis_filter_state2, 0, sizeof(synthesis_filter_state2));
  }

  WebRtc_Word16 low_pass_data[kSamplesPer16kHzChannel];
  WebRtc_Word16 high_pass_data[kSamplesPer16kHzChannel];

  WebRtc_Word32 analysis_filter_state1[6];
  WebRtc_Word32 analysis_filter_state2[6];
  WebRtc_Word32 synthesis_filter_state1[6];
  WebRtc_Word32 synthesis_filter_state2[6];
};

// TODO(am): check range of input parameters?
AudioBuffer::AudioBuffer(WebRtc_Word32 max_num_channels,
                         WebRtc_Word32 samples_per_channel)
    : max_num_channels_(max_num_channels),
      num_channels_(0),
      num_mixed_channels_(0),
      num_mixed_low_pass_channels_(0),
      samples_per_channel_(samples_per_channel),
      samples_per_split_channel_(samples_per_channel),
      reference_copied_(false),
      data_(NULL),
      channels_(NULL),
      split_channels_(NULL),
      mixed_low_pass_channels_(NULL),
      low_pass_reference_channels_(NULL) {
  if (max_num_channels_ > 1) {
    channels_ = new AudioChannel[max_num_channels_];
    mixed_low_pass_channels_ = new AudioChannel[max_num_channels_];
  }
  low_pass_reference_channels_ = new AudioChannel[max_num_channels_];

  if (samples_per_channel_ == kSamplesPer32kHzChannel) {
    split_channels_ = new SplitAudioChannel[max_num_channels_];
    samples_per_split_channel_ = kSamplesPer16kHzChannel;
  }
}

AudioBuffer::~AudioBuffer() {
  if (channels_ != NULL) {
    delete [] channels_;
  }

  if (mixed_low_pass_channels_ != NULL) {
    delete [] mixed_low_pass_channels_;
  }

  if (low_pass_reference_channels_ != NULL) {
    delete [] low_pass_reference_channels_;
  }

  if (split_channels_ != NULL) {
    delete [] split_channels_;
  }
}

WebRtc_Word16* AudioBuffer::data(WebRtc_Word32 channel) const {
  assert(channel >= 0 && channel < num_channels_);
  if (data_ != NULL) {
    return data_;
  }

  return channels_[channel].data;
}

WebRtc_Word16* AudioBuffer::low_pass_split_data(WebRtc_Word32 channel) const {
  assert(channel >= 0 && channel < num_channels_);
  if (split_channels_ == NULL) {
    return data(channel);
  }

  return split_channels_[channel].low_pass_data;
}

WebRtc_Word16* AudioBuffer::high_pass_split_data(WebRtc_Word32 channel) const {
  assert(channel >= 0 && channel < num_channels_);
  if (split_channels_ == NULL) {
    return NULL;
  }

  return split_channels_[channel].high_pass_data;
}

WebRtc_Word16* AudioBuffer::mixed_low_pass_data(WebRtc_Word32 channel) const {
  assert(channel >= 0 && channel < num_mixed_low_pass_channels_);

  return mixed_low_pass_channels_[channel].data;
}

WebRtc_Word16* AudioBuffer::low_pass_reference(WebRtc_Word32 channel) const {
  assert(channel >= 0 && channel < num_channels_);
  if (!reference_copied_) {
    return NULL;
  }

  return low_pass_reference_channels_[channel].data;
}

WebRtc_Word32* AudioBuffer::analysis_filter_state1(WebRtc_Word32 channel) const {
  assert(channel >= 0 && channel < num_channels_);
  return split_channels_[channel].analysis_filter_state1;
}

WebRtc_Word32* AudioBuffer::analysis_filter_state2(WebRtc_Word32 channel) const {
  assert(channel >= 0 && channel < num_channels_);
  return split_channels_[channel].analysis_filter_state2;
}

WebRtc_Word32* AudioBuffer::synthesis_filter_state1(WebRtc_Word32 channel) const {
  assert(channel >= 0 && channel < num_channels_);
  return split_channels_[channel].synthesis_filter_state1;
}

WebRtc_Word32* AudioBuffer::synthesis_filter_state2(WebRtc_Word32 channel) const {
  assert(channel >= 0 && channel < num_channels_);
  return split_channels_[channel].synthesis_filter_state2;
}

WebRtc_Word32 AudioBuffer::num_channels() const {
  return num_channels_;
}

WebRtc_Word32 AudioBuffer::samples_per_channel() const {
  return samples_per_channel_;
}

WebRtc_Word32 AudioBuffer::samples_per_split_channel() const {
  return samples_per_split_channel_;
}

// TODO(ajm): Do deinterleaving and mixing in one step?
void AudioBuffer::DeinterleaveFrom(AudioFrame* audioFrame) {
  assert(audioFrame->_audioChannel <= max_num_channels_);
  assert(audioFrame->_payloadDataLengthInSamples ==  samples_per_channel_);

  num_channels_ = audioFrame->_audioChannel;
  num_mixed_channels_ = 0;
  num_mixed_low_pass_channels_ = 0;
  reference_copied_ = false;

  if (num_channels_ == 1) {
    // We can get away with a pointer assignment in this case.
    data_ = audioFrame->_payloadData;
    return;
  }

  for (int i = 0; i < num_channels_; i++) {
    WebRtc_Word16* deinterleaved = channels_[i].data;
    WebRtc_Word16* interleaved = audioFrame->_payloadData;
    WebRtc_Word32 interleaved_idx = i;
    for (int j = 0; j < samples_per_channel_; j++) {
      deinterleaved[j] = interleaved[interleaved_idx];
      interleaved_idx += num_channels_;
    }
  }
}

void AudioBuffer::InterleaveTo(AudioFrame* audioFrame) const {
  assert(audioFrame->_audioChannel == num_channels_);
  assert(audioFrame->_payloadDataLengthInSamples == samples_per_channel_);

  if (num_channels_ == 1) {
    if (num_mixed_channels_ == 1) {
      memcpy(audioFrame->_payloadData,
             channels_[0].data,
             sizeof(WebRtc_Word16) * samples_per_channel_);
    } else {
      // These should point to the same buffer in this case.
      assert(data_ == audioFrame->_payloadData);
    }

    return;
  }

  for (int i = 0; i < num_channels_; i++) {
    WebRtc_Word16* deinterleaved = channels_[i].data;
    WebRtc_Word16* interleaved = audioFrame->_payloadData;
    WebRtc_Word32 interleaved_idx = i;
    for (int j = 0; j < samples_per_channel_; j++) {
      interleaved[interleaved_idx] = deinterleaved[j];
      interleaved_idx += num_channels_;
    }
  }
}

// TODO(ajm): would be good to support the no-mix case with pointer assignment.
// TODO(ajm): handle mixing to multiple channels?
void AudioBuffer::Mix(WebRtc_Word32 num_mixed_channels) {
  // We currently only support the stereo to mono case.
  assert(num_channels_ == 2);
  assert(num_mixed_channels == 1);

  StereoToMono(channels_[0].data,
               channels_[1].data,
               channels_[0].data,
               samples_per_channel_);

  num_channels_ = num_mixed_channels;
  num_mixed_channels_ = num_mixed_channels;
}

void AudioBuffer::CopyAndMixLowPass(WebRtc_Word32 num_mixed_channels) {
  // We currently only support the stereo to mono case.
  assert(num_channels_ == 2);
  assert(num_mixed_channels == 1);

  StereoToMono(low_pass_split_data(0),
               low_pass_split_data(1),
               mixed_low_pass_channels_[0].data,
               samples_per_split_channel_);

  num_mixed_low_pass_channels_ = num_mixed_channels;
}

void AudioBuffer::CopyLowPassToReference() {
  reference_copied_ = true;
  for (int i = 0; i < num_channels_; i++) {
    memcpy(low_pass_reference_channels_[i].data,
           low_pass_split_data(i),
           sizeof(WebRtc_Word16) * samples_per_split_channel_);
  }
}
}  // namespace webrtc
