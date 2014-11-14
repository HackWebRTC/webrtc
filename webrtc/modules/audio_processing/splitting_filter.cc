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

#include <assert.h>

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"

namespace webrtc {

SplittingFilter::SplittingFilter(int channels)
    : channels_(channels), two_bands_states_(new TwoBandsStates[channels]) {
}

void SplittingFilter::TwoBandsAnalysis(const int16_t* const* in_data,
                                       int in_data_length,
                                       int channels,
                                       int16_t* const* low_band,
                                       int16_t* const* high_band) {
  assert(channels_ == channels);
  for (int i = 0; i < channels_; ++i) {
    WebRtcSpl_AnalysisQMF(in_data[i], in_data_length, low_band[i], high_band[i],
                          two_bands_states_[i].analysis_filter_state1,
                          two_bands_states_[i].analysis_filter_state2);
  }
}

void SplittingFilter::TwoBandsSynthesis(const int16_t* const* low_band,
                                        const int16_t* const* high_band,
                                        int band_length,
                                        int channels,
                                        int16_t* const* out_data) {
  assert(channels_ == channels);
  for (int i = 0; i < channels_; ++i) {
    WebRtcSpl_SynthesisQMF(low_band[i], high_band[i], band_length, out_data[i],
                           two_bands_states_[i].synthesis_filter_state1,
                           two_bands_states_[i].synthesis_filter_state2);
  }
}

}  // namespace webrtc
