/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/pitch_search.h"
#include "modules/audio_processing/agc2/rnn_vad/pitch_search_internal.h"

namespace webrtc {
namespace rnn_vad {

PitchInfo PitchSearch(rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
                      PitchInfo prev_pitch_48kHz) {
  // Perform the initial pitch search at 12 kHz.
  std::array<float, kBufSize12kHz> pitch_buf_decimated;
  Decimate2x(pitch_buf,
             {pitch_buf_decimated.data(), pitch_buf_decimated.size()});
  // Compute auto-correlation terms.
  std::array<float, kNumInvertedLags12kHz> auto_corr;
  ComputePitchAutoCorrelation(
      {pitch_buf_decimated.data(), pitch_buf_decimated.size()}, kMaxPitch12kHz,
      {auto_corr.data(), auto_corr.size()});
  // Search for pitch at 12 kHz.
  std::array<size_t, 2> pitch_candidates_inv_lags = FindBestPitchPeriods(
      {auto_corr.data(), auto_corr.size()},
      {pitch_buf_decimated.data(), pitch_buf_decimated.size()}, kMaxPitch12kHz);

  // Refine the pitch period estimation.
  // The refinement is done using the pitch buffer that contains 24 kHz samples.
  // Therefore, adapt the inverted lags in |pitch_candidates_inv_lags| from 12
  // to 24 kHz.
  for (size_t i = 0; i < pitch_candidates_inv_lags.size(); ++i)
    pitch_candidates_inv_lags[i] *= 2;
  size_t pitch_inv_lag_48kHz = RefinePitchPeriod48kHz(
      pitch_buf,
      {pitch_candidates_inv_lags.data(), pitch_candidates_inv_lags.size()});
  // Look for stronger harmonics to find the final pitch period and its gain.
  RTC_DCHECK_LT(pitch_inv_lag_48kHz, kMaxPitch48kHz);
  return CheckLowerPitchPeriodsAndComputePitchGain(
      pitch_buf, kMaxPitch48kHz - pitch_inv_lag_48kHz, prev_pitch_48kHz);
}

}  // namespace rnn_vad
}  // namespace webrtc
