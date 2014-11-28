/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/splitting_filter.h"

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_processing/channel_buffer.h"

namespace webrtc {

SplittingFilter::SplittingFilter(int channels)
    : channels_(channels),
      two_bands_states_(new TwoBandsStates[channels]),
      band1_states_(new TwoBandsStates[channels]),
      band2_states_(new TwoBandsStates[channels]) {
  for (int i = 0; i < channels; ++i) {
    analysis_resamplers_.push_back(new PushSincResampler(
        kSamplesPer48kHzChannel, kSamplesPer64kHzChannel));
    synthesis_resamplers_.push_back(new PushSincResampler(
        kSamplesPer64kHzChannel, kSamplesPer48kHzChannel));
  }
}

void SplittingFilter::Analysis(const IFChannelBuffer* in_data,
                               const std::vector<IFChannelBuffer*>& bands) {
  DCHECK(bands.size() == 2 || bands.size() == 3);
  DCHECK_EQ(channels_, in_data->num_channels());
  for (size_t i = 0; i < bands.size(); ++i) {
    DCHECK_EQ(channels_, bands[i]->num_channels());
    DCHECK_EQ(in_data->samples_per_channel(),
              static_cast<int>(bands.size()) * bands[i]->samples_per_channel());
  }
  if (bands.size() == 2) {
    TwoBandsAnalysis(in_data, bands[0], bands[1]);
  } else if (bands.size() == 3) {
    ThreeBandsAnalysis(in_data, bands[0], bands[1], bands[2]);
  }
}

void SplittingFilter::Synthesis(const std::vector<IFChannelBuffer*>& bands,
                                IFChannelBuffer* out_data) {
  DCHECK(bands.size() == 2 || bands.size() == 3);
  DCHECK_EQ(channels_, out_data->num_channels());
  for (size_t i = 0; i < bands.size(); ++i) {
    DCHECK_EQ(channels_, bands[i]->num_channels());
    DCHECK_EQ(out_data->samples_per_channel(),
              static_cast<int>(bands.size()) * bands[i]->samples_per_channel());
  }
  if (bands.size() == 2) {
    TwoBandsSynthesis(bands[0], bands[1], out_data);
  } else if (bands.size() == 3) {
    ThreeBandsSynthesis(bands[0], bands[1], bands[2], out_data);
  }
}

void SplittingFilter::TwoBandsAnalysis(const IFChannelBuffer* in_data,
                                       IFChannelBuffer* band1,
                                       IFChannelBuffer* band2) {
  for (int i = 0; i < channels_; ++i) {
    WebRtcSpl_AnalysisQMF(in_data->ibuf_const()->channel(i),
                          in_data->samples_per_channel(),
                          band1->ibuf()->channel(i),
                          band2->ibuf()->channel(i),
                          two_bands_states_[i].analysis_state1,
                          two_bands_states_[i].analysis_state2);
  }
}

void SplittingFilter::TwoBandsSynthesis(const IFChannelBuffer* band1,
                                        const IFChannelBuffer* band2,
                                        IFChannelBuffer* out_data) {
  for (int i = 0; i < channels_; ++i) {
    WebRtcSpl_SynthesisQMF(band1->ibuf_const()->channel(i),
                           band2->ibuf_const()->channel(i),
                           band1->samples_per_channel(),
                           out_data->ibuf()->channel(i),
                           two_bands_states_[i].synthesis_state1,
                           two_bands_states_[i].synthesis_state2);
  }
}

// This is a simple implementation using the existing code and will be replaced
// by a proper 3 band filter bank.
// It up-samples from 48kHz to 64kHz, splits twice into 2 bands and discards the
// uppermost band, because it is empty anyway.
void SplittingFilter::ThreeBandsAnalysis(const IFChannelBuffer* in_data,
                                         IFChannelBuffer* band1,
                                         IFChannelBuffer* band2,
                                         IFChannelBuffer* band3) {
  DCHECK_EQ(kSamplesPer48kHzChannel,
            in_data->samples_per_channel());
  InitBuffers();
  for (int i = 0; i < channels_; ++i) {
    analysis_resamplers_[i]->Resample(in_data->ibuf_const()->channel(i),
                                      kSamplesPer48kHzChannel,
                                      int_buffer_.get(),
                                      kSamplesPer64kHzChannel);
    WebRtcSpl_AnalysisQMF(int_buffer_.get(),
                          kSamplesPer64kHzChannel,
                          int_buffer_.get(),
                          int_buffer_.get() + kSamplesPer32kHzChannel,
                          two_bands_states_[i].analysis_state1,
                          two_bands_states_[i].analysis_state2);
    WebRtcSpl_AnalysisQMF(int_buffer_.get(),
                          kSamplesPer32kHzChannel,
                          band1->ibuf()->channel(i),
                          band2->ibuf()->channel(i),
                          band1_states_[i].analysis_state1,
                          band1_states_[i].analysis_state2);
    WebRtcSpl_AnalysisQMF(int_buffer_.get() + kSamplesPer32kHzChannel,
                          kSamplesPer32kHzChannel,
                          int_buffer_.get(),
                          band3->ibuf()->channel(i),
                          band2_states_[i].analysis_state1,
                          band2_states_[i].analysis_state2);
  }
}

// This is a simple implementation using the existing code and will be replaced
// by a proper 3 band filter bank.
// Using an empty uppermost band, it merges the 4 bands in 2 steps and
// down-samples from 64kHz to 48kHz.
void SplittingFilter::ThreeBandsSynthesis(const IFChannelBuffer* band1,
                                          const IFChannelBuffer* band2,
                                          const IFChannelBuffer* band3,
                                          IFChannelBuffer* out_data) {
  DCHECK_EQ(kSamplesPer48kHzChannel,
            out_data->samples_per_channel());
  InitBuffers();
  for (int i = 0; i < channels_; ++i) {
    memset(int_buffer_.get(),
           0,
           kSamplesPer64kHzChannel * sizeof(int_buffer_[0]));
    WebRtcSpl_SynthesisQMF(band1->ibuf_const()->channel(i),
                           band2->ibuf_const()->channel(i),
                           kSamplesPer16kHzChannel,
                           int_buffer_.get(),
                           band1_states_[i].synthesis_state1,
                           band1_states_[i].synthesis_state2);
    WebRtcSpl_SynthesisQMF(int_buffer_.get() + kSamplesPer32kHzChannel,
                           band3->ibuf_const()->channel(i),
                           kSamplesPer16kHzChannel,
                           int_buffer_.get() + kSamplesPer32kHzChannel,
                           band2_states_[i].synthesis_state1,
                           band2_states_[i].synthesis_state2);
    WebRtcSpl_SynthesisQMF(int_buffer_.get(),
                           int_buffer_.get() + kSamplesPer32kHzChannel,
                           kSamplesPer32kHzChannel,
                           int_buffer_.get(),
                           two_bands_states_[i].synthesis_state1,
                           two_bands_states_[i].synthesis_state2);
    synthesis_resamplers_[i]->Resample(int_buffer_.get(),
                                       kSamplesPer64kHzChannel,
                                       out_data->ibuf()->channel(i),
                                       kSamplesPer48kHzChannel);
  }
}

void SplittingFilter::InitBuffers() {
  if (!int_buffer_) {
    int_buffer_.reset(new int16_t[kSamplesPer64kHzChannel]);
  }
}

}  // namespace webrtc
