/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/subtractor.h"

#include <algorithm>
#include <numeric>

#include "api/array_view.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

bool EnableAdaptationDuringSaturation() {
  return !field_trial::IsEnabled("WebRTC-Aec3RapidAgcGainRecoveryKillSwitch");
}

bool EnableMisadjustmentEstimator() {
  return !field_trial::IsEnabled("WebRTC-Aec3MisadjustmentEstimatorKillSwitch");
}

void PredictionError(const Aec3Fft& fft,
                     const FftData& S,
                     rtc::ArrayView<const float> y,
                     std::array<float, kBlockSize>* e,
                     std::array<float, kBlockSize>* s,
                     bool adaptation_during_saturation,
                     bool* saturation) {
  std::array<float, kFftLength> tmp;
  fft.Ifft(S, &tmp);
  constexpr float kScale = 1.0f / kFftLengthBy2;
  std::transform(y.begin(), y.end(), tmp.begin() + kFftLengthBy2, e->begin(),
                 [&](float a, float b) { return a - b * kScale; });

  *saturation = false;

  if (s) {
    for (size_t k = 0; k < s->size(); ++k) {
      (*s)[k] = kScale * tmp[k + kFftLengthBy2];
    }
    auto result = std::minmax_element(s->begin(), s->end());
    *saturation = *result.first <= -32768 || *result.first >= 32767;
  }
  if (!(*saturation)) {
    auto result = std::minmax_element(e->begin(), e->end());
    *saturation = *result.first <= -32768 || *result.first >= 32767;
  }

  if (!adaptation_during_saturation) {
    std::for_each(e->begin(), e->end(),
                  [](float& a) { a = rtc::SafeClamp(a, -32768.f, 32767.f); });
  } else {
    *saturation = false;
  }
}

}  // namespace

Subtractor::Subtractor(const EchoCanceller3Config& config,
                       ApmDataDumper* data_dumper,
                       Aec3Optimization optimization)
    : fft_(),
      data_dumper_(data_dumper),
      optimization_(optimization),
      config_(config),
      adaptation_during_saturation_(EnableAdaptationDuringSaturation()),
      enable_misadjustment_estimator_(EnableMisadjustmentEstimator()),
      main_filter_(config_.filter.main.length_blocks,
                   config_.filter.main_initial.length_blocks,
                   config.filter.config_change_duration_blocks,
                   optimization,
                   data_dumper_),
      shadow_filter_(config_.filter.shadow.length_blocks,
                     config_.filter.shadow_initial.length_blocks,
                     config.filter.config_change_duration_blocks,
                     optimization,
                     data_dumper_),
      G_main_(config_.filter.main_initial,
              config_.filter.config_change_duration_blocks),
      G_shadow_(config_.filter.shadow_initial,
                config.filter.config_change_duration_blocks) {
  RTC_DCHECK(data_dumper_);
  // Currently, the rest of AEC3 requires the main and shadow filter lengths to
  // be identical.
  RTC_DCHECK_EQ(config_.filter.main.length_blocks,
                config_.filter.shadow.length_blocks);
  RTC_DCHECK_EQ(config_.filter.main_initial.length_blocks,
                config_.filter.shadow_initial.length_blocks);
}

Subtractor::~Subtractor() = default;

void Subtractor::HandleEchoPathChange(
    const EchoPathVariability& echo_path_variability) {
  const auto full_reset = [&]() {
    main_filter_.HandleEchoPathChange();
    shadow_filter_.HandleEchoPathChange();
    G_main_.HandleEchoPathChange(echo_path_variability);
    G_shadow_.HandleEchoPathChange();
    G_main_.SetConfig(config_.filter.main_initial, true);
    G_shadow_.SetConfig(config_.filter.shadow_initial, true);
    main_filter_.SetSizePartitions(config_.filter.main_initial.length_blocks,
                                   true);
    shadow_filter_.SetSizePartitions(
        config_.filter.shadow_initial.length_blocks, true);
  };

  // TODO(peah): Add delay-change specific reset behavior.
  if ((echo_path_variability.delay_change ==
       EchoPathVariability::DelayAdjustment::kBufferFlush) ||
      (echo_path_variability.delay_change ==
       EchoPathVariability::DelayAdjustment::kDelayReset)) {
    full_reset();
  } else if (echo_path_variability.delay_change ==
             EchoPathVariability::DelayAdjustment::kNewDetectedDelay) {
    full_reset();
  } else if (echo_path_variability.delay_change ==
             EchoPathVariability::DelayAdjustment::kBufferReadjustment) {
    full_reset();
  }
}

void Subtractor::ExitInitialState() {
  G_main_.SetConfig(config_.filter.main, false);
  G_shadow_.SetConfig(config_.filter.shadow, false);
  main_filter_.SetSizePartitions(config_.filter.main.length_blocks, false);
  shadow_filter_.SetSizePartitions(config_.filter.shadow.length_blocks, false);
}

