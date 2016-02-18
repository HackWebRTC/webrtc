/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/intelligibility/intelligibility_enhancer.h"

#include <math.h>
#include <stdlib.h>
#include <algorithm>
#include <limits>
#include <numeric>

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/window_generator.h"

namespace webrtc {

namespace {

const size_t kErbResolution = 2;
const int kWindowSizeMs = 16;
const int kChunkSizeMs = 10;  // Size provided by APM.
const float kClipFreq = 200.0f;
const float kConfigRho = 0.02f;  // Default production and interpretation SNR.
const float kKbdAlpha = 1.5f;
const float kLambdaBot = -1.0f;      // Extreme values in bisection
const float kLambdaTop = -10e-18f;  // search for lamda.

// Returns dot product of vectors |a| and |b| with size |length|.
float DotProduct(const float* a, const float* b, size_t length) {
  float ret = 0.f;
  for (size_t i = 0; i < length; ++i) {
    ret = fmaf(a[i], b[i], ret);
  }
  return ret;
}

// Computes the power across ERB bands from the power spectral density |pow|.
// Stores it in |result|.
void MapToErbBands(const float* pow,
                   const std::vector<std::vector<float>>& filter_bank,
                   float* result) {
  for (size_t i = 0; i < filter_bank.size(); ++i) {
    RTC_DCHECK_GT(filter_bank[i].size(), 0u);
    result[i] = DotProduct(&filter_bank[i][0], pow, filter_bank[i].size());
  }
}

}  // namespace

IntelligibilityEnhancer::TransformCallback::TransformCallback(
    IntelligibilityEnhancer* parent)
    : parent_(parent) {
}

void IntelligibilityEnhancer::TransformCallback::ProcessAudioBlock(
    const std::complex<float>* const* in_block,
    size_t in_channels,
    size_t frames,
    size_t /* out_channels */,
    std::complex<float>* const* out_block) {
  RTC_DCHECK_EQ(parent_->freqs_, frames);
  for (size_t i = 0; i < in_channels; ++i) {
    parent_->ProcessClearBlock(in_block[i], out_block[i]);
  }
}

IntelligibilityEnhancer::IntelligibilityEnhancer()
    : IntelligibilityEnhancer(IntelligibilityEnhancer::Config()) {
}

IntelligibilityEnhancer::IntelligibilityEnhancer(const Config& config)
    : freqs_(RealFourier::ComplexLength(
          RealFourier::FftOrder(config.sample_rate_hz * kWindowSizeMs / 1000))),
      window_size_(static_cast<size_t>(1 << RealFourier::FftOrder(freqs_))),
      chunk_length_(
          static_cast<size_t>(config.sample_rate_hz * kChunkSizeMs / 1000)),
      bank_size_(GetBankSize(config.sample_rate_hz, kErbResolution)),
      sample_rate_hz_(config.sample_rate_hz),
      erb_resolution_(kErbResolution),
      num_capture_channels_(config.num_capture_channels),
      num_render_channels_(config.num_render_channels),
      analysis_rate_(config.analysis_rate),
      active_(true),
      clear_power_(freqs_, config.decay_rate),
      noise_power_(freqs_, 0.f),
      filtered_clear_pow_(new float[bank_size_]),
      filtered_noise_pow_(new float[bank_size_]),
      center_freqs_(new float[bank_size_]),
      render_filter_bank_(CreateErbBank(freqs_)),
      rho_(new float[bank_size_]),
      gains_eq_(new float[bank_size_]),
      gain_applier_(freqs_, config.gain_change_limit),
      temp_render_out_buffer_(chunk_length_, num_render_channels_),
      kbd_window_(new float[window_size_]),
      render_callback_(this),
      block_count_(0),
      analysis_step_(0) {
  RTC_DCHECK_LE(config.rho, 1.0f);

  memset(filtered_clear_pow_.get(),
         0,
         bank_size_ * sizeof(filtered_clear_pow_[0]));
  memset(filtered_noise_pow_.get(),
         0,
         bank_size_ * sizeof(filtered_noise_pow_[0]));

  // Assumes all rho equal.
  for (size_t i = 0; i < bank_size_; ++i) {
    rho_[i] = config.rho * config.rho;
  }

  float freqs_khz = kClipFreq / 1000.0f;
  size_t erb_index = static_cast<size_t>(ceilf(
      11.17f * logf((freqs_khz + 0.312f) / (freqs_khz + 14.6575f)) + 43.0f));
  start_freq_ = std::max(static_cast<size_t>(1), erb_index * erb_resolution_);

  WindowGenerator::KaiserBesselDerived(kKbdAlpha, window_size_,
                                       kbd_window_.get());
  render_mangler_.reset(new LappedTransform(
      num_render_channels_, num_render_channels_, chunk_length_,
      kbd_window_.get(), window_size_, window_size_ / 2, &render_callback_));
}

void IntelligibilityEnhancer::SetCaptureNoiseEstimate(
    std::vector<float> noise) {
  if (capture_filter_bank_.size() != bank_size_ ||
      capture_filter_bank_[0].size() != noise.size()) {
    capture_filter_bank_ = CreateErbBank(noise.size());
  }
  if (noise.size() != noise_power_.size()) {
    noise_power_.resize(noise.size());
  }
  for (size_t i = 0; i < noise.size(); ++i) {
    noise_power_[i] = noise[i] * noise[i];
  }
}

void IntelligibilityEnhancer::ProcessRenderAudio(float* const* audio,
                                                 int sample_rate_hz,
                                                 size_t num_channels) {
  RTC_CHECK_EQ(sample_rate_hz_, sample_rate_hz);
  RTC_CHECK_EQ(num_render_channels_, num_channels);

  if (active_) {
    render_mangler_->ProcessChunk(audio, temp_render_out_buffer_.channels());
  }

  if (active_) {
    for (size_t i = 0; i < num_render_channels_; ++i) {
      memcpy(audio[i], temp_render_out_buffer_.channels()[i],
             chunk_length_ * sizeof(**audio));
    }
  }
}

void IntelligibilityEnhancer::ProcessClearBlock(
    const std::complex<float>* in_block,
    std::complex<float>* out_block) {
  if (block_count_ < 2) {
    memset(out_block, 0, freqs_ * sizeof(*out_block));
    ++block_count_;
    return;
  }

  // TODO(ekm): Use VAD to |Step| and |AnalyzeClearBlock| only if necessary.
  if (true) {
    clear_power_.Step(in_block);
    if (block_count_ % analysis_rate_ == analysis_rate_ - 1) {
      AnalyzeClearBlock();
      ++analysis_step_;
    }
    ++block_count_;
  }

  if (active_) {
    gain_applier_.Apply(in_block, out_block);
  }
}

void IntelligibilityEnhancer::AnalyzeClearBlock() {
  const float* clear_power = clear_power_.Power();
  MapToErbBands(clear_power,
                render_filter_bank_,
                filtered_clear_pow_.get());
  MapToErbBands(&noise_power_[0],
                capture_filter_bank_,
                filtered_noise_pow_.get());
  SolveForGainsGivenLambda(kLambdaTop, start_freq_, gains_eq_.get());
  const float power_target = std::accumulate(
          clear_power, clear_power + freqs_, 0.f);
  const float power_top =
      DotProduct(gains_eq_.get(), filtered_clear_pow_.get(), bank_size_);
  SolveForGainsGivenLambda(kLambdaBot, start_freq_, gains_eq_.get());
  const float power_bot =
      DotProduct(gains_eq_.get(), filtered_clear_pow_.get(), bank_size_);
  if (power_target >= power_bot && power_target <= power_top) {
    SolveForLambda(power_target, power_bot, power_top);
    UpdateErbGains();
  }  // Else experiencing power underflow, so do nothing.
}

void IntelligibilityEnhancer::SolveForLambda(float power_target,
                                             float power_bot,
                                             float power_top) {
  const float kConvergeThresh = 0.001f;  // TODO(ekmeyerson): Find best values
  const int kMaxIters = 100;             // for these, based on experiments.

  const float reciprocal_power_target =
      1.f / (power_target + std::numeric_limits<float>::epsilon());
  float lambda_bot = kLambdaBot;
  float lambda_top = kLambdaTop;
  float power_ratio = 2.0f;  // Ratio of achieved power to target power.
  int iters = 0;
  while (std::fabs(power_ratio - 1.0f) > kConvergeThresh &&
         iters <= kMaxIters) {
    const float lambda = lambda_bot + (lambda_top - lambda_bot) / 2.0f;
    SolveForGainsGivenLambda(lambda, start_freq_, gains_eq_.get());
    const float power =
        DotProduct(gains_eq_.get(), filtered_clear_pow_.get(), bank_size_);
    if (power < power_target) {
      lambda_bot = lambda;
    } else {
      lambda_top = lambda;
    }
    power_ratio = std::fabs(power * reciprocal_power_target);
    ++iters;
  }
}

void IntelligibilityEnhancer::UpdateErbGains() {
  // (ERB gain) = filterbank' * (freq gain)
  float* gains = gain_applier_.target();
  for (size_t i = 0; i < freqs_; ++i) {
    gains[i] = 0.0f;
    for (size_t j = 0; j < bank_size_; ++j) {
      gains[i] = fmaf(render_filter_bank_[j][i], gains_eq_[j], gains[i]);
    }
  }
}

size_t IntelligibilityEnhancer::GetBankSize(int sample_rate,
                                            size_t erb_resolution) {
  float freq_limit = sample_rate / 2000.0f;
  size_t erb_scale = static_cast<size_t>(ceilf(
      11.17f * logf((freq_limit + 0.312f) / (freq_limit + 14.6575f)) + 43.0f));
  return erb_scale * erb_resolution;
}

std::vector<std::vector<float>> IntelligibilityEnhancer::CreateErbBank(
    size_t num_freqs) {
  std::vector<std::vector<float>> filter_bank(bank_size_);
  size_t lf = 1, rf = 4;

  for (size_t i = 0; i < bank_size_; ++i) {
    float abs_temp = fabsf((i + 1.0f) / static_cast<float>(erb_resolution_));
    center_freqs_[i] = 676170.4f / (47.06538f - expf(0.08950404f * abs_temp));
    center_freqs_[i] -= 14678.49f;
  }
  float last_center_freq = center_freqs_[bank_size_ - 1];
  for (size_t i = 0; i < bank_size_; ++i) {
    center_freqs_[i] *= 0.5f * sample_rate_hz_ / last_center_freq;
  }

  for (size_t i = 0; i < bank_size_; ++i) {
    filter_bank[i].resize(num_freqs);
  }

  for (size_t i = 1; i <= bank_size_; ++i) {
    size_t lll, ll, rr, rrr;
    static const size_t kOne = 1;  // Avoids repeated static_cast<>s below.
    lll = static_cast<size_t>(round(
        center_freqs_[std::max(kOne, i - lf) - 1] * num_freqs /
            (0.5f * sample_rate_hz_)));
    ll = static_cast<size_t>(round(
        center_freqs_[std::max(kOne, i) - 1] * num_freqs /
            (0.5f * sample_rate_hz_)));
    lll = std::min(num_freqs, std::max(lll, kOne)) - 1;
    ll = std::min(num_freqs, std::max(ll, kOne)) - 1;

    rrr = static_cast<size_t>(round(
        center_freqs_[std::min(bank_size_, i + rf) - 1] * num_freqs /
            (0.5f * sample_rate_hz_)));
    rr = static_cast<size_t>(round(
        center_freqs_[std::min(bank_size_, i + 1) - 1] * num_freqs /
            (0.5f * sample_rate_hz_)));
    rrr = std::min(num_freqs, std::max(rrr, kOne)) - 1;
    rr = std::min(num_freqs, std::max(rr, kOne)) - 1;

    float step, element;

    step = ll == lll ? 0.f : 1.f / (ll - lll);
    element = 0.0f;
    for (size_t j = lll; j <= ll; ++j) {
      filter_bank[i - 1][j] = element;
      element += step;
    }
    step = rr == rrr ? 0.f : 1.f / (rrr - rr);
    element = 1.0f;
    for (size_t j = rr; j <= rrr; ++j) {
      filter_bank[i - 1][j] = element;
      element -= step;
    }
    for (size_t j = ll; j <= rr; ++j) {
      filter_bank[i - 1][j] = 1.0f;
    }
  }

  float sum;
  for (size_t i = 0; i < num_freqs; ++i) {
    sum = 0.0f;
    for (size_t j = 0; j < bank_size_; ++j) {
      sum += filter_bank[j][i];
    }
    for (size_t j = 0; j < bank_size_; ++j) {
      filter_bank[j][i] /= sum;
    }
  }
  return filter_bank;
}

void IntelligibilityEnhancer::SolveForGainsGivenLambda(float lambda,
                                                       size_t start_freq,
                                                       float* sols) {
  bool quadratic = (kConfigRho < 1.0f);
  const float* pow_x0 = filtered_clear_pow_.get();
  const float* pow_n0 = filtered_noise_pow_.get();

  for (size_t n = 0; n < start_freq; ++n) {
    sols[n] = 1.0f;
  }

  // Analytic solution for optimal gains. See paper for derivation.
  for (size_t n = start_freq - 1; n < bank_size_; ++n) {
    float alpha0, beta0, gamma0;
    gamma0 = 0.5f * rho_[n] * pow_x0[n] * pow_n0[n] +
             lambda * pow_x0[n] * pow_n0[n] * pow_n0[n];
    beta0 = lambda * pow_x0[n] * (2 - rho_[n]) * pow_x0[n] * pow_n0[n];
    if (quadratic) {
      alpha0 = lambda * pow_x0[n] * (1 - rho_[n]) * pow_x0[n] * pow_x0[n];
      sols[n] =
          (-beta0 - sqrtf(beta0 * beta0 - 4 * alpha0 * gamma0)) /
          (2 * alpha0 + std::numeric_limits<float>::epsilon());
    } else {
      sols[n] = -gamma0 / beta0;
    }
    sols[n] = fmax(0, sols[n]);
  }
}

bool IntelligibilityEnhancer::active() const {
  return active_;
}

}  // namespace webrtc
