/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/spectral_features.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr float kSilenceThreshold = 0.04f;

// Computes the new cepstral difference stats and pushes them into the passed
// symmetric matrix buffer.
void UpdateCepstralDifferenceStats(
    rtc::ArrayView<const float, kNumBands> new_cepstral_coeffs,
    const RingBuffer<float, kNumBands, kCepstralCoeffsHistorySize>& ring_buf,
    SymmetricMatrixBuffer<float, kCepstralCoeffsHistorySize>* sym_matrix_buf) {
  RTC_DCHECK(sym_matrix_buf);
  // Compute the new cepstral distance stats.
  std::array<float, kCepstralCoeffsHistorySize - 1> distances;
  for (size_t i = 0; i < kCepstralCoeffsHistorySize - 1; ++i) {
    const size_t delay = i + 1;
    auto old_cepstral_coeffs = ring_buf.GetArrayView(delay);
    distances[i] = 0.f;
    for (size_t k = 0; k < kNumBands; ++k) {
      const float c = new_cepstral_coeffs[k] - old_cepstral_coeffs[k];
      distances[i] += c * c;
    }
  }
  // Push the new spectral distance stats into the symmetric matrix buffer.
  sym_matrix_buf->Push(distances);
}

}  // namespace

SpectralFeaturesExtractor::SpectralFeaturesExtractor()
    : fft_(),
      reference_frame_fft_(kFftSizeBy2Plus1),
      lagged_frame_fft_(kFftSizeBy2Plus1),
      dct_table_(ComputeDctTable()) {}

SpectralFeaturesExtractor::~SpectralFeaturesExtractor() = default;

void SpectralFeaturesExtractor::Reset() {
  cepstral_coeffs_ring_buf_.Reset();
  cepstral_diffs_buf_.Reset();
}

bool SpectralFeaturesExtractor::CheckSilenceComputeFeatures(
    rtc::ArrayView<const float, kFrameSize20ms24kHz> reference_frame,
    rtc::ArrayView<const float, kFrameSize20ms24kHz> lagged_frame,
    rtc::ArrayView<float, kNumBands - kNumLowerBands> higher_bands_cepstrum,
    rtc::ArrayView<float, kNumLowerBands> average,
    rtc::ArrayView<float, kNumLowerBands> first_derivative,
    rtc::ArrayView<float, kNumLowerBands> second_derivative,
    rtc::ArrayView<float, kNumLowerBands> bands_cross_corr,
    float* variability) {
  // Compute the Opus band energies for the reference frame.
  fft_.WindowedFft(reference_frame, reference_frame_fft_);
  spectral_correlator_.ComputeAutoCorrelation(
      {reference_frame_fft_.data(), kFftSizeBy2Plus1},
      reference_frame_bands_energy_);
  // Check if the reference frame has silence.
  const float tot_energy =
      std::accumulate(reference_frame_bands_energy_.begin(),
                      reference_frame_bands_energy_.end(), 0.f);
  if (tot_energy < kSilenceThreshold) {
    return true;
  }
  // Compute the Opus band energies for the lagged frame.
  fft_.WindowedFft(lagged_frame, lagged_frame_fft_);
  spectral_correlator_.ComputeAutoCorrelation(
      {lagged_frame_fft_.data(), kFftSizeBy2Plus1}, lagged_frame_bands_energy_);
  // Log of the band energies for the reference frame.
  std::array<float, kNumBands> log_bands_energy;
  ComputeSmoothedLogMagnitudeSpectrum(reference_frame_bands_energy_,
                                      log_bands_energy);
  // Reference frame cepstrum.
  std::array<float, kNumBands> cepstrum;
  ComputeDct(log_bands_energy, dct_table_, cepstrum);
  // Ad-hoc correction terms for the first two cepstral coefficients.
  cepstrum[0] -= 12.f;
  cepstrum[1] -= 4.f;
  // Update the ring buffer and the cepstral difference stats.
  cepstral_coeffs_ring_buf_.Push(cepstrum);
  UpdateCepstralDifferenceStats(cepstrum, cepstral_coeffs_ring_buf_,
                                &cepstral_diffs_buf_);
  // Write the higher bands cepstral coefficients.
  RTC_DCHECK_EQ(cepstrum.size() - kNumLowerBands, higher_bands_cepstrum.size());
  std::copy(cepstrum.begin() + kNumLowerBands, cepstrum.end(),
            higher_bands_cepstrum.begin());
  // Compute and write remaining features.
  ComputeAvgAndDerivatives(average, first_derivative, second_derivative);
  ComputeNormalizedCepstralCorrelation(bands_cross_corr);
  RTC_DCHECK(variability);
  *variability = ComputeVariability();
  return false;
}

