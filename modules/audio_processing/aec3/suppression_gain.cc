/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/suppression_gain.h"

#include "typedefs.h"  // NOLINT(build/include)
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include <math.h>
#include <algorithm>
#include <functional>
#include <numeric>

#include "modules/audio_processing/aec3/vector_math.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// Reduce gain to avoid narrow band echo leakage.
void NarrowBandAttenuation(int narrow_bin,
                           const std::array<float, kFftLengthBy2Plus1>& nearend,
                           const std::array<float, kFftLengthBy2Plus1>& echo,
                           std::array<float, kFftLengthBy2Plus1>* gain) {
  // TODO(peah): Verify that the condition below is not too conservative.
  if (10.f * echo[narrow_bin] > nearend[narrow_bin]) {
    const int upper_bin =
        std::min(narrow_bin + 6, static_cast<int>(kFftLengthBy2Plus1 - 1));
    for (int k = std::max(0, narrow_bin - 6); k <= upper_bin; ++k) {
      (*gain)[k] = std::min((*gain)[k], 0.001f);
    }
  }
}

// Adjust the gains according to the presence of known external filters.
void AdjustForExternalFilters(std::array<float, kFftLengthBy2Plus1>* gain) {
  // Limit the low frequency gains to avoid the impact of the high-pass filter
  // on the lower-frequency gain influencing the overall achieved gain.
  (*gain)[0] = (*gain)[1] = std::min((*gain)[1], (*gain)[2]);

  // Limit the high frequency gains to avoid the impact of the anti-aliasing
  // filter on the upper-frequency gains influencing the overall achieved
  // gain. TODO(peah): Update this when new anti-aliasing filters are
  // implemented.
  constexpr size_t kAntiAliasingImpactLimit = (64 * 2000) / 8000;
  const float min_upper_gain = (*gain)[kAntiAliasingImpactLimit];
  std::for_each(
      gain->begin() + kAntiAliasingImpactLimit, gain->end() - 1,
      [min_upper_gain](float& a) { a = std::min(a, min_upper_gain); });
  (*gain)[kFftLengthBy2] = (*gain)[kFftLengthBy2Minus1];
}

// Computes the gain to apply for the bands beyond the first band.
float UpperBandsGain(
    const rtc::Optional<int>& narrow_peak_band,
    bool saturated_echo,
    const std::vector<std::vector<float>>& render,
    const std::array<float, kFftLengthBy2Plus1>& low_band_gain) {
  RTC_DCHECK_LT(0, render.size());
  if (render.size() == 1) {
    return 1.f;
  }

  if (narrow_peak_band &&
      (*narrow_peak_band > static_cast<int>(kFftLengthBy2Plus1 - 10))) {
    return 0.001f;
  }

  constexpr size_t kLowBandGainLimit = kFftLengthBy2 / 2;
  const float gain_below_8_khz = *std::min_element(
      low_band_gain.begin() + kLowBandGainLimit, low_band_gain.end());

  // Always attenuate the upper bands when there is saturated echo.
  if (saturated_echo) {
    return std::min(0.001f, gain_below_8_khz);
  }

  // Compute the upper and lower band energies.
  const auto sum_of_squares = [](float a, float b) { return a + b * b; };
  const float low_band_energy =
      std::accumulate(render[0].begin(), render[0].end(), 0.f, sum_of_squares);
  float high_band_energy = 0.f;
  for (size_t k = 1; k < render.size(); ++k) {
    const float energy = std::accumulate(render[k].begin(), render[k].end(),
                                         0.f, sum_of_squares);
    high_band_energy = std::max(high_band_energy, energy);
  }

  // If there is more power in the lower frequencies than the upper frequencies,
  // or if the power in upper frequencies is low, do not bound the gain in the
  // upper bands.
  float anti_howling_gain;
  constexpr float kThreshold = kBlockSize * 10.f * 10.f / 4.f;
  if (high_band_energy < std::max(low_band_energy, kThreshold)) {
    anti_howling_gain = 1.f;
  } else {
    // In all other cases, bound the gain for upper frequencies.
    RTC_DCHECK_LE(low_band_energy, high_band_energy);
    RTC_DCHECK_NE(0.f, high_band_energy);
    anti_howling_gain = 0.01f * sqrtf(low_band_energy / high_band_energy);
  }

  // Choose the gain as the minimum of the lower and upper gains.
  return std::min(gain_below_8_khz, anti_howling_gain);
}

