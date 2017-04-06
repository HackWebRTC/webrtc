/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/residual_echo_estimator.h"

#include <numeric>
#include <vector>

#include "webrtc/base/checks.h"

namespace webrtc {
namespace {

// Estimates the echo generating signal power as gated maximal power over a time
// window.
void EchoGeneratingPower(const RenderBuffer& render_buffer,
                         size_t min_delay,
                         size_t max_delay,
                         std::array<float, kFftLengthBy2Plus1>* X2) {
  X2->fill(0.f);
  for (size_t k = min_delay; k <= max_delay; ++k) {
    std::transform(X2->begin(), X2->end(), render_buffer.Spectrum(k).begin(),
                   X2->begin(),
                   [](float a, float b) { return std::max(a, b); });
  }

  // Apply soft noise gate of -78 dBFS.
  constexpr float kNoiseGatePower = 27509.42f;
  std::for_each(X2->begin(), X2->end(), [kNoiseGatePower](float& a) {
    if (kNoiseGatePower > a) {
      a = std::max(0.f, a - 0.3f * (kNoiseGatePower - a));
    }
  });
}

// Estimates the residual echo power based on the erle and the linear power
// estimate.
void LinearResidualPowerEstimate(
    const std::array<float, kFftLengthBy2Plus1>& S2_linear,
    const std::array<float, kFftLengthBy2Plus1>& erle,
    std::array<int, kFftLengthBy2Plus1>* R2_hold_counter,
    std::array<float, kFftLengthBy2Plus1>* R2) {
  std::fill(R2_hold_counter->begin(), R2_hold_counter->end(), 10.f);
  std::transform(erle.begin(), erle.end(), S2_linear.begin(), R2->begin(),
                 [](float a, float b) {
                   RTC_DCHECK_LT(0.f, a);
                   return b / a;
                 });
}

// Estimates the residual echo power based on the estimate of the echo path
// gain.
void NonLinearResidualPowerEstimate(
    const std::array<float, kFftLengthBy2Plus1>& X2,
    const std::array<float, kFftLengthBy2Plus1>& Y2,
    const std::array<float, kFftLengthBy2Plus1>& R2_old,
    std::array<int, kFftLengthBy2Plus1>* R2_hold_counter,
    std::array<float, kFftLengthBy2Plus1>* R2) {
  // Compute preliminary residual echo.
  // TODO(peah): Try to make this adaptive. Currently the gain is hardcoded to
  // 20 dB.
  std::transform(X2.begin(), X2.end(), R2->begin(),
                 [](float a) { return a * kFixedEchoPathGain; });

  for (size_t k = 0; k < R2->size(); ++k) {
    // Update hold counter.
    (*R2_hold_counter)[k] =
        R2_old[k] < (*R2)[k] ? 0 : (*R2_hold_counter)[k] + 1;

    // Compute the residual echo by holding a maximum echo powers and an echo
    // fading corresponding to a room with an RT60 value of about 50 ms.
    (*R2)[k] = (*R2_hold_counter)[k] < 2
                   ? std::max((*R2)[k], R2_old[k])
                   : std::min((*R2)[k] + R2_old[k] * 0.1f, Y2[k]);
  }
}

}  // namespace

ResidualEchoEstimator::ResidualEchoEstimator() {
  R2_old_.fill(0.f);
  R2_hold_counter_.fill(0);
}

ResidualEchoEstimator::~ResidualEchoEstimator() = default;

void ResidualEchoEstimator::Estimate(
    bool using_subtractor_output,
    const AecState& aec_state,
    const RenderBuffer& render_buffer,
    const std::array<float, kFftLengthBy2Plus1>& S2_linear,
    const std::array<float, kFftLengthBy2Plus1>& Y2,
    std::array<float, kFftLengthBy2Plus1>* R2) {
  RTC_DCHECK(R2);

  // Return zero residual echo power when a headset is detected.
  if (aec_state.HeadsetDetected()) {
    R2->fill(0.f);
    R2_old_.fill(0.f);
    R2_hold_counter_.fill(0.f);
    return;
  }

  // Estimate the echo generating signal power.
  std::array<float, kFftLengthBy2Plus1> X2;
  if (aec_state.ExternalDelay() || aec_state.FilterDelay()) {
    const int delay =
        static_cast<int>(aec_state.FilterDelay() ? *aec_state.FilterDelay()
                                                 : *aec_state.ExternalDelay());
    // Computes the spectral power over that blocks surrounding the delauy..
    EchoGeneratingPower(
        render_buffer, std::max(0, delay - 1),
        std::min(kResidualEchoPowerRenderWindowSize - 1, delay + 1), &X2);
  } else {
    // Computes the spectral power over that last 30 blocks.
    EchoGeneratingPower(render_buffer, 0,
                        kResidualEchoPowerRenderWindowSize - 1, &X2);
  }

  // Estimate the residual echo power.
  if ((aec_state.UsableLinearEstimate() && using_subtractor_output)) {
    LinearResidualPowerEstimate(S2_linear, aec_state.Erle(), &R2_hold_counter_,
                                R2);
  } else {
    NonLinearResidualPowerEstimate(X2, Y2, R2_old_, &R2_hold_counter_, R2);
  }

  // If the echo is saturated, estimate the echo power as the maximum echo power
  // with a leakage factor.
  if (aec_state.SaturatedEcho()) {
    constexpr float kSaturationLeakageFactor = 100.f;
    R2->fill((*std::max_element(R2->begin(), R2->end())) *
             kSaturationLeakageFactor);
  }

  std::copy(R2->begin(), R2->end(), R2_old_.begin());
}

}  // namespace webrtc