void SpectralFeaturesExtractor::ComputeAvgAndDerivatives(
    rtc::ArrayView<float, kNumLowerBands> average,
    rtc::ArrayView<float, kNumLowerBands> first_derivative,
    rtc::ArrayView<float, kNumLowerBands> second_derivative) const {
  auto curr = cepstral_coeffs_ring_buf_.GetArrayView(0);
  auto prev1 = cepstral_coeffs_ring_buf_.GetArrayView(1);
  auto prev2 = cepstral_coeffs_ring_buf_.GetArrayView(2);
  RTC_DCHECK_EQ(average.size(), first_derivative.size());
  RTC_DCHECK_EQ(first_derivative.size(), second_derivative.size());
  RTC_DCHECK_LE(average.size(), curr.size());
  for (size_t i = 0; i < average.size(); ++i) {
    // Average, kernel: [1, 1, 1].
    average[i] = curr[i] + prev1[i] + prev2[i];
    // First derivative, kernel: [1, 0, - 1].
    first_derivative[i] = curr[i] - prev2[i];
    // Second derivative, Laplacian kernel: [1, -2, 1].
    second_derivative[i] = curr[i] - 2 * prev1[i] + prev2[i];
  }
}

void SpectralFeaturesExtractor::ComputeNormalizedCepstralCorrelation(
    rtc::ArrayView<float, kNumLowerBands> bands_cross_corr) {
  spectral_correlator_.ComputeCrossCorrelation(
      {reference_frame_fft_.data(), kFftSizeBy2Plus1},
      {lagged_frame_fft_.data(), kFftSizeBy2Plus1}, bands_cross_corr_);
  // Normalize.
  for (size_t i = 0; i < bands_cross_corr_.size(); ++i) {
    bands_cross_corr_[i] =
        bands_cross_corr_[i] /
        std::sqrt(0.001f + reference_frame_bands_energy_[i] *
                               lagged_frame_bands_energy_[i]);
  }
  // Cepstrum.
  ComputeDct(bands_cross_corr_, dct_table_, bands_cross_corr);
  // Ad-hoc correction terms for the first two cepstral coefficients.
  bands_cross_corr[0] -= 1.3f;
  bands_cross_corr[1] -= 0.9f;
}

float SpectralFeaturesExtractor::ComputeVariability() const {
  // Compute cepstral variability score.
  float variability = 0.f;
  for (size_t delay1 = 0; delay1 < kCepstralCoeffsHistorySize; ++delay1) {
    float min_dist = std::numeric_limits<float>::max();
    for (size_t delay2 = 0; delay2 < kCepstralCoeffsHistorySize; ++delay2) {
      if (delay1 == delay2)  // The distance would be 0.
        continue;
      min_dist =
          std::min(min_dist, cepstral_diffs_buf_.GetValue(delay1, delay2));
    }
    variability += min_dist;
  }
  // Normalize (based on training set stats).
  // TODO(bugs.webrtc.org/10480): Isolate normalization from feature extraction.
  return variability / kCepstralCoeffsHistorySize - 2.1f;
}

}  // namespace rnn_vad
}  // namespace webrtc
