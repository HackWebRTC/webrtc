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

#include "webrtc/modules/audio_processing/beamformer/covariance_matrix_generator.h"

#include <cmath>

namespace {

float BesselJ0(float x) {
#if WEBRTC_WIN
  return _j0(x);
#else
  return j0(x);
#endif
}

}  // namespace

namespace webrtc {

// Calculates the boxcar-angular desired source distribution at a given
// wavenumber, and stores it in |mat|.
void CovarianceMatrixGenerator::Boxcar(float wave_number,
                                       int num_input_channels,
                                       float mic_spacing,
                                       float half_width,
                                       ComplexMatrix<float>* mat) {
  CHECK_EQ(num_input_channels, mat->num_rows());
  CHECK_EQ(num_input_channels, mat->num_columns());

  complex<float>* const* boxcar_elements = mat->elements();

  for (int i = 0; i < num_input_channels; ++i) {
    for (int j = 0; j < num_input_channels; ++j) {
      if (i == j) {
        boxcar_elements[i][j] = complex<float>(2.f * half_width, 0.f);
      } else {
        float factor = (j - i) * wave_number * mic_spacing;
        float boxcar_real = 2.f * sin(factor * half_width) / factor;
        boxcar_elements[i][j] = complex<float>(boxcar_real, 0.f);
      }
    }
  }
}

void CovarianceMatrixGenerator::GappedUniformCovarianceMatrix(
    float wave_number,
    float num_input_channels,
    float mic_spacing,
    float gap_half_width,
    ComplexMatrix<float>* mat) {
  CHECK_EQ(num_input_channels, mat->num_rows());
  CHECK_EQ(num_input_channels, mat->num_columns());

  complex<float>* const* mat_els = mat->elements();
  for (int i = 0; i < num_input_channels; ++i) {
    for (int j = 0; j < num_input_channels; ++j) {
      float x = (j - i) * wave_number * mic_spacing;
      mat_els[i][j] = BesselJ0(x);
    }
  }

  ComplexMatrix<float> boxcar_mat(num_input_channels, num_input_channels);
  CovarianceMatrixGenerator::Boxcar(wave_number,
                                    num_input_channels,
                                    mic_spacing,
                                    gap_half_width,
                                    &boxcar_mat);
  mat->Subtract(boxcar_mat);
}

void CovarianceMatrixGenerator::AngledCovarianceMatrix(
    float sound_speed,
    float angle,
    int frequency_bin,
    int fft_size,
    int num_freq_bins,
    int sample_rate,
    int num_input_channels,
    float mic_spacing,
    ComplexMatrix<float>* mat) {
  CHECK_EQ(num_input_channels, mat->num_rows());
  CHECK_EQ(num_input_channels, mat->num_columns());

  ComplexMatrix<float> interf_cov_vector(1, num_input_channels);
  ComplexMatrix<float> interf_cov_vector_transposed(num_input_channels, 1);
  PhaseAlignmentMasks(frequency_bin,
                      fft_size,
                      sample_rate,
                      sound_speed,
                      mic_spacing,
                      num_input_channels,
                      sin(angle),
                      &interf_cov_vector);
  interf_cov_vector_transposed.Transpose(interf_cov_vector);
  interf_cov_vector.PointwiseConjugate();
  mat->Multiply(interf_cov_vector_transposed, interf_cov_vector);
}

void CovarianceMatrixGenerator::DCCovarianceMatrix(int num_input_channels,
                                                   float half_width,
                                                   ComplexMatrix<float>* mat) {
  CHECK_EQ(num_input_channels, mat->num_rows());
  CHECK_EQ(num_input_channels, mat->num_columns());

  complex<float>* const* elements = mat->elements();

  float diagonal_value = 1.f - 2.f * half_width;
  for (int i = 0; i < num_input_channels; ++i) {
    for (int j = 0; j < num_input_channels; ++j) {
      if (i == j) {
        elements[i][j] = complex<float>(diagonal_value, 0.f);
      } else {
        elements[i][j] = complex<float>(0.f, 0.f);
      }
    }
  }
}

void CovarianceMatrixGenerator::PhaseAlignmentMasks(int frequency_bin,
                                                    int fft_size,
                                                    int sample_rate,
                                                    float sound_speed,
                                                    float mic_spacing,
                                                    int num_input_channels,
                                                    float sin_angle,
                                                    ComplexMatrix<float>* mat) {
  CHECK_EQ(1, mat->num_rows());
  CHECK_EQ(num_input_channels, mat->num_columns());

  float freq_in_hertz =
      (static_cast<float>(frequency_bin) / fft_size) * sample_rate;

  complex<float>* const* mat_els = mat->elements();
  for (int c_ix = 0; c_ix < num_input_channels; ++c_ix) {
    // TODO(aluebs): Generalize for non-uniform-linear microphone arrays.
    float distance = mic_spacing * c_ix * sin_angle * -1.f;
    float phase_shift = 2 * M_PI * distance * freq_in_hertz / sound_speed;

    // Euler's formula for mat[0][c_ix] = e^(j * phase_shift).
    mat_els[0][c_ix] = complex<float>(cos(phase_shift), sin(phase_shift));
  }
}

}  // namespace webrtc