// Computes the gain to reduce the echo to a non audible level.
void GainToNoAudibleEcho(
    const EchoCanceller3Config& config,
    bool low_noise_render,
    bool saturated_echo,
    bool saturating_echo_path,
    bool linear_echo_estimate,
    const std::array<float, kFftLengthBy2Plus1>& nearend,
    const std::array<float, kFftLengthBy2Plus1>& echo,
    const std::array<float, kFftLengthBy2Plus1>& masker,
    const std::array<float, kFftLengthBy2Plus1>& min_gain,
    const std::array<float, kFftLengthBy2Plus1>& max_gain,
    const std::array<float, kFftLengthBy2Plus1>& one_by_echo,
    std::array<float, kFftLengthBy2Plus1>* gain) {
  float nearend_masking_margin = 0.f;
  if (linear_echo_estimate) {
    nearend_masking_margin =
        low_noise_render
            ? config.gain_mask.m9
            : (saturated_echo ? config.gain_mask.m2 : config.gain_mask.m3);
  } else {
    nearend_masking_margin = config.gain_mask.m7;
  }

  RTC_DCHECK_LE(0.f, nearend_masking_margin);
  RTC_DCHECK_GT(1.f, nearend_masking_margin);
  const float one_by_one_minus_nearend_masking_margin =
      1.f / (1.0f - nearend_masking_margin);

  const float masker_margin =
      linear_echo_estimate ? config.gain_mask.m1 : config.gain_mask.m8;

  for (size_t k = 0; k < gain->size(); ++k) {
    const float unity_gain_masker = std::max(nearend[k], masker[k]);
    RTC_DCHECK_LE(0.f, nearend_masking_margin * unity_gain_masker);
    if (echo[k] <= nearend_masking_margin * unity_gain_masker ||
        unity_gain_masker <= 0.f) {
      (*gain)[k] = 1.f;
    } else {
      RTC_DCHECK_LT(0.f, unity_gain_masker);
      (*gain)[k] = std::max(0.f, (1.f - 5.f * echo[k] / unity_gain_masker) *
                                     one_by_one_minus_nearend_masking_margin);
      (*gain)[k] =
          std::max(masker_margin * masker[k] * one_by_echo[k], (*gain)[k]);
    }

    (*gain)[k] = std::min(std::max((*gain)[k], min_gain[k]), max_gain[k]);
  }
}

// TODO(peah): Make adaptive to take the actual filter error into account.
constexpr size_t kUpperAccurateBandPlus1 = 29;

// Computes the signal output power that masks the echo signal.
void MaskingPower(const EchoCanceller3Config& config,
                  const std::array<float, kFftLengthBy2Plus1>& nearend,
                  const std::array<float, kFftLengthBy2Plus1>& comfort_noise,
                  const std::array<float, kFftLengthBy2Plus1>& last_masker,
                  const std::array<float, kFftLengthBy2Plus1>& gain,
                  std::array<float, kFftLengthBy2Plus1>* masker) {
  std::array<float, kFftLengthBy2Plus1> side_band_masker;
  float max_nearend_after_gain = 0.f;
  for (size_t k = 0; k < gain.size(); ++k) {
    const float nearend_after_gain = nearend[k] * gain[k];
    max_nearend_after_gain =
        std::max(max_nearend_after_gain, nearend_after_gain);
    side_band_masker[k] = nearend_after_gain + comfort_noise[k];
    (*masker)[k] = comfort_noise[k] + config.gain_mask.m4 * last_masker[k];
  }

  // Apply masking only between lower frequency bands.
  RTC_DCHECK_LT(kUpperAccurateBandPlus1, gain.size());
  for (size_t k = 1; k < kUpperAccurateBandPlus1; ++k) {
    (*masker)[k] += config.gain_mask.m5 *
                    (side_band_masker[k - 1] + side_band_masker[k + 1]);
  }

  // Add full-band masking as a minimum value for the masker.
  const float min_masker = max_nearend_after_gain * config.gain_mask.m6;
  std::for_each(masker->begin(), masker->end(),
                [min_masker](float& a) { a = std::max(a, min_masker); });
}

