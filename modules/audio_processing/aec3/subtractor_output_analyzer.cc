/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/subtractor_output_analyzer.h"

#include <array>
#include <numeric>

namespace webrtc {

void SubtractorOutputAnalyzer::Update(
    rtc::ArrayView<const float> y,
    const SubtractorOutput& subtractor_output) {
  const auto& e_main = subtractor_output.e_main;
  const auto& e_shadow = subtractor_output.e_shadow;

  const auto sum_of_squares = [](float a, float b) { return a + b * b; };
  const float y2 = std::accumulate(y.begin(), y.end(), 0.f, sum_of_squares);
  const float e2_main =
      std::accumulate(e_main.begin(), e_main.end(), 0.f, sum_of_squares);
  const float e2_shadow =
      std::accumulate(e_shadow.begin(), e_shadow.end(), 0.f, sum_of_squares);

  constexpr float kConvergenceThreshold = 50 * 50 * kBlockSize;
  main_filter_converged_ = e2_main < 0.5f * y2 && y2 > kConvergenceThreshold;
  shadow_filter_converged_ =
      e2_shadow < 0.05 * y2 && y2 > kConvergenceThreshold;
  main_filter_diverged_ = e2_main > 1.5f * y2 && y2 > 30.f * 30.f * kBlockSize;
}

void SubtractorOutputAnalyzer::HandleEchoPathChange() {
  shadow_filter_converged_ = false;
  main_filter_converged_ = false;
  main_filter_diverged_ = false;
}

}  // namespace webrtc
