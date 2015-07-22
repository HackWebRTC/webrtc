/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  Implements core class for intelligibility enhancer.
//
//  Details of the model and algorithm can be found in the original paper:
//  http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=6882788
//

#include "webrtc/modules/audio_processing/intelligibility/intelligibility_enhancer.h"

#include <math.h>
#include <stdlib.h>

#include <algorithm>
#include <numeric>

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/vad/include/webrtc_vad.h"
#include "webrtc/common_audio/window_generator.h"

namespace webrtc {

namespace {

const int kWindowSizeMs = 2;
const int kChunkSizeMs = 10;  // Size provided by APM.
const float kClipFreq = 200.0f;
const float kConfigRho = 0.02f;  // Default production and interpretation SNR.
const float kKbdAlpha = 1.5f;
const float kLambdaBot = -1.0f;      // Extreme values in bisection
const float kLambdaTop = -10e-18f;  // search for lamda.

}  // namespace

using std::complex;
using std::max;
using std::min;
using VarianceType = intelligibility::VarianceArray::StepType;

IntelligibilityEnhancer::TransformCallback::TransformCallback(
    IntelligibilityEnhancer* parent,
    IntelligibilityEnhancer::AudioSource source)
    : parent_(parent), source_(source) {
}

void IntelligibilityEnhancer::TransformCallback::ProcessAudioBlock(
    const complex<float>* const* in_block,
    int in_channels,
    int frames,
    int /* out_channels */,
    complex<float>* const* out_block) {
  DCHECK_EQ(parent_->freqs_, frames);
  for (int i = 0; i < in_channels; ++i) {
    parent_->DispatchAudio(source_, in_block[i], out_block[i]);
  }
}

IntelligibilityEnhancer::IntelligibilityEnhancer(int erb_resolution,
                                                 int sample_rate_hz,
                                                 int channels,
                                                 int cv_type,
                                                 float cv_alpha,
                                                 int cv_win,
                                                 int analysis_rate,
                                                 int variance_rate,
                                                 float gain_limit)
    : freqs_(RealFourier::ComplexLength(
          RealFourier::FftOrder(sample_rate_hz * kWindowSizeMs / 1000))),
      window_size_(1 << RealFourier::FftOrder(freqs_)),
      chunk_length_(sample_rate_hz * kChunkSizeMs / 1000),
      bank_size_(GetBankSize(sample_rate_hz, erb_resolution)),
      sample_rate_hz_(sample_rate_hz),
      erb_resolution_(erb_resolution),
      channels_(channels),
      analysis_rate_(analysis_rate),
      variance_rate_(variance_rate),
      clear_variance_(freqs_,
                      static_cast<VarianceType>(cv_type),
                      cv_win,
                      cv_alpha),
      noise_variance_(freqs_, VarianceType::kStepInfinite, 475, 0.01f),
      filtered_clear_var_(new float[bank_size_]),
      filtered_noise_var_(new float[bank_size_]),
      filter_bank_(bank_size_),
      center_freqs_(new float[bank_size_]),
      rho_(new float[bank_size_]),
      gains_eq_(new float[bank_size_]),
      gain_applier_(freqs_, gain_limit),
      temp_out_buffer_(nullptr),
      input_audio_(new float* [channels]),
      kbd_window_(new float[window_size_]),
      render_callback_(this, AudioSource::kRenderStream),
      capture_callback_(this, AudioSource::kCaptureStream),
      block_count_(0),
      analysis_step_(0),
      vad_high_(WebRtcVad_Create()),
      vad_low_(WebRtcVad_Create()),
      vad_tmp_buffer_(new int16_t[chunk_length_]) {
  DCHECK_LE(kConfigRho, 1.0f);

  CreateErbBank();

  WebRtcVad_Init(vad_high_);
  WebRtcVad_set_mode(vad_high_, 0);  // High likelihood of speech.
  WebRtcVad_Init(vad_low_);
  WebRtcVad_set_mode(vad_low_, 3);  // Low likelihood of speech.

  temp_out_buffer_ = static_cast<float**>(
      malloc(sizeof(*temp_out_buffer_) * channels_ +
             sizeof(**temp_out_buffer_) * chunk_length_ * channels_));
  for (int i = 0; i < channels_; ++i) {
    temp_out_buffer_[i] =
        reinterpret_cast<float*>(temp_out_buffer_ + channels_) +
        chunk_length_ * i;
  }

  // Assumes all rho equal.
  for (int i = 0; i < bank_size_; ++i) {
    rho_[i] = kConfigRho * kConfigRho;
  }

  float freqs_khz = kClipFreq / 1000.0f;
  int erb_index = static_cast<int>(ceilf(
      11.17f * logf((freqs_khz + 0.312f) / (freqs_khz + 14.6575f)) + 43.0f));
  start_freq_ = std::max(1, erb_index * erb_resolution);

  WindowGenerator::KaiserBesselDerived(kKbdAlpha, window_size_,
                                       kbd_window_.get());
  render_mangler_.reset(new LappedTransform(
      channels_, channels_, chunk_length_, kbd_window_.get(), window_size_,
      window_size_ / 2, &render_callback_));
  capture_mangler_.reset(new LappedTransform(
      channels_, channels_, chunk_length_, kbd_window_.get(), window_size_,
      window_size_ / 2, &capture_callback_));
}

IntelligibilityEnhancer::~IntelligibilityEnhancer() {
  WebRtcVad_Free(vad_low_);
  WebRtcVad_Free(vad_high_);
  free(temp_out_buffer_);
}

void IntelligibilityEnhancer::ProcessRenderAudio(float* const* audio) {
  for (int i = 0; i < chunk_length_; ++i) {
    vad_tmp_buffer_[i] = (int16_t)audio[0][i];
  }
  has_voice_low_ = WebRtcVad_Process(vad_low_, sample_rate_hz_,
                                     vad_tmp_buffer_.get(), chunk_length_) == 1;

  // Process and enhance chunk of |audio|
  render_mangler_->ProcessChunk(audio, temp_out_buffer_);

  for (int i = 0; i < channels_; ++i) {
    memcpy(audio[i], temp_out_buffer_[i],
           chunk_length_ * sizeof(**temp_out_buffer_));
  }
}

void IntelligibilityEnhancer::ProcessCaptureAudio(float* const* audio) {
  for (int i = 0; i < chunk_length_; ++i) {
    vad_tmp_buffer_[i] = (int16_t)audio[0][i];
  }
  // TODO(bercic): The VAD was always detecting voice in the noise stream,
  // no matter what the aggressiveness, so it was temporarily disabled here.

  #if 0
    if (WebRtcVad_Process(vad_high_, sample_rate_hz_, vad_tmp_buffer_.get(),
      chunk_length_) == 1) {
      printf("capture HAS speech\n");
      return;
    }
    printf("capture NO speech\n");
  #endif

  capture_mangler_->ProcessChunk(audio, temp_out_buffer_);
}

void IntelligibilityEnhancer::DispatchAudio(
    IntelligibilityEnhancer::AudioSource source,
    const complex<float>* in_block,
    complex<float>* out_block) {
  switch (source) {
    case kRenderStream:
      ProcessClearBlock(in_block, out_block);
      break;
    case kCaptureStream:
      ProcessNoiseBlock(in_block, out_block);
      break;
  }
}

void IntelligibilityEnhancer::ProcessClearBlock(const complex<float>* in_block,
                                                complex<float>* out_block) {
  if (block_count_ < 2) {
    memset(out_block, 0, freqs_ * sizeof(*out_block));
    ++block_count_;
    return;
  }

  // For now, always assumes enhancement is necessary.
  // TODO(ekmeyerson): Change to only enhance if necessary,
  // based on experiments with different cutoffs.
  if (has_voice_low_ || true) {
    clear_variance_.Step(in_block, false);
    const float power_target = std::accumulate(
        clear_variance_.variance(), clear_variance_.variance() + freqs_, 0.0f);

    if (block_count_ % analysis_rate_ == analysis_rate_ - 1) {
      AnalyzeClearBlock(power_target);
      ++analysis_step_;
      if (analysis_step_ == variance_rate_) {
        analysis_step_ = 0;
        clear_variance_.Clear();
        noise_variance_.Clear();
      }
    }
    ++block_count_;
  }

  /* efidata(n,:) = sqrt(b(n)) * fidata(n,:) */
  gain_applier_.Apply(in_block, out_block);
}

void IntelligibilityEnhancer::AnalyzeClearBlock(float power_target) {
  FilterVariance(clear_variance_.variance(), filtered_clear_var_.get());
  FilterVariance(noise_variance_.variance(), filtered_noise_var_.get());

  SolveForGainsGivenLambda(kLambdaTop, start_freq_, gains_eq_.get());
  const float power_top =
      DotProduct(gains_eq_.get(), filtered_clear_var_.get(), bank_size_);
  SolveForGainsGivenLambda(kLambdaBot, start_freq_, gains_eq_.get());
  const float power_bot =
      DotProduct(gains_eq_.get(), filtered_clear_var_.get(), bank_size_);
  if (power_target >= power_bot && power_target <= power_top) {
    SolveForLambda(power_target, power_bot, power_top);
    UpdateErbGains();
  }  // Else experiencing variance underflow, so do nothing.
}

void IntelligibilityEnhancer::SolveForLambda(float power_target,
                                             float power_bot,
                                             float power_top) {
  const float kConvergeThresh = 0.001f;  // TODO(ekmeyerson): Find best values
  const int kMaxIters = 100;             // for these, based on experiments.

  const float reciprocal_power_target = 1.f / power_target;
  float lambda_bot = kLambdaBot;
  float lambda_top = kLambdaTop;
  float power_ratio = 2.0f;  // Ratio of achieved power to target power.
  int iters = 0;
  while (std::fabs(power_ratio - 1.0f) > kConvergeThresh &&
         iters <= kMaxIters) {
    const float lambda = lambda_bot + (lambda_top - lambda_bot) / 2.0f;
    SolveForGainsGivenLambda(lambda, start_freq_, gains_eq_.get());
    const float power =
        DotProduct(gains_eq_.get(), filtered_clear_var_.get(), bank_size_);
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
  for (int i = 0; i < freqs_; ++i) {
    gains[i] = 0.0f;
    for (int j = 0; j < bank_size_; ++j) {
      gains[i] = fmaf(filter_bank_[j][i], gains_eq_[j], gains[i]);
    }
  }
}

void IntelligibilityEnhancer::ProcessNoiseBlock(const complex<float>* in_block,
                                                complex<float>* /*out_block*/) {
  noise_variance_.Step(in_block);
}

int IntelligibilityEnhancer::GetBankSize(int sample_rate, int erb_resolution) {
  float freq_limit = sample_rate / 2000.0f;
  int erb_scale = ceilf(
      11.17f * logf((freq_limit + 0.312f) / (freq_limit + 14.6575f)) + 43.0f);
  return erb_scale * erb_resolution;
}

void IntelligibilityEnhancer::CreateErbBank() {
  int lf = 1, rf = 4;

  for (int i = 0; i < bank_size_; ++i) {
    float abs_temp = fabsf((i + 1.0f) / static_cast<float>(erb_resolution_));
    center_freqs_[i] = 676170.4f / (47.06538f - expf(0.08950404f * abs_temp));
    center_freqs_[i] -= 14678.49f;
  }
  float last_center_freq = center_freqs_[bank_size_ - 1];
  for (int i = 0; i < bank_size_; ++i) {
    center_freqs_[i] *= 0.5f * sample_rate_hz_ / last_center_freq;
  }

  for (int i = 0; i < bank_size_; ++i) {
    filter_bank_[i].resize(freqs_);
  }

  for (int i = 1; i <= bank_size_; ++i) {
    int lll, ll, rr, rrr;
    lll = round(center_freqs_[max(1, i - lf) - 1] * freqs_ /
                (0.5f * sample_rate_hz_));
    ll =
        round(center_freqs_[max(1, i) - 1] * freqs_ / (0.5f * sample_rate_hz_));
    lll = min(freqs_, max(lll, 1)) - 1;
    ll = min(freqs_, max(ll, 1)) - 1;

    rrr = round(center_freqs_[min(bank_size_, i + rf) - 1] * freqs_ /
                (0.5f * sample_rate_hz_));
    rr = round(center_freqs_[min(bank_size_, i + 1) - 1] * freqs_ /
               (0.5f * sample_rate_hz_));
    rrr = min(freqs_, max(rrr, 1)) - 1;
    rr = min(freqs_, max(rr, 1)) - 1;

    float step, element;

    step = 1.0f / (ll - lll);
    element = 0.0f;
    for (int j = lll; j <= ll; ++j) {
      filter_bank_[i - 1][j] = element;
      element += step;
    }
    step = 1.0f / (rrr - rr);
    element = 1.0f;
    for (int j = rr; j <= rrr; ++j) {
      filter_bank_[i - 1][j] = element;
      element -= step;
    }
    for (int j = ll; j <= rr; ++j) {
      filter_bank_[i - 1][j] = 1.0f;
    }
  }

  float sum;
  for (int i = 0; i < freqs_; ++i) {
    sum = 0.0f;
    for (int j = 0; j < bank_size_; ++j) {
      sum += filter_bank_[j][i];
    }
    for (int j = 0; j < bank_size_; ++j) {
      filter_bank_[j][i] /= sum;
    }
  }
}

void IntelligibilityEnhancer::SolveForGainsGivenLambda(float lambda,
                                                       int start_freq,
                                                       float* sols) {
  bool quadratic = (kConfigRho < 1.0f);
  const float* var_x0 = filtered_clear_var_.get();
  const float* var_n0 = filtered_noise_var_.get();

  for (int n = 0; n < start_freq; ++n) {
    sols[n] = 1.0f;
  }

  // Analytic solution for optimal gains. See paper for derivation.
  for (int n = start_freq - 1; n < bank_size_; ++n) {
    float alpha0, beta0, gamma0;
    gamma0 = 0.5f * rho_[n] * var_x0[n] * var_n0[n] +
             lambda * var_x0[n] * var_n0[n] * var_n0[n];
    beta0 = lambda * var_x0[n] * (2 - rho_[n]) * var_x0[n] * var_n0[n];
    if (quadratic) {
      alpha0 = lambda * var_x0[n] * (1 - rho_[n]) * var_x0[n] * var_x0[n];
      sols[n] =
          (-beta0 - sqrtf(beta0 * beta0 - 4 * alpha0 * gamma0)) / (2 * alpha0);
    } else {
      sols[n] = -gamma0 / beta0;
    }
    sols[n] = fmax(0, sols[n]);
  }
}

void IntelligibilityEnhancer::FilterVariance(const float* var, float* result) {
  DCHECK_GT(freqs_, 0);
  for (int i = 0; i < bank_size_; ++i) {
    result[i] = DotProduct(&filter_bank_[i][0], var, freqs_);
  }
}

float IntelligibilityEnhancer::DotProduct(const float* a,
                                          const float* b,
                                          int length) {
  float ret = 0.0f;

  for (int i = 0; i < length; ++i) {
    ret = fmaf(a[i], b[i], ret);
  }
  return ret;
}

}  // namespace webrtc