// Limits the gain in the frequencies for which the adaptive filter has not
// converged. Currently, these frequencies are not hardcoded to the frequencies
// which are typically not excited by speech.
// TODO(peah): Make adaptive to take the actual filter error into account.
void AdjustNonConvergedFrequencies(
    std::array<float, kFftLengthBy2Plus1>* gain) {
  constexpr float oneByBandsInSum =
      1 / static_cast<float>(kUpperAccurateBandPlus1 - 20);
  const float hf_gain_bound =
      std::accumulate(gain->begin() + 20,
                      gain->begin() + kUpperAccurateBandPlus1, 0.f) *
      oneByBandsInSum;

  std::for_each(gain->begin() + kUpperAccurateBandPlus1, gain->end(),
                [hf_gain_bound](float& a) { a = std::min(a, hf_gain_bound); });
}

}  // namespace

// TODO(peah): Add further optimizations, in particular for the divisions.
void SuppressionGain::LowerBandGain(
    bool low_noise_render,
    const rtc::Optional<int>& narrow_peak_band,
    const AecState& aec_state,
    const std::array<float, kFftLengthBy2Plus1>& nearend,
    const std::array<float, kFftLengthBy2Plus1>& echo,
    const std::array<float, kFftLengthBy2Plus1>& comfort_noise,
    std::array<float, kFftLengthBy2Plus1>* gain) {
  const bool saturated_echo = aec_state.SaturatedEcho();
  const bool saturating_echo_path = aec_state.SaturatingEchoPath();
  const bool linear_echo_estimate = aec_state.UsableLinearEstimate();

  // Count the number of blocks since saturation.
  no_saturation_counter_ = saturated_echo ? 0 : no_saturation_counter_ + 1;

  // Precompute 1/echo (note that when the echo is zero, the precomputed value
  // is never used).
  std::array<float, kFftLengthBy2Plus1> one_by_echo;
  std::transform(echo.begin(), echo.end(), one_by_echo.begin(),
                 [](float a) { return a > 0.f ? 1.f / a : 1.f; });

  // Compute the minimum gain as the attenuating gain to put the signal just
  // above the zero sample values.
  std::array<float, kFftLengthBy2Plus1> min_gain;
  const float min_echo_power =
      low_noise_render ? config_.echo_audibility.low_render_limit
                       : config_.echo_audibility.normal_render_limit;
  if (no_saturation_counter_ > 10) {
    for (size_t k = 0; k < nearend.size(); ++k) {
      const float denom = std::min(nearend[k], echo[k]);
      min_gain[k] = denom > 0.f ? min_echo_power / denom : 1.f;
      min_gain[k] = std::min(min_gain[k], 1.f);
    }
  } else {
    min_gain.fill(0.f);
  }

  // Compute the maximum gain by limiting the gain increase from the previous
  // gain.
  std::array<float, kFftLengthBy2Plus1> max_gain;
  for (size_t k = 0; k < gain->size(); ++k) {
    max_gain[k] = std::min(std::max(last_gain_[k] * gain_increase_[k],
                                    config_.gain_updates.floor_first_increase),
                           1.f);
  }

  // Iteratively compute the gain required to attenuate the echo to a non
  // noticeable level.
  gain->fill(0.f);
  for (int k = 0; k < 2; ++k) {
    std::array<float, kFftLengthBy2Plus1> masker;
    MaskingPower(config_, nearend, comfort_noise, last_masker_, *gain, &masker);
    GainToNoAudibleEcho(config_, low_noise_render, saturated_echo,
                        saturating_echo_path, linear_echo_estimate, nearend,
                        echo, masker, min_gain, max_gain, one_by_echo, gain);
    AdjustForExternalFilters(gain);
    if (narrow_peak_band) {
      NarrowBandAttenuation(*narrow_peak_band, nearend, echo, gain);
    }
  }

  // Adjust the gain for frequencies which have not yet converged.
  AdjustNonConvergedFrequencies(gain);

  // Update the allowed maximum gain increase.
  UpdateGainIncrease(low_noise_render, linear_echo_estimate, echo, *gain);

  // Adjust gain dynamics.
  const float gain_bound =
      std::max(0.001f, *std::min_element(gain->begin(), gain->end()) * 10000.f);
  std::for_each(gain->begin(), gain->end(),
                [gain_bound](float& a) { a = std::min(a, gain_bound); });

  // Store data required for the gain computation of the next block.
  std::copy(echo.begin(), echo.end(), last_echo_.begin());
  std::copy(gain->begin(), gain->end(), last_gain_.begin());
  MaskingPower(config_, nearend, comfort_noise, last_masker_, *gain,
               &last_masker_);
  aec3::VectorMath(optimization_).Sqrt(*gain);
}

