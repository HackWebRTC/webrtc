/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#define _USE_MATH_DEFINES

#include "webrtc/modules/audio_processing/beamformer/beamformer.h"

#include <algorithm>
#include <cmath>

#include "webrtc/common_audio/window_generator.h"
#include "webrtc/modules/audio_processing/beamformer/covariance_matrix_generator.h"

namespace webrtc {
namespace {

// Alpha for the Kaiser Bessel Derived window.
const float kAlpha = 1.5f;

// The minimum value a postprocessing mask can take.
const float kMaskMinimum = 0.01f;

const int kFftSize = 256;
const float kSpeedOfSoundMeterSeconds = 340;

// For both target and interf angles, 0 is perpendicular to the microphone
// array, facing forwards. The positive direction goes counterclockwise.
// The angle at which we amplify sound.
const float kTargetAngleRadians = 0.f;

// The angle at which we suppress sound. Suppression is symmetric around 0
// radians, so sound is suppressed at both +|kInterfAngleRadians| and
// -|kInterfAngleRadians|. Since the beamformer is robust, this should
// suppress sound coming from angles near +-|kInterfAngleRadians| as well.
const float kInterfAngleRadians = static_cast<float>(M_PI) / 4.f;

// When calculating the interf covariance matrix, this is the weight for
// the weighted average between the uniform covariance matrix and the angled
// covariance matrix.
// Rpsi = Rpsi_angled * kBalance + Rpsi_uniform * (1 - kBalance)
const float kBalance = 0.2f;

const int kNumFreqBins = kFftSize / 2 + 1;

// TODO(claguna): need comment here.
const float kBeamwidthConstant = 0.00001f;

// Width of the boxcar.
const float kBoxcarHalfWidth = 0.001f;

// We put a gap in the covariance matrix where we expect the target to come
// from. Warning: This must be very small, ex. < 0.01, because otherwise it can
// cause the covariance matrix not to be positive semidefinite, and we require
// that our covariance matrices are positive semidefinite.
const float kCovUniformGapHalfWidth = 0.001f;

// How many blocks of past masks (including the current block) we save. Saved
// masks are used for postprocessing such as removing musical noise.
const int kNumberSavedPostfilterMasks = 2;

// Lower bound on gain decay.
const float kHalfLifeSeconds = 0.05f;

// The average mask is computed from masks in this mid-frequency range.
const int kMidFrequnecyLowerBoundHz = 250;
const int kMidFrequencyUpperBoundHz = 400;

const int kHighFrequnecyLowerBoundHz = 4000;
const int kHighFrequencyUpperBoundHz = 7000;

// Does conjugate(|norm_mat|) * |mat| * transpose(|norm_mat|). No extra space is
// used; to accomplish this, we compute both multiplications in the same loop.
float Norm(const ComplexMatrix<float>& mat,
           const ComplexMatrix<float>& norm_mat) {
  CHECK_EQ(norm_mat.num_rows(), 1);
  CHECK_EQ(norm_mat.num_columns(), mat.num_rows());
  CHECK_EQ(norm_mat.num_columns(), mat.num_columns());

  complex<float> first_product = complex<float>(0.f, 0.f);
  complex<float> second_product = complex<float>(0.f, 0.f);

  const complex<float>* const* mat_els = mat.elements();
  const complex<float>* const* norm_mat_els = norm_mat.elements();

  for (int i = 0; i < norm_mat.num_columns(); ++i) {
    for (int j = 0; j < norm_mat.num_columns(); ++j) {
      complex<float> cur_norm_element = conj(norm_mat_els[0][j]);
      complex<float> cur_mat_element = mat_els[j][i];
      first_product += cur_norm_element * cur_mat_element;
    }
    second_product += first_product * norm_mat_els[0][i];
    first_product = 0.f;
  }
  return second_product.real();
}

// Does conjugate(|lhs|) * |rhs| for row vectors |lhs| and |rhs|.
complex<float> ConjugateDotProduct(const ComplexMatrix<float>& lhs,
                                   const ComplexMatrix<float>& rhs) {
  CHECK_EQ(lhs.num_rows(), 1);
  CHECK_EQ(rhs.num_rows(), 1);
  CHECK_EQ(lhs.num_columns(), rhs.num_columns());

  const complex<float>* const* lhs_elements = lhs.elements();
  const complex<float>* const* rhs_elements = rhs.elements();

  complex<float> result = complex<float>(0.f, 0.f);
  for (int i = 0; i < lhs.num_columns(); ++i) {
    result += conj(lhs_elements[0][i]) * rhs_elements[0][i];
  }

  return result;
}

// Works for positive numbers only.
int Round(float x) {
  return std::floor(x + 0.5f);
}

}  // namespace

Beamformer::Beamformer(int chunk_size_ms,
                       int sample_rate_hz,
                       const std::vector<Point>& array_geometry)
    : chunk_length_(sample_rate_hz / (1000.f / chunk_size_ms)),
      window_(new float[kFftSize]),
      num_input_channels_(array_geometry.size()),
      sample_rate_hz_(sample_rate_hz),
      mic_spacing_(MicSpacingFromGeometry(array_geometry)),
      decay_threshold_(
          pow(2, (kFftSize / -2.f) / (sample_rate_hz_ * kHalfLifeSeconds))),
      mid_frequency_lower_bin_bound_(
          Round(kMidFrequnecyLowerBoundHz * kFftSize / sample_rate_hz_)),
      mid_frequency_upper_bin_bound_(
          Round(kMidFrequencyUpperBoundHz * kFftSize / sample_rate_hz_)),
      high_frequency_lower_bin_bound_(
          Round(kHighFrequnecyLowerBoundHz * kFftSize / sample_rate_hz_)),
      high_frequency_upper_bin_bound_(
          Round(kHighFrequencyUpperBoundHz * kFftSize / sample_rate_hz_)),
      current_block_ix_(0),
      previous_block_ix_(-1),
      postfilter_masks_(new MatrixF[kNumberSavedPostfilterMasks]),
      delay_sum_masks_(new ComplexMatrixF[kNumFreqBins]),
      target_cov_mats_(new ComplexMatrixF[kNumFreqBins]),
      interf_cov_mats_(new ComplexMatrixF[kNumFreqBins]),
      reflected_interf_cov_mats_(new ComplexMatrixF[kNumFreqBins]),
      mask_thresholds_(new float[kNumFreqBins]),
      wave_numbers_(new float[kNumFreqBins]),
      rxiws_(new float[kNumFreqBins]),
      rpsiws_(new float[kNumFreqBins]),
      reflected_rpsiws_(new float[kNumFreqBins]) {
  DCHECK_LE(mid_frequency_upper_bin_bound_, kNumFreqBins);
  DCHECK_LT(mid_frequency_lower_bin_bound_, mid_frequency_upper_bin_bound_);
  DCHECK_LE(high_frequency_upper_bin_bound_, kNumFreqBins);
  DCHECK_LT(high_frequency_lower_bin_bound_, high_frequency_upper_bin_bound_);

  WindowGenerator::KaiserBesselDerived(kAlpha, kFftSize, window_.get());
  lapped_transform_.reset(new LappedTransform(num_input_channels_,
                                              1,
                                              chunk_length_,
                                              window_.get(),
                                              kFftSize,
                                              kFftSize / 2,
                                              this));

  for (int i = 0; i < kNumFreqBins; ++i) {
    float freq_hz = (static_cast<float>(i) / kFftSize) * sample_rate_hz_;
    wave_numbers_[i] = 2 * M_PI * freq_hz / kSpeedOfSoundMeterSeconds;
  }

  for (int i = 0; i < kNumFreqBins; ++i) {
    mask_thresholds_[i] = num_input_channels_ * num_input_channels_ *
                          kBeamwidthConstant * wave_numbers_[i] *
                          wave_numbers_[i];
  }

  // Init all nonadaptive values before looping through the frames.
  InitDelaySumMasks();
  InitTargetCovMats();
  InitInterfCovMats();

  for (int i = 0; i < kNumFreqBins; ++i) {
    rxiws_[i] = Norm(target_cov_mats_[i], delay_sum_masks_[i]);
  }
  for (int i = 0; i < kNumFreqBins; ++i) {
    rpsiws_[i] = Norm(interf_cov_mats_[i], delay_sum_masks_[i]);
  }
  for (int i = 0; i < kNumFreqBins; ++i) {
    reflected_rpsiws_[i] =
        Norm(reflected_interf_cov_mats_[i], delay_sum_masks_[i]);
  }
  for (int i = 0; i < kNumberSavedPostfilterMasks; ++i) {
    postfilter_masks_[i].Resize(1, kNumFreqBins);
  }
}

void Beamformer::InitDelaySumMasks() {
  float sin_target = sin(kTargetAngleRadians);
  for (int f_ix = 0; f_ix < kNumFreqBins; ++f_ix) {
    delay_sum_masks_[f_ix].Resize(1, num_input_channels_);
    CovarianceMatrixGenerator::PhaseAlignmentMasks(f_ix,
                                                   kFftSize,
                                                   sample_rate_hz_,
                                                   kSpeedOfSoundMeterSeconds,
                                                   mic_spacing_,
                                                   num_input_channels_,
                                                   sin_target,
                                                   &delay_sum_masks_[f_ix]);

    complex_f norm_factor = sqrt(
        ConjugateDotProduct(delay_sum_masks_[f_ix], delay_sum_masks_[f_ix]));
    delay_sum_masks_[f_ix].Scale(1.f / norm_factor);
  }
}

void Beamformer::InitTargetCovMats() {
  target_cov_mats_[0].Resize(num_input_channels_, num_input_channels_);
  CovarianceMatrixGenerator::DCCovarianceMatrix(
      num_input_channels_, kBoxcarHalfWidth, &target_cov_mats_[0]);

  complex_f normalization_factor = target_cov_mats_[0].Trace();
  target_cov_mats_[0].Scale(1.f / normalization_factor);

  for (int i = 1; i < kNumFreqBins; ++i) {
    float wave_number = wave_numbers_[i];

    target_cov_mats_[i].Resize(num_input_channels_, num_input_channels_);
    CovarianceMatrixGenerator::Boxcar(wave_number,
                                      num_input_channels_,
                                      mic_spacing_,
                                      kBoxcarHalfWidth,
                                      &target_cov_mats_[i]);

    complex_f normalization_factor = target_cov_mats_[i].Trace();
    target_cov_mats_[i].Scale(1.f / normalization_factor);
  }
}

void Beamformer::InitInterfCovMats() {
  interf_cov_mats_[0].Resize(num_input_channels_, num_input_channels_);
  CovarianceMatrixGenerator::DCCovarianceMatrix(
      num_input_channels_, kCovUniformGapHalfWidth, &interf_cov_mats_[0]);

  complex_f normalization_factor = interf_cov_mats_[0].Trace();
  interf_cov_mats_[0].Scale(1.f / normalization_factor);

  for (int i = 1; i < kNumFreqBins; ++i) {
    float wave_number = wave_numbers_[i];

    interf_cov_mats_[i].Resize(num_input_channels_, num_input_channels_);
    ComplexMatrixF uniform_cov_mat(num_input_channels_, num_input_channels_);
    ComplexMatrixF angled_cov_mat(num_input_channels_, num_input_channels_);

    CovarianceMatrixGenerator::GappedUniformCovarianceMatrix(
        wave_number,
        num_input_channels_,
        mic_spacing_,
        kCovUniformGapHalfWidth,
        &uniform_cov_mat);

    CovarianceMatrixGenerator::AngledCovarianceMatrix(kSpeedOfSoundMeterSeconds,
                                                      kInterfAngleRadians,
                                                      i,
                                                      kFftSize,
                                                      kNumFreqBins,
                                                      sample_rate_hz_,
                                                      num_input_channels_,
                                                      mic_spacing_,
                                                      &angled_cov_mat);
    // Normalize matrices before averaging them.
    complex_f normalization_factor = uniform_cov_mat.Trace();
    uniform_cov_mat.Scale(1.f / normalization_factor);
    normalization_factor = angled_cov_mat.Trace();
    angled_cov_mat.Scale(1.f / normalization_factor);

    // Average matrices.
    uniform_cov_mat.Scale(1 - kBalance);
    angled_cov_mat.Scale(kBalance);
    interf_cov_mats_[i].Add(uniform_cov_mat, angled_cov_mat);
  }

  for (int i = 0; i < kNumFreqBins; ++i) {
    reflected_interf_cov_mats_[i].PointwiseConjugate(interf_cov_mats_[i]);
  }
}

void Beamformer::ProcessChunk(const float* const* input,
                              const float* const* high_pass_split_input,
                              int num_input_channels,
                              int num_frames_per_band,
                              float* const* output,
                              float* const* high_pass_split_output) {
  CHECK_EQ(num_input_channels, num_input_channels_);
  CHECK_EQ(num_frames_per_band, chunk_length_);

  num_blocks_in_this_chunk_ = 0;
  float old_high_pass_mask = high_pass_postfilter_mask_;
  high_pass_postfilter_mask_ = 0.f;
  high_pass_exists_ = high_pass_split_input != NULL;
  lapped_transform_->ProcessChunk(input, output);

  // Apply delay and sum and postfilter in the time domain. WARNING: only works
  // because delay-and-sum is not frequency dependent.
  if (high_pass_exists_) {
    high_pass_postfilter_mask_ /= num_blocks_in_this_chunk_;

    if (previous_block_ix_ == -1) {
      old_high_pass_mask = high_pass_postfilter_mask_;
    }

    // Ramp up/down for smoothing. 1 mask per 10ms results in audible
    // discontinuities.
    float ramp_inc =
        (high_pass_postfilter_mask_ - old_high_pass_mask) / num_frames_per_band;
    for (int i = 0; i < num_frames_per_band; ++i) {
      old_high_pass_mask += ramp_inc;

      // Applying the delay and sum (at zero degrees, this is equivalent to
      // averaging).
      float sum = 0.f;
      for (int j = 0; j < num_input_channels; ++j) {
        sum += high_pass_split_input[j][i];
      }
      high_pass_split_output[0][i] =
          sum / num_input_channels * old_high_pass_mask;
    }
  }
}

void Beamformer::ProcessAudioBlock(const complex_f* const* input,
                                   int num_input_channels,
                                   int num_freq_bins,
                                   int num_output_channels,
                                   complex_f* const* output) {
  CHECK_EQ(num_freq_bins, kNumFreqBins);
  CHECK_EQ(num_input_channels, num_input_channels_);
  CHECK_EQ(num_output_channels, 1);

  float* mask_data = postfilter_masks_[current_block_ix_].elements()[0];

  // Calculating the postfilter masks. Note that we need two for each
  // frequency bin to account for the positive and negative interferer
  // angle.
  for (int i = 0; i < kNumFreqBins; ++i) {
    eig_m_.CopyFromColumn(input, i, num_input_channels_);
    float eig_m_norm_factor =
        std::sqrt(ConjugateDotProduct(eig_m_, eig_m_)).real();
    if (eig_m_norm_factor != 0.f) {
      eig_m_.Scale(1.f / eig_m_norm_factor);
    }

    float rxim = Norm(target_cov_mats_[i], eig_m_);
    float ratio_rxiw_rxim = 0.f;
    if (rxim != 0.f) {
      ratio_rxiw_rxim = rxiws_[i] / rxim;
    }

    complex_f rmw = abs(ConjugateDotProduct(delay_sum_masks_[i], eig_m_));
    rmw *= rmw;
    float rmw_r = rmw.real();

    mask_data[i] = CalculatePostfilterMask(interf_cov_mats_[i],
                                           rpsiws_[i],
                                           ratio_rxiw_rxim,
                                           rmw_r,
                                           mask_thresholds_[i]);

    mask_data[i] *= CalculatePostfilterMask(reflected_interf_cov_mats_[i],
                                            reflected_rpsiws_[i],
                                            ratio_rxiw_rxim,
                                            rmw_r,
                                            mask_thresholds_[i]);
  }

  // Can't access block_index - 1 on the first block.
  if (previous_block_ix_ >= 0) {
    ApplyDecay();
  }

  ApplyLowFrequencyCorrection();

  if (high_pass_exists_) {
    CalculateHighFrequencyMask();
  }

  ApplyMasks(input, output);

  previous_block_ix_ = current_block_ix_;
  current_block_ix_ = (current_block_ix_ + 1) % kNumberSavedPostfilterMasks;
  num_blocks_in_this_chunk_++;
}

float Beamformer::CalculatePostfilterMask(const ComplexMatrixF& interf_cov_mat,
                                          float rpsiw,
                                          float ratio_rxiw_rxim,
                                          float rmw_r,
                                          float mask_threshold) {
  float rpsim = Norm(interf_cov_mat, eig_m_);

  // Find lambda.
  float ratio = rpsiw / rpsim;
  float numerator = rmw_r - ratio;
  float denominator = ratio_rxiw_rxim - ratio;

  float mask = 1.f;
  if (denominator > mask_threshold) {
    float lambda = numerator / denominator;
    mask = std::max(lambda * ratio_rxiw_rxim / rmw_r, kMaskMinimum);
  }
  return mask;
}

void Beamformer::ApplyMasks(const complex_f* const* input,
                            complex_f* const* output) {
  complex_f* output_channel = output[0];
  const float* postfilter_mask_els =
      postfilter_masks_[current_block_ix_].elements()[0];
  for (int f_ix = 0; f_ix < kNumFreqBins; ++f_ix) {
    output_channel[f_ix] = complex_f(0.f, 0.f);

    const complex_f* delay_sum_mask_els = delay_sum_masks_[f_ix].elements()[0];
    for (int c_ix = 0; c_ix < num_input_channels_; ++c_ix) {
      output_channel[f_ix] += input[c_ix][f_ix] * delay_sum_mask_els[c_ix];
    }

    output_channel[f_ix] *= postfilter_mask_els[f_ix];
  }
}

void Beamformer::ApplyDecay() {
  float* current_mask_els = postfilter_masks_[current_block_ix_].elements()[0];
  const float* previous_block_els =
      postfilter_masks_[previous_block_ix_].elements()[0];
  for (int i = 0; i < kNumFreqBins; ++i) {
    current_mask_els[i] =
        std::max(current_mask_els[i], previous_block_els[i] * decay_threshold_);
  }
}

void Beamformer::ApplyLowFrequencyCorrection() {
  float low_frequency_mask = 0.f;
  float* mask_els = postfilter_masks_[current_block_ix_].elements()[0];
  for (int i = mid_frequency_lower_bin_bound_;
       i <= mid_frequency_upper_bin_bound_;
       ++i) {
    low_frequency_mask += mask_els[i];
  }

  low_frequency_mask /=
      mid_frequency_upper_bin_bound_ - mid_frequency_lower_bin_bound_ + 1;

  for (int i = 0; i < mid_frequency_lower_bin_bound_; ++i) {
    mask_els[i] = low_frequency_mask;
  }
}

void Beamformer::CalculateHighFrequencyMask() {
  float high_pass_mask = 0.f;
  float* mask_els = postfilter_masks_[current_block_ix_].elements()[0];
  for (int i = high_frequency_lower_bin_bound_;
       i <= high_frequency_upper_bin_bound_;
       ++i) {
    high_pass_mask += mask_els[i];
  }

  high_pass_mask /=
      high_frequency_upper_bin_bound_ - high_frequency_lower_bin_bound_ + 1;

  high_pass_postfilter_mask_ += high_pass_mask;
}

// This method CHECKs for a uniform linear array.
float Beamformer::MicSpacingFromGeometry(const std::vector<Point>& geometry) {
  CHECK_GE(geometry.size(), 2u);
  float mic_spacing = 0.f;
  for (size_t i = 0u; i < 3u; ++i) {
    float difference = geometry[1].c[i] - geometry[0].c[i];
    for (size_t j = 2u; j < geometry.size(); ++j) {
      CHECK_LT(geometry[j].c[i] - geometry[j - 1].c[i] - difference, 1e-6);
    }
    mic_spacing += difference * difference;
  }
  return sqrt(mic_spacing);
}

}  // namespace webrtc
