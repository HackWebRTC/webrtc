/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/smoothing_filter.h"

#include <cmath>

namespace webrtc {

SmoothingFilterImpl::SmoothingFilterImpl(int init_time_ms_, const Clock* clock)
    : init_time_ms_(init_time_ms_),
      // Duing the initalization time, we use an increasing alpha. Specifically,
      //   alpha(n) = exp(pow(init_factor_, n)),
      // where |init_factor_| is chosen such that
      //   alpha(init_time_ms_) = exp(-1.0f / init_time_ms_),
      init_factor_(pow(init_time_ms_, 1.0f / init_time_ms_)),
      // |init_const_| is to a factor to help the calculation during
      // initialization phase.
      init_const_(1.0f / (init_time_ms_ -
                          pow(init_time_ms_, 1.0f - 1.0f / init_time_ms_))),
      clock_(clock) {
  UpdateAlpha(init_time_ms_);
}

SmoothingFilterImpl::~SmoothingFilterImpl() = default;

void SmoothingFilterImpl::AddSample(float sample) {
  const int64_t now_ms = clock_->TimeInMilliseconds();

  if (!first_sample_time_ms_) {
    // This is equivalent to assuming the filter has been receiving the same
    // value as the first sample since time -infinity.
    state_ = last_sample_ = sample;
    first_sample_time_ms_ = rtc::Optional<int64_t>(now_ms);
    last_state_time_ms_ = now_ms;
    return;
  }

  ExtrapolateLastSample(now_ms);
  last_sample_ = sample;
}

rtc::Optional<float> SmoothingFilterImpl::GetAverage() {
  if (!first_sample_time_ms_)
    return rtc::Optional<float>();
  ExtrapolateLastSample(clock_->TimeInMilliseconds());
  return rtc::Optional<float>(state_);
}

bool SmoothingFilterImpl::SetTimeConstantMs(int time_constant_ms) {
  if (!first_sample_time_ms_ ||
      last_state_time_ms_ < *first_sample_time_ms_ + init_time_ms_) {
    return false;
  }
  UpdateAlpha(time_constant_ms);
  return true;
}

void SmoothingFilterImpl::UpdateAlpha(int time_constant_ms) {
  alpha_ = exp(-1.0f / time_constant_ms);
}

void SmoothingFilterImpl::ExtrapolateLastSample(int64_t time_ms) {
  RTC_DCHECK_GE(time_ms, last_state_time_ms_);
  RTC_DCHECK(first_sample_time_ms_);

  float multiplier = 0.0f;
  if (time_ms <= *first_sample_time_ms_ + init_time_ms_) {
    // Current update is to be made during initialization phase.
    // We update the state as if the |alpha| has been increased according
    //   alpha(n) = exp(pow(init_factor_, n)),
    // where n is the time (in millisecond) since the first sample received.
    // With algebraic derivation as shown in the Appendix, we can find that the
    // state can be updated in a similar manner as if alpha is a constant,
    // except for a different multiplier.
    multiplier = exp(-init_const_ *
        (pow(init_factor_,
             *first_sample_time_ms_ + init_time_ms_ - last_state_time_ms_) -
         pow(init_factor_, *first_sample_time_ms_ + init_time_ms_ - time_ms)));
  } else {
    if (last_state_time_ms_ < *first_sample_time_ms_ + init_time_ms_) {
      // The latest state update was made during initialization phase.
      // We first extrapolate to the initialization time.
      ExtrapolateLastSample(*first_sample_time_ms_ + init_time_ms_);
      // Then extrapolate the rest by the following.
    }
    multiplier = pow(alpha_, time_ms - last_state_time_ms_);
  }

  state_ = multiplier * state_ + (1.0f - multiplier) * last_sample_;
  last_state_time_ms_ = time_ms;
}

}  // namespace webrtc

// Appendix: derivation of extrapolation during initialization phase.
// (LaTeX syntax)
// Assuming
//   \begin{align}
//     y(n) &= \alpha_{n-1} y(n-1) + \left(1 - \alpha_{n-1}\right) x(m) \\*
//          &= \left(\prod_{i=m}^{n-1} \alpha_i\right) y(m) +
//             \left(1 - \prod_{i=m}^{n-1} \alpha_i \right) x(m)
//   \end{align}
// Taking $\alpha_{n} = \exp{\gamma^n}$, $\gamma$ denotes init\_factor\_, the
// multiplier becomes
//   \begin{align}
//     \prod_{i=m}^{n-1} \alpha_i
//     &= \exp\left(\prod_{i=m}^{n-1} \gamma^i \right) \\*
//     &= \exp\left(\frac{\gamma^m - \gamma^n}{1 - \gamma} \right)
//   \end{align}
// We know $\gamma = T^\frac{1}{T}$, where $T$ denotes init\_time\_ms\_. Then
// $1 - \gamma$ approaches zero when $T$ increases. This can cause numerical
// difficulties. We multiply $T$ to both numerator and denominator in the
// fraction. See.
//   \begin{align}
//     \frac{\gamma^m - \gamma^n}{1 - \gamma}
//     &= \frac{T^\frac{T-m}{T} - T^\frac{T-n}{T}}{T - T^{1-\frac{1}{T}}}
//   \end{align}