SuppressionGain::SuppressionGain(const EchoCanceller3Config& config,
                                 Aec3Optimization optimization)
    : optimization_(optimization),
      config_(config),
      state_change_duration_blocks_(
          static_cast<int>(config_.filter.config_change_duration_blocks)) {
  RTC_DCHECK_LT(0, state_change_duration_blocks_);
  one_by_state_change_duration_blocks_ = 1.f / state_change_duration_blocks_;
  last_gain_.fill(1.f);
  last_masker_.fill(0.f);
  gain_increase_.fill(1.f);
  last_echo_.fill(0.f);
}

void SuppressionGain::GetGain(
    const std::array<float, kFftLengthBy2Plus1>& nearend,
    const std::array<float, kFftLengthBy2Plus1>& echo,
    const std::array<float, kFftLengthBy2Plus1>& comfort_noise,
    const RenderSignalAnalyzer& render_signal_analyzer,
    const AecState& aec_state,
    const std::vector<std::vector<float>>& render,
    float* high_bands_gain,
    std::array<float, kFftLengthBy2Plus1>* low_band_gain) {
  RTC_DCHECK(high_bands_gain);
  RTC_DCHECK(low_band_gain);

  // Compute gain for the lower band.
  bool low_noise_render = low_render_detector_.Detect(render);
  const rtc::Optional<int> narrow_peak_band =
      render_signal_analyzer.NarrowPeakBand();
  LowerBandGain(low_noise_render, narrow_peak_band, aec_state, nearend, echo,
                comfort_noise, low_band_gain);

  const float gain_upper_bound = aec_state.SuppressionGainLimit();
  if (gain_upper_bound < 1.f) {
    for (size_t k = 0; k < low_band_gain->size(); ++k) {
      (*low_band_gain)[k] = std::min((*low_band_gain)[k], gain_upper_bound);
    }
  }

  // Compute the gain for the upper bands.
  *high_bands_gain = UpperBandsGain(narrow_peak_band, aec_state.SaturatedEcho(),
                                    render, *low_band_gain);
}

void SuppressionGain::SetInitialState(bool state) {
  initial_state_ = state;
  if (state) {
    initial_state_change_counter_ = state_change_duration_blocks_;
  } else {
    initial_state_change_counter_ = 0;
  }
}