void Subtractor::Process(const RenderBuffer& render_buffer,
                         const rtc::ArrayView<const float> capture,
                         const RenderSignalAnalyzer& render_signal_analyzer,
                         const AecState& aec_state,
                         SubtractorOutput* output) {
  RTC_DCHECK_EQ(kBlockSize, capture.size());
  rtc::ArrayView<const float> y = capture;
  FftData& E_main = output->E_main;
  FftData E_shadow;
  std::array<float, kBlockSize>& e_main = output->e_main;
  std::array<float, kBlockSize>& e_shadow = output->e_shadow;

  FftData S;
  FftData& G = S;

  // Form the output of the main filter.
  main_filter_.Filter(render_buffer, &S);
  bool main_saturation = false;
  PredictionError(fft_, S, y, &e_main, &output->s_main,
                  adaptation_during_saturation_, &main_saturation);
  fft_.ZeroPaddedFft(e_main, Aec3Fft::Window::kHanning, &E_main);

  // Form the output of the shadow filter.
  shadow_filter_.Filter(render_buffer, &S);
  bool shadow_saturation = false;
  PredictionError(fft_, S, y, &e_shadow, nullptr, adaptation_during_saturation_,
                  &shadow_saturation);
  fft_.ZeroPaddedFft(e_shadow, Aec3Fft::Window::kHanning, &E_shadow);

  if (enable_misadjustment_estimator_) {
    filter_misadjustment_estimator_.Update(e_main, y);
    if (filter_misadjustment_estimator_.IsAdjustmentNeeded()) {
      float scale = filter_misadjustment_estimator_.GetMisadjustment();
      main_filter_.ScaleFilter(scale);
      output->ScaleOutputMainFilter(scale);
      filter_misadjustment_estimator_.Reset();
    }
  }
  // Compute spectra for future use.
  E_shadow.Spectrum(optimization_, output->E2_shadow);
  E_main.Spectrum(optimization_, output->E2_main);

  // Update the main filter.
  std::array<float, kFftLengthBy2Plus1> X2;
  render_buffer.SpectralSum(main_filter_.SizePartitions(), &X2);
  G_main_.Compute(X2, render_signal_analyzer, *output, main_filter_,
                  aec_state.SaturatedCapture() || main_saturation, &G);
  main_filter_.Adapt(render_buffer, G);
  data_dumper_->DumpRaw("aec3_subtractor_G_main", G.re);
  data_dumper_->DumpRaw("aec3_subtractor_G_main", G.im);

  // Update the shadow filter.
  if (shadow_filter_.SizePartitions() != main_filter_.SizePartitions()) {
    render_buffer.SpectralSum(shadow_filter_.SizePartitions(), &X2);
  }
  G_shadow_.Compute(X2, render_signal_analyzer, E_shadow,
                    shadow_filter_.SizePartitions(),
                    aec_state.SaturatedCapture() || shadow_saturation, &G);
  shadow_filter_.Adapt(render_buffer, G);

  data_dumper_->DumpRaw("aec3_subtractor_G_shadow", G.re);
  data_dumper_->DumpRaw("aec3_subtractor_G_shadow", G.im);
  filter_misadjustment_estimator_.Dump(data_dumper_);
  DumpFilters();

  if (adaptation_during_saturation_) {
    std::for_each(e_main.begin(), e_main.end(),
                  [](float& a) { a = rtc::SafeClamp(a, -32768.f, 32767.f); });
  }
}

void Subtractor::FilterMisadjustmentEstimator::Update(
    rtc::ArrayView<const float> e,
    rtc::ArrayView<const float> y) {
  const auto sum_of_squares = [](float a, float b) { return a + b * b; };
  const float y2 = std::accumulate(y.begin(), y.end(), 0.f, sum_of_squares);
  const float e2 = std::accumulate(e.begin(), e.end(), 0.f, sum_of_squares);

  e2_acum_ += e2;
  y2_acum_ += y2;
  if (++n_blocks_acum_ == n_blocks_) {
    if (y2_acum_ > n_blocks_ * 200.f * 200.f * kBlockSize) {
      float update = (e2_acum_ / y2_acum_);
      if (e2_acum_ > n_blocks_ * 7500.f * 7500.f * kBlockSize) {
        overhang_ = 4;  // Duration equal to blockSizeMs * n_blocks_ * 4
      } else {
        overhang_ = std::max(overhang_ - 1, 0);
      }

      if ((update < inv_misadjustment_) || (overhang_ > 0)) {
        inv_misadjustment_ += 0.1f * (update - inv_misadjustment_);
      }
    }
    e2_acum_ = 0.f;
    y2_acum_ = 0.f;
    n_blocks_acum_ = 0;
  }
}

void Subtractor::FilterMisadjustmentEstimator::Reset() {
  e2_acum_ = 0.f;
  y2_acum_ = 0.f;
  n_blocks_acum_ = 0;
  inv_misadjustment_ = 0.f;
  overhang_ = 0.f;
}

void Subtractor::FilterMisadjustmentEstimator::Dump(
    ApmDataDumper* data_dumper) const {
  data_dumper->DumpRaw("aec3_inv_misadjustment_factor", inv_misadjustment_);
}

}  // namespace webrtc
