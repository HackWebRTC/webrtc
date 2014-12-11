/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_H_

#include <vector>

#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/modules/audio_processing/channel_buffer.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/splitting_filter.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/scoped_vector.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class PushSincResampler;
class IFChannelBuffer;

static const int kMaxNumBands = 3;
enum Band {
  kBand0To8kHz = 0,
  kBand8To16kHz = 1,
  kBand16To24kHz = 2
};

class AudioBuffer {
 public:
  // TODO(ajm): Switch to take ChannelLayouts.
  AudioBuffer(int input_samples_per_channel,
              int num_input_channels,
              int process_samples_per_channel,
              int num_process_channels,
              int output_samples_per_channel);
  virtual ~AudioBuffer();

  int num_channels() const;
  void set_num_channels(int num_channels);
  int samples_per_channel() const;
  int samples_per_split_channel() const;
  int samples_per_keyboard_channel() const;
  int num_bands() const;

  // Sample array accessors. Channels are guaranteed to be stored contiguously
  // in memory. Prefer to use the const variants of each accessor when
  // possible, since they incur less float<->int16 conversion overhead.
  int16_t* data(int channel);
  const int16_t* data_const(int channel) const;
  int16_t* const* channels();
  const int16_t* const* channels_const() const;
  int16_t* const* split_bands(int channel);
  const int16_t* const* split_bands_const(int channel) const;
  int16_t* const* split_channels(Band band);
  const int16_t* const* split_channels_const(Band band) const;

  // Returns a pointer to the low-pass data downmixed to mono. If this data
  // isn't already available it re-calculates it.
  const int16_t* mixed_low_pass_data();
  const int16_t* low_pass_reference(int channel) const;

  // Float versions of the accessors, with automatic conversion back and forth
  // as necessary. The range of the numbers are the same as for int16_t.
  float* data_f(int channel);
  const float* data_const_f(int channel) const;
  float* const* channels_f();
  const float* const* channels_const_f() const;
  float* const* split_bands_f(int channel);
  const float* const* split_bands_const_f(int channel) const;
  float* const* split_channels_f(Band band);
  const float* const* split_channels_const_f(Band band) const;

  const float* keyboard_data() const;

  void set_activity(AudioFrame::VADActivity activity);
  AudioFrame::VADActivity activity() const;

  // Use for int16 interleaved data.
  void DeinterleaveFrom(AudioFrame* audioFrame);
  // If |data_changed| is false, only the non-audio data members will be copied
  // to |frame|.
  void InterleaveTo(AudioFrame* frame, bool data_changed) const;

  // Use for float deinterleaved data.
  void CopyFrom(const float* const* data,
                int samples_per_channel,
                AudioProcessing::ChannelLayout layout);
  void CopyTo(int samples_per_channel,
              AudioProcessing::ChannelLayout layout,
              float* const* data);
  void CopyLowPassToReference();

  // Splits the signal into different bands.
  void SplitIntoFrequencyBands();
  // Recombine the different bands into one signal.
  void MergeFrequencyBands();

 private:
  // Called from DeinterleaveFrom() and CopyFrom().
  void InitForNewData();

  // The audio is passed into DeinterleaveFrom() or CopyFrom() with input
  // format (samples per channel and number of channels).
  const int input_samples_per_channel_;
  const int num_input_channels_;
  // The audio is stored by DeinterleaveFrom() or CopyFrom() with processing
  // format.
  const int proc_samples_per_channel_;
  const int num_proc_channels_;
  // The audio is returned by InterleaveTo() and CopyTo() with output samples
  // per channels and the current number of channels. This last one can be
  // changed at any time using set_num_channels().
  const int output_samples_per_channel_;
  int num_channels_;

  int num_bands_;
  int samples_per_split_channel_;
  bool mixed_low_pass_valid_;
  bool reference_copied_;
  AudioFrame::VADActivity activity_;

  const float* keyboard_data_;
  scoped_ptr<IFChannelBuffer> channels_;
  ScopedVector<IFChannelBuffer> split_channels_;
  scoped_ptr<int16_t*[]> bands_;
  scoped_ptr<float*[]> bands_f_;
  scoped_ptr<SplittingFilter> splitting_filter_;
  scoped_ptr<ChannelBuffer<int16_t> > mixed_low_pass_channels_;
  scoped_ptr<ChannelBuffer<int16_t> > low_pass_reference_channels_;
  scoped_ptr<ChannelBuffer<float> > input_buffer_;
  scoped_ptr<ChannelBuffer<float> > process_buffer_;
  ScopedVector<PushSincResampler> input_resamplers_;
  ScopedVector<PushSincResampler> output_resamplers_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_H_