void SuppressionGain::UpdateGainIncrease(
    bool low_noise_render,
    bool linear_echo_estimate,
    const std::array<float, kFftLengthBy2Plus1>& echo,
    const std::array<float, kFftLengthBy2Plus1>& new_gain) {
  float max_inc;
  float max_dec;
  float rate_inc;
  float rate_dec;
  float min_inc;
  float min_dec;

  RTC_DCHECK_GE(state_change_duration_blocks_, initial_state_change_counter_);
  if (initial_state_change_counter_ > 0) {
    if (--initial_state_change_counter_ == 0) {
      initial_state_ = false;
    }
  }
  RTC_DCHECK_LE(0, initial_state_change_counter_);

  // EchoCanceller3Config::GainUpdates
  auto& p = config_.gain_updates;
  if (!linear_echo_estimate) {
    max_inc = p.nonlinear.max_inc;
    max_dec = p.nonlinear.max_dec;
    rate_inc = p.nonlinear.rate_inc;
    rate_dec = p.nonlinear.rate_dec;
    min_inc = p.nonlinear.min_inc;
    min_dec = p.nonlinear.min_dec;
  } else if (initial_state_ && no_saturation_counter_ > 10) {
    if (initial_state_change_counter_ > 0) {
      float change_factor =
          initial_state_change_counter_ * one_by_state_change_duration_blocks_;

      auto average = [](float from, float to, float from_weight) {
        return from * from_weight + to * (1.f - from_weight);
      };

      max_inc = average(p.initial.max_inc, p.normal.max_inc, change_factor);
      max_dec = average(p.initial.max_dec, p.normal.max_dec, change_factor);
      rate_inc = average(p.initial.rate_inc, p.normal.rate_inc, change_factor);
      rate_dec = average(p.initial.rate_dec, p.normal.rate_dec, change_factor);
      min_inc = average(p.initial.min_inc, p.normal.min_inc, change_factor);
      min_dec = average(p.initial.min_dec, p.normal.min_dec, change_factor);
    } else {
      max_inc = p.initial.max_inc;
      max_dec = p.initial.max_dec;
      rate_inc = p.initial.rate_inc;
      rate_dec = p.initial.rate_dec;
      min_inc = p.initial.min_inc;
      min_dec = p.initial.min_dec;
    }
  } else if (low_noise_render) {
    max_inc = p.low_noise.max_inc;
    max_dec = p.low_noise.max_dec;
    rate_inc = p.low_noise.rate_inc;
    rate_dec = p.low_noise.rate_dec;
    min_inc = p.low_noise.min_inc;
    min_dec = p.low_noise.min_dec;
  } else if (no_saturation_counter_ > 10) {
    max_inc = p.normal.max_inc;
    max_dec = p.normal.max_dec;
    rate_inc = p.normal.rate_inc;
    rate_dec = p.normal.rate_dec;
    min_inc = p.normal.min_inc;
    min_dec = p.normal.min_dec;
  } else {
    max_inc = p.saturation.max_inc;
    max_dec = p.saturation.max_dec;
    rate_inc = p.saturation.rate_inc;
    rate_dec = p.saturation.rate_dec;
    min_inc = p.saturation.min_inc;
    min_dec = p.saturation.min_dec;
  }

  for (size_t k = 0; k < new_gain.size(); ++k) {
    auto increase_update = [](float new_gain, float last_gain,
                              float current_inc, float max_inc, float min_inc,
                              float change_rate) {
      return new_gain > last_gain ? std::min(max_inc, current_inc * change_rate)
                                  : min_inc;
    };

    if (echo[k] > last_echo_[k]) {
      gain_increase_[k] =
          increase_update(new_gain[k], last_gain_[k], gain_increase_[k],
                          max_inc, min_inc, rate_inc);
    } else {
      gain_increase_[k] =
          increase_update(new_gain[k], last_gain_[k], gain_increase_[k],
                          max_dec, min_dec, rate_dec);
    }
  }
}

// Detects when the render signal can be considered to have low power and
// consist of stationary noise.
bool SuppressionGain::LowNoiseRenderDetector::Detect(
    const std::vector<std::vector<float>>& render) {
  float x2_sum = 0.f;
  float x2_max = 0.f;
  for (auto x_k : render[0]) {
    const float x2 = x_k * x_k;
    x2_sum += x2;
    x2_max = std::max(x2_max, x2);
  }

  constexpr float kThreshold = 50.f * 50.f * 64.f;
  const bool low_noise_render =
      average_power_ < kThreshold && x2_max < 3 * average_power_;
  average_power_ = average_power_ * 0.9f + x2_sum * 0.1f;
  return low_noise_render;
}

}  // namespace webrtc
