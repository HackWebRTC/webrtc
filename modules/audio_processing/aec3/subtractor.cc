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

namespace webrtc {

namespace {

const float kHanning64[64] = {
    0.f,         0.00248461f, 0.00991376f, 0.0222136f,  0.03926189f,
    0.06088921f, 0.08688061f, 0.11697778f, 0.15088159f, 0.1882551f,
    0.22872687f, 0.27189467f, 0.31732949f, 0.36457977f, 0.41317591f,
    0.46263495f, 0.51246535f, 0.56217185f, 0.61126047f, 0.65924333f,
    0.70564355f, 0.75f,       0.79187184f, 0.83084292f, 0.86652594f,
    0.89856625f, 0.92664544f, 0.95048443f, 0.96984631f, 0.98453864f,
    0.99441541f, 0.99937846f, 0.99937846f, 0.99441541f, 0.98453864f,
    0.96984631f, 0.95048443f, 0.92664544f, 0.89856625f, 0.86652594f,
    0.83084292f, 0.79187184f, 0.75f,       0.70564355f, 0.65924333f,
    0.61126047f, 0.56217185f, 0.51246535f, 0.46263495f, 0.41317591f,
    0.36457977f, 0.31732949f, 0.27189467f, 0.22872687f, 0.1882551f,
    0.15088159f, 0.11697778f, 0.08688061f, 0.06088921f, 0.03926189f,
    0.0222136f,  0.00991376f, 0.00248461f, 0.f};

void PredictionError(const Aec3Fft& fft,
                     const FftData& S,
                     rtc::ArrayView<const float> y,
                     std::array<float, kBlockSize>* e,
                     FftData* E,
                     std::array<float, kBlockSize>* s) {
  std::array<float, kFftLength> tmp;
  fft.Ifft(S, &tmp);
  constexpr float kScale = 1.0f / kFftLengthBy2;
  std::transform(y.begin(), y.end(), tmp.begin() + kFftLengthBy2, e->begin(),
                 [&](float a, float b) { return a - b * kScale; });

  if (s) {
    for (size_t k = 0; k < s->size(); ++k) {
      (*s)[k] = kScale * tmp[k + kFftLengthBy2];
    }
  }

  std::for_each(e->begin(), e->end(),
                [](float& a) { a = rtc::SafeClamp(a, -32768.f, 32767.f); });

  RTC_DCHECK_EQ(64, e->size());
  RTC_DCHECK_LE(64, tmp.size());
  std::transform(e->begin(), e->end(), std::begin(kHanning64), tmp.begin(),
                 [](float a, float b) { return a * b; });

  fft.ZeroPaddedFft(rtc::ArrayView<const float>(tmp.data(), 64), E);
}

}  // namespace

Subtractor::Subtractor(const EchoCanceller3Config& config,
                       ApmDataDumper* data_dumper,
                       Aec3Optimization optimization)
    : fft_(),
      data_dumper_(data_dumper),
      optimization_(optimization),
      main_filter_(config.filter.length_blocks, optimization, data_dumper_),
      shadow_filter_(config.filter.length_blocks, optimization, data_dumper_),
      G_main_(config.filter.leakage_converged,
              config.filter.leakage_diverged,
              config.filter.main_noise_gate,
              config.filter.error_floor),
      G_shadow_(config.filter.shadow_rate, config.filter.shadow_noise_gate) {
  RTC_DCHECK(data_dumper_);
}

Subtractor::~Subtractor() = default;

void Subtractor::HandleEchoPathChange(
    const EchoPathVariability& echo_path_variability) {
  const auto full_reset = [&]() {
    main_filter_.HandleEchoPathChange();
    shadow_filter_.HandleEchoPathChange();
    G_main_.HandleEchoPathChange(echo_path_variability);
    G_shadow_.HandleEchoPathChange();
    converged_filter_ = false;
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
  PredictionError(fft_, S, y, &e_main, &E_main, &output->s_main);

  // Form the output of the shadow filter.
  shadow_filter_.Filter(render_buffer, &S);
  PredictionError(fft_, S, y, &e_shadow, &E_shadow, nullptr);

  if (!converged_filter_) {
    const auto sum_of_squares = [](float a, float b) { return a + b * b; };
    const float e2_main =
        std::accumulate(e_main.begin(), e_main.end(), 0.f, sum_of_squares);
    const float e2_shadow =
        std::accumulate(e_shadow.begin(), e_shadow.end(), 0.f, sum_of_squares);
    const float y2 = std::accumulate(y.begin(), y.end(), 0.f, sum_of_squares);

    if (y2 > kBlockSize * 50.f * 50.f) {
      converged_filter_ = (e2_main > 0.3 * y2 || e2_shadow > 0.1 * y2);
    }
  }

  // Compute spectra for future use.
  E_main.Spectrum(optimization_, output->E2_main);
  E_shadow.Spectrum(optimization_, output->E2_shadow);

  // Update the main filter.
  std::array<float, kFftLengthBy2Plus1> X2;
  render_buffer.SpectralSum(main_filter_.SizePartitions(), &X2);
  G_main_.Compute(X2, render_signal_analyzer, *output, main_filter_,
                  aec_state.SaturatedCapture(), &G);
  main_filter_.Adapt(render_buffer, G);
  data_dumper_->DumpRaw("aec3_subtractor_G_main", G.re);
  data_dumper_->DumpRaw("aec3_subtractor_G_main", G.im);

  // Update the shadow filter.
  if (shadow_filter_.SizePartitions() != main_filter_.SizePartitions()) {
    render_buffer.SpectralSum(shadow_filter_.SizePartitions(), &X2);
  }
  G_shadow_.Compute(X2, render_signal_analyzer, E_shadow,
                    shadow_filter_.SizePartitions(),
                    aec_state.SaturatedCapture(), &G);
  shadow_filter_.Adapt(render_buffer, G);

  data_dumper_->DumpRaw("aec3_subtractor_G_shadow", G.re);
  data_dumper_->DumpRaw("aec3_subtractor_G_shadow", G.im);

  main_filter_.DumpFilter("aec3_subtractor_H_main");
  shadow_filter_.DumpFilter("aec3_subtractor_H_shadow");
}

}  // namespace webrtc
