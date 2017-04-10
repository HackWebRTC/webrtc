/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/suppression_gain.h"

#include "webrtc/typedefs.h"
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include <math.h>
#include <algorithm>
#include <functional>
#include <numeric>

#include "webrtc/base/checks.h"

namespace webrtc {
namespace {

void GainPostProcessing(std::array<float, kFftLengthBy2Plus1>* gain_squared) {
  // Limit the low frequency gains to avoid the impact of the high-pass filter
  // on the lower-frequency gain influencing the overall achieved gain.
  (*gain_squared)[1] = std::min((*gain_squared)[1], (*gain_squared)[2]);
  (*gain_squared)[0] = (*gain_squared)[1];

  // Limit the high frequency gains to avoid the impact of the anti-aliasing
  // filter on the upper-frequency gains influencing the overall achieved
  // gain. TODO(peah): Update this when new anti-aliasing filters are
  // implemented.
  constexpr size_t kAntiAliasingImpactLimit = (64 * 2000) / 8000;
  std::for_each(gain_squared->begin() + kAntiAliasingImpactLimit,
                gain_squared->end() - 1,
                [gain_squared, kAntiAliasingImpactLimit](float& a) {
                  a = std::min(a, (*gain_squared)[kAntiAliasingImpactLimit]);
                });
  (*gain_squared)[kFftLengthBy2] = (*gain_squared)[kFftLengthBy2Minus1];
}

constexpr int kNumIterations = 2;
constexpr float kEchoMaskingMargin = 1.f / 20.f;
constexpr float kBandMaskingFactor = 1.f / 10.f;
constexpr float kTimeMaskingFactor = 1.f / 10.f;

}  // namespace

namespace aec3 {

#if defined(WEBRTC_ARCH_X86_FAMILY)

// Optimized SSE2 code for the gain computation.
// TODO(peah): Add further optimizations, in particular for the divisions.
void ComputeGains_SSE2(
    const std::array<float, kFftLengthBy2Plus1>& nearend_power,
    const std::array<float, kFftLengthBy2Plus1>& residual_echo_power,
    const std::array<float, kFftLengthBy2Plus1>& comfort_noise_power,
    float strong_nearend_margin,
    std::array<float, kFftLengthBy2Minus1>* previous_gain_squared,
    std::array<float, kFftLengthBy2Minus1>* previous_masker,
    std::array<float, kFftLengthBy2Plus1>* gain) {
  std::array<float, kFftLengthBy2Minus1> masker;
  std::array<float, kFftLengthBy2Minus1> same_band_masker;
  std::array<float, kFftLengthBy2Minus1> one_by_residual_echo_power;
  std::array<bool, kFftLengthBy2Minus1> strong_nearend;
  std::array<float, kFftLengthBy2Plus1> neighboring_bands_masker;
  std::array<float, kFftLengthBy2Plus1>* gain_squared = gain;

  // Precompute 1/residual_echo_power.
  std::transform(residual_echo_power.begin() + 1, residual_echo_power.end() - 1,
                 one_by_residual_echo_power.begin(),
                 [](float a) { return a > 0.f ? 1.f / a : -1.f; });

  // Precompute indicators for bands with strong nearend.
  std::transform(
      residual_echo_power.begin() + 1, residual_echo_power.end() - 1,
      nearend_power.begin() + 1, strong_nearend.begin(),
      [&](float a, float b) { return a <= strong_nearend_margin * b; });

  //  Precompute masker for the same band.
  std::transform(comfort_noise_power.begin() + 1, comfort_noise_power.end() - 1,
                 previous_masker->begin(), same_band_masker.begin(),
                 [&](float a, float b) { return a + kTimeMaskingFactor * b; });

  for (int k = 0; k < kNumIterations; ++k) {
    if (k == 0) {
      // Add masker from the same band.
      std::copy(same_band_masker.begin(), same_band_masker.end(),
                masker.begin());
    } else {
      // Add masker for neighboring bands.
      std::transform(nearend_power.begin(), nearend_power.end(),
                     gain_squared->begin(), neighboring_bands_masker.begin(),
                     std::multiplies<float>());
      std::transform(neighboring_bands_masker.begin(),
                     neighboring_bands_masker.end(),
                     comfort_noise_power.begin(),
                     neighboring_bands_masker.begin(), std::plus<float>());
      std::transform(
          neighboring_bands_masker.begin(), neighboring_bands_masker.end() - 2,
          neighboring_bands_masker.begin() + 2, masker.begin(),
          [&](float a, float b) { return kBandMaskingFactor * (a + b); });

      // Add masker from the same band.
      std::transform(same_band_masker.begin(), same_band_masker.end(),
                     masker.begin(), masker.begin(), std::plus<float>());
    }

    // Compute new gain as:
    // G2(t,f) = (comfort_noise_power(t,f) + G2(t-1)*nearend_power(t-1)) *
    //           kTimeMaskingFactor
    //           * kEchoMaskingMargin / residual_echo_power(t,f).
    // or
    // G2(t,f) = ((comfort_noise_power(t,f) + G2(t-1) *
    //             nearend_power(t-1)) * kTimeMaskingFactor +
    //            (comfort_noise_power(t, f-1) + comfort_noise_power(t, f+1) +
    //             (G2(t,f-1)*nearend_power(t, f-1) +
    //              G2(t,f+1)*nearend_power(t, f+1)) *
    //             kTimeMaskingFactor) * kBandMaskingFactor)
    //           * kEchoMaskingMargin / residual_echo_power(t,f).
    std::transform(
        masker.begin(), masker.end(), one_by_residual_echo_power.begin(),
        gain_squared->begin() + 1, [&](float a, float b) {
          return b >= 0 ? std::min(kEchoMaskingMargin * a * b, 1.f) : 1.f;
        });

    // Limit gain for bands with strong nearend.
    std::transform(gain_squared->begin() + 1, gain_squared->end() - 1,
                   strong_nearend.begin(), gain_squared->begin() + 1,
                   [](float a, bool b) { return b ? 1.f : a; });

    // Limit the allowed gain update over time.
    std::transform(gain_squared->begin() + 1, gain_squared->end() - 1,
                   previous_gain_squared->begin(), gain_squared->begin() + 1,
                   [](float a, float b) {
                     return b < 0.001f ? std::min(a, 0.001f)
                                       : std::min(a, b * 2.f);
                   });

    // Process the gains to avoid artefacts caused by gain realization in the
    // filterbank and impact of external pre-processing of the signal.
    GainPostProcessing(gain_squared);
  }

  std::copy(gain_squared->begin() + 1, gain_squared->end() - 1,
            previous_gain_squared->begin());

  std::transform(gain_squared->begin() + 1, gain_squared->end() - 1,
                 nearend_power.begin() + 1, previous_masker->begin(),
                 std::multiplies<float>());
  std::transform(previous_masker->begin(), previous_masker->end(),
                 comfort_noise_power.begin() + 1, previous_masker->begin(),
                 std::plus<float>());

  for (size_t k = 0; k < kFftLengthBy2; k += 4) {
    __m128 g = _mm_loadu_ps(&(*gain_squared)[k]);
    g = _mm_sqrt_ps(g);
    _mm_storeu_ps(&(*gain)[k], g);
  }

  (*gain)[kFftLengthBy2] = sqrtf((*gain)[kFftLengthBy2]);
}

#endif

void ComputeGains(
    const std::array<float, kFftLengthBy2Plus1>& nearend_power,
    const std::array<float, kFftLengthBy2Plus1>& residual_echo_power,
    const std::array<float, kFftLengthBy2Plus1>& comfort_noise_power,
    float strong_nearend_margin,
    std::array<float, kFftLengthBy2Minus1>* previous_gain_squared,
    std::array<float, kFftLengthBy2Minus1>* previous_masker,
    std::array<float, kFftLengthBy2Plus1>* gain) {
  std::array<float, kFftLengthBy2Minus1> masker;
  std::array<float, kFftLengthBy2Minus1> same_band_masker;
  std::array<float, kFftLengthBy2Minus1> one_by_residual_echo_power;
  std::array<bool, kFftLengthBy2Minus1> strong_nearend;
  std::array<float, kFftLengthBy2Plus1> neighboring_bands_masker;
  std::array<float, kFftLengthBy2Plus1>* gain_squared = gain;

  // Precompute 1/residual_echo_power.
  std::transform(residual_echo_power.begin() + 1, residual_echo_power.end() - 1,
                 one_by_residual_echo_power.begin(),
                 [](float a) { return a > 0.f ? 1.f / a : -1.f; });

  // Precompute indicators for bands with strong nearend.
  std::transform(
      residual_echo_power.begin() + 1, residual_echo_power.end() - 1,
      nearend_power.begin() + 1, strong_nearend.begin(),
      [&](float a, float b) { return a <= strong_nearend_margin * b; });

  //  Precompute masker for the same band.
  std::transform(comfort_noise_power.begin() + 1, comfort_noise_power.end() - 1,
                 previous_masker->begin(), same_band_masker.begin(),
                 [&](float a, float b) { return a + kTimeMaskingFactor * b; });

  for (int k = 0; k < kNumIterations; ++k) {
    if (k == 0) {
      // Add masker from the same band.
      std::copy(same_band_masker.begin(), same_band_masker.end(),
                masker.begin());
    } else {
      // Add masker for neightboring bands.
      std::transform(nearend_power.begin(), nearend_power.end(),
                     gain_squared->begin(), neighboring_bands_masker.begin(),
                     std::multiplies<float>());
      std::transform(neighboring_bands_masker.begin(),
                     neighboring_bands_masker.end(),
                     comfort_noise_power.begin(),
                     neighboring_bands_masker.begin(), std::plus<float>());
      std::transform(
          neighboring_bands_masker.begin(), neighboring_bands_masker.end() - 2,
          neighboring_bands_masker.begin() + 2, masker.begin(),
          [&](float a, float b) { return kBandMaskingFactor * (a + b); });

      // Add masker from the same band.
      std::transform(same_band_masker.begin(), same_band_masker.end(),
                     masker.begin(), masker.begin(), std::plus<float>());
    }

    // Compute new gain as:
    // G2(t,f) = (comfort_noise_power(t,f) + G2(t-1)*nearend_power(t-1)) *
    //           kTimeMaskingFactor
    //           * kEchoMaskingMargin / residual_echo_power(t,f).
    // or
    // G2(t,f) = ((comfort_noise_power(t,f) + G2(t-1) *
    //             nearend_power(t-1)) * kTimeMaskingFactor +
    //            (comfort_noise_power(t, f-1) + comfort_noise_power(t, f+1) +
    //             (G2(t,f-1)*nearend_power(t, f-1) +
    //              G2(t,f+1)*nearend_power(t, f+1)) *
    //             kTimeMaskingFactor) * kBandMaskingFactor)
    //           * kEchoMaskingMargin / residual_echo_power(t,f).
    std::transform(
        masker.begin(), masker.end(), one_by_residual_echo_power.begin(),
        gain_squared->begin() + 1, [&](float a, float b) {
          return b >= 0 ? std::min(kEchoMaskingMargin * a * b, 1.f) : 1.f;
        });

    // Limit gain for bands with strong nearend.
    std::transform(gain_squared->begin() + 1, gain_squared->end() - 1,
                   strong_nearend.begin(), gain_squared->begin() + 1,
                   [](float a, bool b) { return b ? 1.f : a; });

    // Limit the allowed gain update over time.
    std::transform(gain_squared->begin() + 1, gain_squared->end() - 1,
                   previous_gain_squared->begin(), gain_squared->begin() + 1,
                   [](float a, float b) {
                     return b < 0.001f ? std::min(a, 0.001f)
                                       : std::min(a, b * 2.f);
                   });

    // Process the gains to avoid artefacts caused by gain realization in the
    // filterbank and impact of external pre-processing of the signal.
    GainPostProcessing(gain_squared);
  }

  std::copy(gain_squared->begin() + 1, gain_squared->end() - 1,
            previous_gain_squared->begin());

  std::transform(gain_squared->begin() + 1, gain_squared->end() - 1,
                 nearend_power.begin() + 1, previous_masker->begin(),
                 std::multiplies<float>());
  std::transform(previous_masker->begin(), previous_masker->end(),
                 comfort_noise_power.begin() + 1, previous_masker->begin(),
                 std::plus<float>());

  std::transform(gain_squared->begin(), gain_squared->end(), gain->begin(),
                 [](float a) { return sqrtf(a); });
}

}  // namespace aec3

// Computes an upper bound on the gain to apply for high frequencies.
float HighFrequencyGainBound(bool saturated_echo,
                             const std::vector<std::vector<float>>& render) {
  if (render.size() == 1) {
    return 1.f;
  }

  // Always attenuate the upper bands when there is saturated echo.
  if (saturated_echo) {
    return 0.001f;
  }

  // Compute the upper and lower band energies.
  float low_band_energy =
      std::accumulate(render[0].begin(), render[0].end(), 0.f,
                      [](float a, float b) -> float { return a + b * b; });
  float high_band_energies = 0.f;
  for (size_t k = 1; k < render.size(); ++k) {
    high_band_energies = std::max(
        high_band_energies,
        std::accumulate(render[k].begin(), render[k].end(), 0.f,
                        [](float a, float b) -> float { return a + b * b; }));
  }

  // If there is more power in the lower frequencies than the upper frequencies,
  // or if the power in upper frequencies  is low, do not bound the gain in the
  // upper bands.
  if (high_band_energies < low_band_energy ||
      high_band_energies < kSubBlockSize * 10.f * 10.f) {
    return 1.f;
  }

  // In all other cases, bound the gain for upper frequencies.
  RTC_DCHECK_LE(low_band_energy, high_band_energies);
  return 0.01f * sqrtf(low_band_energy / high_band_energies);
}

SuppressionGain::SuppressionGain(Aec3Optimization optimization)
    : optimization_(optimization) {
  previous_gain_squared_.fill(1.f);
  previous_masker_.fill(0.f);
}

void SuppressionGain::GetGain(
    const std::array<float, kFftLengthBy2Plus1>& nearend_power,
    const std::array<float, kFftLengthBy2Plus1>& residual_echo_power,
    const std::array<float, kFftLengthBy2Plus1>& comfort_noise_power,
    bool saturated_echo,
    const std::vector<std::vector<float>>& render,
    size_t num_capture_bands,
    bool force_zero_gain,
    float* high_bands_gain,
    std::array<float, kFftLengthBy2Plus1>* low_band_gain) {
  RTC_DCHECK(high_bands_gain);
  RTC_DCHECK(low_band_gain);

  if (force_zero_gain) {
    previous_gain_squared_.fill(0.f);
    std::copy(comfort_noise_power.begin() + 1, comfort_noise_power.end() - 1,
              previous_masker_.begin());
    low_band_gain->fill(0.f);
    *high_bands_gain = 0.f;
    return;
  }

  // Choose margin to use.
  const float margin = saturated_echo ? 0.001f : 0.01f;
  switch (optimization_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
    case Aec3Optimization::kSse2:
      aec3::ComputeGains_SSE2(
          nearend_power, residual_echo_power, comfort_noise_power, margin,
          &previous_gain_squared_, &previous_masker_, low_band_gain);
      break;
#endif
    default:
      aec3::ComputeGains(nearend_power, residual_echo_power,
                         comfort_noise_power, margin, &previous_gain_squared_,
                         &previous_masker_, low_band_gain);
  }

  if (num_capture_bands > 1) {
    // Compute the gain for upper frequencies.
    const float min_high_band_gain =
        HighFrequencyGainBound(saturated_echo, render);
    *high_bands_gain =
        *std::min_element(low_band_gain->begin() + 32, low_band_gain->end());

    *high_bands_gain = std::min(*high_bands_gain, min_high_band_gain);

  } else {
    *high_bands_gain = 1.f;
  }
}

}  // namespace webrtc
