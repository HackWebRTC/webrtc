/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_BEAMFORMER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_BEAMFORMER_H_

#include "webrtc/common_audio/lapped_transform.h"
#include "webrtc/modules/audio_processing/beamformer/complex_matrix.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"

namespace webrtc {

// Enhances sound sources coming directly in front of a uniform linear array
// and suppresses sound sources coming from all other directions. Operates on
// multichannel signals and produces single-channel output.
//
// The implemented nonlinear postfilter algorithm taken from "A Robust Nonlinear
// Beamforming Postprocessor" by Bastiaan Kleijn.
//
// TODO: Target angle assumed to be 0. Parameterize target angle.
class Beamformer : public LappedTransform::Callback {
 public:
  // At the moment it only accepts uniform linear microphone arrays. Using the
  // first microphone as a reference position [0, 0, 0] is a natural choice.
  Beamformer(int chunk_size_ms,
             // Sample rate corresponds to the lower band.
             int sample_rate_hz,
             const std::vector<Point>& array_geometry);

  // Process one time-domain chunk of audio. The audio can be separated into
  // two signals by frequency, with the higher half passed in as the second
  // parameter. Use NULL for |high_pass_split_input| if you only have one
  // audio signal. The number of frames and channels must correspond to the
  // ctor parameters. The same signal can be passed in as |input| and |output|.
  void ProcessChunk(const float* const* input,
                    const float* const* high_pass_split_input,
                    int num_input_channels,
                    int num_frames_per_band,
                    float* const* output,
                    float* const* high_pass_split_output);

 protected:
  // Process one frequency-domain block of audio. This is where the fun
  // happens. Implements LappedTransform::Callback.
  void ProcessAudioBlock(const complex<float>* const* input,
                         int num_input_channels,
                         int num_freq_bins,
                         int num_output_channels,
                         complex<float>* const* output);

 private:
  typedef Matrix<float> MatrixF;
  typedef ComplexMatrix<float> ComplexMatrixF;
  typedef complex<float> complex_f;

  void InitDelaySumMasks();
  void InitTargetCovMats();  // TODO: Make this depend on target angle.
  void InitInterfCovMats();

  // An implementation of equation 18, which calculates postfilter masks that,
  // when applied, minimize the mean-square error of our estimation of the
  // desired signal. A sub-task is to calculate lambda, which is solved via
  // equation 13.
  float CalculatePostfilterMask(const ComplexMatrixF& interf_cov_mat,
                                float rpsiw,
                                float ratio_rxiw_rxim,
                                float rmxi_r,
                                float mask_threshold);

  // Prevents the postfilter masks from degenerating too quickly (a cause of
  // musical noise).
  void ApplyDecay();

  // The postfilter masks are unreliable at low frequencies. Calculates a better
  // mask by averaging mid-low frequency values.
  void ApplyLowFrequencyCorrection();

  // Postfilter masks are also unreliable at high frequencies. Average mid-high
  // frequency masks to calculate a single mask per block which can be applied
  // in the time-domain. Further, we average these block-masks over a chunk,
  // resulting in one postfilter mask per audio chunk. This allows us to skip
  // both transforming and blocking the high-frequency signal.
  void CalculateHighFrequencyMask();

  // Applies both sets of masks to |input| and store in |output|.
  void ApplyMasks(const complex_f* const* input, complex_f* const* output);

  float MicSpacingFromGeometry(const std::vector<Point>& array_geometry);

  // Deals with the fft transform and blocking.
  const int chunk_length_;
  scoped_ptr<LappedTransform> lapped_transform_;
  scoped_ptr<float[]> window_;

  // Parameters exposed to the user.
  const int num_input_channels_;
  const int sample_rate_hz_;
  const float mic_spacing_;

  // Calculated based on user-input and constants in the .cc file.
  const float decay_threshold_;
  const int mid_frequency_lower_bin_bound_;
  const int mid_frequency_upper_bin_bound_;
  const int high_frequency_lower_bin_bound_;
  const int high_frequency_upper_bin_bound_;

  // Indices into |postfilter_masks_|.
  int current_block_ix_;
  int previous_block_ix_;

  // Old masks are saved in this ring buffer for smoothing. Array of length
  // |kNumberSavedMasks| matrix of size 1 x |kNumFreqBins|.
  scoped_ptr<MatrixF[]> postfilter_masks_;

  // Array of length |kNumFreqBins|, Matrix of size |1| x |num_channels_|.
  scoped_ptr<ComplexMatrixF[]> delay_sum_masks_;

  // Array of length |kNumFreqBins|, Matrix of size |num_input_channels_| x
  // |num_input_channels_|.
  scoped_ptr<ComplexMatrixF[]> target_cov_mats_;

  // Array of length |kNumFreqBins|, Matrix of size |num_input_channels_| x
  // |num_input_channels_|.
  scoped_ptr<ComplexMatrixF[]> interf_cov_mats_;
  scoped_ptr<ComplexMatrixF[]> reflected_interf_cov_mats_;

  // Of length |kNumFreqBins|.
  scoped_ptr<float[]> mask_thresholds_;
  scoped_ptr<float[]> wave_numbers_;

  // Preallocated for ProcessAudioBlock()
  // Of length |kNumFreqBins|.
  scoped_ptr<float[]> rxiws_;
  scoped_ptr<float[]> rpsiws_;
  scoped_ptr<float[]> reflected_rpsiws_;

  // The microphone normalization factor.
  ComplexMatrixF eig_m_;

  // For processing the high-frequency input signal.
  bool high_pass_exists_;
  int num_blocks_in_this_chunk_;
  float high_pass_postfilter_mask_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_BEAMFORMER_H_
