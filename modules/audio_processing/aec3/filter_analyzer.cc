/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/filter_analyzer.h"
#include <math.h>

#include <algorithm>
#include <array>
#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {
namespace {

size_t FindPeakIndex(rtc::ArrayView<const float> filter_time_domain) {
  size_t peak_index = 0;
  float max_h2 = filter_time_domain[0] * filter_time_domain[0];
  for (size_t k = 1; k < filter_time_domain.size(); ++k) {
    float tmp = filter_time_domain[k] * filter_time_domain[k];
    if (tmp > max_h2) {
      peak_index = k;
      max_h2 = tmp;
    }
  }

  return peak_index;
}

}  // namespace

FilterAnalyzer::FilterAnalyzer(const EchoCanceller3Config& config)
    : bounded_erl_(config.ep_strength.bounded_erl),
      default_gain_(config.ep_strength.lf),
      active_render_threshold_(config.render_levels.active_render_limit *
                               config.render_levels.active_render_limit *
                               kFftLengthBy2) {
  Reset();
}

FilterAnalyzer::~FilterAnalyzer() = default;

void FilterAnalyzer::Reset() {
  delay_blocks_ = 0;
  consistent_estimate_ = false;
  blocks_since_reset_ = 0;
  consistent_estimate_ = false;
  consistent_estimate_counter_ = 0;
  consistent_delay_reference_ = -10;
  gain_ = default_gain_;
}

void FilterAnalyzer::Update(rtc::ArrayView<const float> filter_time_domain,
                            const RenderBuffer& render_buffer) {
  size_t peak_index = FindPeakIndex(filter_time_domain);
  delay_blocks_ = peak_index / kBlockSize;

  UpdateFilterGain(filter_time_domain, peak_index);

  float filter_floor = 0;
  float filter_secondary_peak = 0;
  size_t limit1 = peak_index < 64 ? 0 : peak_index - 64;
  size_t limit2 =
      peak_index > filter_time_domain.size() - 129 ? 0 : peak_index + 128;

  for (size_t k = 0; k < limit1; ++k) {
    float abs_h = fabsf(filter_time_domain[k]);
    filter_floor += abs_h;
    filter_secondary_peak = std::max(filter_secondary_peak, abs_h);
  }
  for (size_t k = limit2; k < filter_time_domain.size(); ++k) {
    float abs_h = fabsf(filter_time_domain[k]);
    filter_floor += abs_h;
    filter_secondary_peak = std::max(filter_secondary_peak, abs_h);
  }

  filter_floor /= (limit1 + filter_time_domain.size() - limit2);

  float abs_peak = fabsf(filter_time_domain[peak_index]);
  bool significant_peak_index =
      abs_peak > 10.f * filter_floor && abs_peak > 2.f * filter_secondary_peak;

  if (consistent_delay_reference_ != delay_blocks_ || !significant_peak_index) {
    consistent_estimate_counter_ = 0;
    consistent_delay_reference_ = delay_blocks_;
  } else {
    const auto& x = render_buffer.Block(-delay_blocks_)[0];
    const float x_energy =
        std::inner_product(x.begin(), x.end(), x.begin(), 0.f);
    const bool active_render_block = x_energy > active_render_threshold_;

    if (active_render_block) {
      ++consistent_estimate_counter_;
    }
  }

  consistent_estimate_ =
      consistent_estimate_counter_ > 1.5f * kNumBlocksPerSecond;
}

void FilterAnalyzer::UpdateFilterGain(
    rtc::ArrayView<const float> filter_time_domain,
    size_t peak_index) {
  bool sufficient_time_to_converge =
      ++blocks_since_reset_ > 5 * kNumBlocksPerSecond;

  if (sufficient_time_to_converge && consistent_estimate_) {
    gain_ = fabsf(filter_time_domain[peak_index]);
  } else {
    if (gain_) {
      gain_ = std::max(gain_, fabsf(filter_time_domain[peak_index]));
    }
  }

  if (bounded_erl_ && gain_) {
    gain_ = std::max(gain_, 0.01f);
  }
}

}  // namespace webrtc
