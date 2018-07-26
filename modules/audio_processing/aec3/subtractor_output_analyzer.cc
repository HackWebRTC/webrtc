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
    const SubtractorOutput& subtractor_output) {
  const float y2 = subtractor_output.y2;
  const float e2_main = subtractor_output.e2_main;
  const float e2_shadow = subtractor_output.e2_shadow;

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
