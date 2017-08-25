/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/aec_state.h"

#include <math.h>
#include <numeric>
#include <vector>

#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"
#include "webrtc/rtc_base/array_view.h"
#include "webrtc/rtc_base/atomicops.h"
#include "webrtc/rtc_base/checks.h"

namespace webrtc {
namespace {

// Computes delay of the adaptive filter.
rtc::Optional<size_t> EstimateFilterDelay(
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        adaptive_filter_frequency_response) {
  const auto& H2 = adaptive_filter_frequency_response;

  size_t reliable_delays_sum = 0;
  size_t num_reliable_delays = 0;

  constexpr size_t kUpperBin = kFftLengthBy2 - 5;
  constexpr float kMinPeakMargin = 10.f;
  const size_t kTailPartition = H2.size() - 1;
  for (size_t k = 1; k < kUpperBin; ++k) {
    // Find the maximum of H2[j].
    int peak = 0;
    for (size_t j = 0; j < H2.size(); ++j) {
      if (H2[j][k] > H2[peak][k]) {
        peak = j;
      }
    }

    // Count the peak as a delay only if the peak is sufficiently larger than
    // the tail.
    if (kMinPeakMargin * H2[kTailPartition][k] < H2[peak][k]) {
      reliable_delays_sum += peak;
      ++num_reliable_delays;
    }
  }

  // Return no delay if not sufficient delays have been found.
  if (num_reliable_delays < 21) {
    return rtc::Optional<size_t>();
  }

  const size_t delay = reliable_delays_sum / num_reliable_delays;
  // Sanity check that the peak is not caused by a false strong DC-component in
  // the filter.
  for (size_t k = 1; k < kUpperBin; ++k) {
    if (H2[delay][k] > H2[delay][0]) {
      RTC_DCHECK_GT(H2.size(), delay);
      return rtc::Optional<size_t>(delay);
    }
  }
  return rtc::Optional<size_t>();
}

constexpr int kEchoPathChangeCounterInitial = kNumBlocksPerSecond / 5;
constexpr int kEchoPathChangeCounterMax = 2 * kNumBlocksPerSecond;

}  // namespace

int AecState::instance_count_ = 0;

AecState::AecState(const AudioProcessing::Config::EchoCanceller3& config)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      erle_estimator_(config.param.erle.min,
                      config.param.erle.max_l,
                      config.param.erle.max_h),
      echo_path_change_counter_(kEchoPathChangeCounterInitial),
      config_(config),
      reverb_decay_(config_.param.ep_strength.default_len) {}

AecState::~AecState() = default;

void AecState::HandleEchoPathChange(
    const EchoPathVariability& echo_path_variability) {
  if (echo_path_variability.AudioPathChanged()) {
    blocks_since_last_saturation_ = 0;
    usable_linear_estimate_ = false;
    echo_leakage_detected_ = false;
    capture_signal_saturation_ = false;
    echo_saturation_ = false;
    previous_max_sample_ = 0.f;

    if (echo_path_variability.delay_change) {
      force_zero_gain_counter_ = 0;
      blocks_with_filter_adaptation_ = 0;
      render_received_ = false;
      force_zero_gain_ = true;
      echo_path_change_counter_ = kEchoPathChangeCounterMax;
    }
    if (echo_path_variability.gain_change) {
      echo_path_change_counter_ = kEchoPathChangeCounterInitial;
    }
  }
}

void AecState::Update(const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                          adaptive_filter_frequency_response,
                      const std::array<float, kAdaptiveFilterTimeDomainLength>&
                          adaptive_filter_impulse_response,
                      const rtc::Optional<size_t>& external_delay_samples,
                      const RenderBuffer& render_buffer,
                      const std::array<float, kFftLengthBy2Plus1>& E2_main,
                      const std::array<float, kFftLengthBy2Plus1>& Y2,
                      rtc::ArrayView<const float> x,
                      const std::array<float, kBlockSize>& s,
                      bool echo_leakage_detected) {
  // Update the echo audibility evaluator.
  echo_audibility_.Update(x, s);

  // Store input parameters.
  echo_leakage_detected_ = echo_leakage_detected;

  // Update counters.
  const float x_energy = std::inner_product(x.begin(), x.end(), x.begin(), 0.f);
  const bool active_render_block = x_energy > 10000.f * kFftLengthBy2;
  if (active_render_block) {
    render_received_ = true;
  }
  blocks_with_filter_adaptation_ +=
      (active_render_block && (!SaturatedCapture()) ? 1 : 0);
  --echo_path_change_counter_;

  // Force zero echo suppression gain after an echo path change to allow at
  // least some render data to be collected in order to avoid an initial echo
  // burst.
  constexpr size_t kZeroGainBlocksAfterChange = kNumBlocksPerSecond / 5;
  force_zero_gain_ = (++force_zero_gain_counter_) < kZeroGainBlocksAfterChange;

  // Estimate delays.
  filter_delay_ = EstimateFilterDelay(adaptive_filter_frequency_response);
  external_delay_ =
      external_delay_samples
          ? rtc::Optional<size_t>(*external_delay_samples / kBlockSize)
          : rtc::Optional<size_t>();

  // Update the ERL and ERLE measures.
  if (filter_delay_ && echo_path_change_counter_ <= 0) {
    const auto& X2 = render_buffer.Spectrum(*filter_delay_);
    erle_estimator_.Update(X2, Y2, E2_main);
    erl_estimator_.Update(X2, Y2);
  }

  // Detect and flag echo saturation.
  // TODO(peah): Add the delay in this computation to ensure that the render and
  // capture signals are properly aligned.
  RTC_DCHECK_LT(0, x.size());
  const float max_sample = fabs(*std::max_element(
      x.begin(), x.end(), [](float a, float b) { return a * a < b * b; }));
  const bool saturated_echo =
      previous_max_sample_ * 100 > 1600 && SaturatedCapture();
  previous_max_sample_ = max_sample;

  // Counts the blocks since saturation.
  constexpr size_t kSaturationLeakageBlocks = 20;
  blocks_since_last_saturation_ =
      saturated_echo ? 0 : blocks_since_last_saturation_ + 1;
  echo_saturation_ = blocks_since_last_saturation_ < kSaturationLeakageBlocks;

  // Flag whether the linear filter estimate is usable.
  constexpr size_t kEchoPathChangeConvergenceBlocks = 2 * kNumBlocksPerSecond;
  usable_linear_estimate_ =
      (!echo_saturation_) &&
      (!render_received_ ||
       blocks_with_filter_adaptation_ > kEchoPathChangeConvergenceBlocks) &&
      filter_delay_ && echo_path_change_counter_ <= 0 && external_delay_;

  // After an amount of active render samples for which an echo should have been
  // detected in the capture signal if the ERL was not infinite, flag that a
  // headset is used.
  constexpr size_t kHeadSetDetectionBlocks = 5 * kNumBlocksPerSecond;
  headset_detected_ = !external_delay_ && !filter_delay_ &&
                      (!render_received_ || blocks_with_filter_adaptation_ >=
                                                kHeadSetDetectionBlocks);

  // Update the room reverb estimate.
  UpdateReverb(adaptive_filter_impulse_response);
}

void AecState::UpdateReverb(
    const std::array<float, kAdaptiveFilterTimeDomainLength>&
        impulse_response) {
  if ((!(filter_delay_ && usable_linear_estimate_)) ||
      (*filter_delay_ > kAdaptiveFilterLength - 4)) {
    return;
  }

  // Form the data to match against by squaring the impulse response
  // coefficients.
  std::array<float, kAdaptiveFilterTimeDomainLength> matching_data;
  std::transform(impulse_response.begin(), impulse_response.end(),
                 matching_data.begin(), [](float a) { return a * a; });

  // Avoid matching against noise in the model by subtracting an estimate of the
  // model noise power.
  constexpr size_t kTailLength = 64;
  constexpr size_t tail_index = kAdaptiveFilterTimeDomainLength - kTailLength;
  const float tail_power = *std::max_element(matching_data.begin() + tail_index,
                                             matching_data.end());
  std::for_each(matching_data.begin(), matching_data.begin() + tail_index,
                [tail_power](float& a) { a = std::max(0.f, a - tail_power); });

  // Identify the peak index of the impulse response.
  const size_t peak_index = *std::max_element(
      matching_data.begin(), matching_data.begin() + tail_index);

  if (peak_index + 128 < tail_index) {
    size_t start_index = peak_index + 64;
    // Compute the matching residual error for the current candidate to match.
    float residual_sqr_sum = 0.f;
    float d_k = reverb_decay_to_test_;
    for (size_t k = start_index; k < tail_index; ++k) {
      if (matching_data[start_index + 1] == 0.f) {
        break;
      }

      float residual = matching_data[k] - matching_data[peak_index] * d_k;
      residual_sqr_sum += residual * residual;
      d_k *= reverb_decay_to_test_;
    }

    // If needed, update the best candidate for the reverb decay.
    if (reverb_decay_candidate_residual_ < 0.f ||
        residual_sqr_sum < reverb_decay_candidate_residual_) {
      reverb_decay_candidate_residual_ = residual_sqr_sum;
      reverb_decay_candidate_ = reverb_decay_to_test_;
    }
  }

  // Compute the next reverb candidate to evaluate such that all candidates will
  // be evaluated within one second.
  reverb_decay_to_test_ += (0.9965f - 0.9f) / (5 * kNumBlocksPerSecond);

  // If all reverb candidates have been evaluated, choose the best one as the
  // reverb decay.
  if (reverb_decay_to_test_ >= 0.9965f) {
    if (reverb_decay_candidate_residual_ < 0.f) {
      // Transform the decay to be in the unit of blocks.
      reverb_decay_ = powf(reverb_decay_candidate_, kFftLengthBy2);

      // Limit the estimated reverb_decay_ to the maximum one needed in practice
      // to minimize the impact of incorrect estimates.
      reverb_decay_ =
          std::min(config_.param.ep_strength.default_len, reverb_decay_);
    }
    reverb_decay_to_test_ = 0.9f;
    reverb_decay_candidate_residual_ = -1.f;
  }

  // For noisy impulse responses, assume a fixed tail length.
  if (tail_power > 0.0005f) {
    reverb_decay_ = config_.param.ep_strength.default_len;
  }
  data_dumper_->DumpRaw("aec3_reverb_decay", reverb_decay_);
  data_dumper_->DumpRaw("aec3_tail_power", tail_power);
}

void AecState::EchoAudibility::Update(rtc::ArrayView<const float> x,
                                      const std::array<float, kBlockSize>& s) {
  auto result_x = std::minmax_element(x.begin(), x.end());
  auto result_s = std::minmax_element(s.begin(), s.end());
  const float x_abs =
      std::max(std::abs(*result_x.first), std::abs(*result_x.second));
  const float s_abs =
      std::max(std::abs(*result_s.first), std::abs(*result_s.second));

  if (x_abs < 5.f) {
    ++low_farend_counter_;
  } else {
    low_farend_counter_ = 0;
  }

  // The echo is deemed as not audible if the echo estimate is on the level of
  // the quantization noise in the FFTs and the nearend level is sufficiently
  // strong to mask that by ensuring that the playout and AGC gains do not boost
  // any residual echo that is below the quantization noise level. Furthermore,
  // cases where the render signal is very close to zero are also identified as
  // not producing audible echo.
  inaudible_echo_ = max_nearend_ > 500 && s_abs < 30.f;
  inaudible_echo_ = inaudible_echo_ || low_farend_counter_ > 20;
}

void AecState::EchoAudibility::UpdateWithOutput(rtc::ArrayView<const float> e) {
  const float e_max = *std::max_element(e.begin(), e.end());
  const float e_min = *std::min_element(e.begin(), e.end());
  const float e_abs = std::max(std::abs(e_max), std::abs(e_min));

  if (max_nearend_ < e_abs) {
    max_nearend_ = e_abs;
    max_nearend_counter_ = 0;
  } else {
    if (++max_nearend_counter_ > 5 * kNumBlocksPerSecond) {
      max_nearend_ *= 0.995f;
    }
  }
}

}  // namespace webrtc
