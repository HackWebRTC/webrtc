/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_SPLITTING_FILTER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_SPLITTING_FILTER_H_

#include <string.h>

#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

struct TwoBandsStates {
  TwoBandsStates() {
    memset(analysis_filter_state1, 0, sizeof(analysis_filter_state1));
    memset(analysis_filter_state2, 0, sizeof(analysis_filter_state2));
    memset(synthesis_filter_state1, 0, sizeof(synthesis_filter_state1));
    memset(synthesis_filter_state2, 0, sizeof(synthesis_filter_state2));
  }

  static const int kStateSize = 6;
  int analysis_filter_state1[kStateSize];
  int analysis_filter_state2[kStateSize];
  int synthesis_filter_state1[kStateSize];
  int synthesis_filter_state2[kStateSize];
};

class SplittingFilter {
 public:
  SplittingFilter(int channels);

  void TwoBandsAnalysis(const int16_t* const* in_data,
                        int in_data_length,
                        int channels,
                        int16_t* const* low_band,
                        int16_t* const* high_band);
  void TwoBandsSynthesis(const int16_t* const* low_band,
                         const int16_t* const* high_band,
                         int band_length,
                         int channels,
                         int16_t* const* out_data);
  void ThreeBandsAnalysis(const float* const* in_data,
                          int in_data_length,
                          int channels,
                          float* const* low_band,
                          float* const* high_band,
                          float* const* super_high_band);
  void ThreeBandsSynthesis(const float* const* low_band,
                           const float* const* high_band,
                           const float* const* super_high_band,
                           int band_length,
                           int channels,
                           float* const* out_data);

 private:
  int channels_;
  scoped_ptr<TwoBandsStates[]> two_bands_states_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_SPLITTING_FILTER_H_
