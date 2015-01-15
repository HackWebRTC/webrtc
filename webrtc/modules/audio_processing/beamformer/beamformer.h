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
  explicit Beamformer(const std::vector<Point>& array_geometry);
  virtual ~Beamformer() {};

  // Sample rate corresponds to the lower band.
  // Needs to be called before the Beamformer can be used.
  virtual void Initialize(int chunk_size_ms, int sample_rate_hz);

  // Process one time-domain chunk of audio. The audio can be separated into
  // two signals by frequency, with the higher half passed in as the second
  // parameter. Use NULL for |high_pass_split_input| if you only have one
  // audio signal. The number of frames and channels must correspond to the
  // ctor parameters. The same signal can be passed in as |input| and |output|.
  virtual void ProcessChunk(const float* const* input,
                            const float* const* high_pass_split_input,
                            int num_input_channels,
                            int num_frames_per_band,
                            float* const* output,
                            float* const* high_pass_split_output);
  // After processing each block |is_target_present_| is set to true if the
  // target signal es present and to false otherwise. This methods can be called
  // to know if the data is target signal or interference and process it
  // accordingly.
  virtual bool is_target_present() { return is_target_present_; }

 protected:
  // Process one frequency-domain block of audio. This is where the fun
  // happens. Implements LappedTransform::Callback.
  void ProcessAudioBlock(const complex<float>* const* input,
                         int num_input_channels,
                         int num_freq_bins,
                         int num_output_channels,
                         complex<float>* const* output) override;

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
  void EstimateTargetPresence(float* mask, int length);

  static const int kFftSize = 256;
  static const int kNumFreqBins = kFftSize / 2 + 1;
  // How many blocks of past masks (including the current block) we save. Saved
  // masks are used for postprocessing such as removing musical noise.
  static const int kNumberSavedPostfilterMasks = 2;

  // Deals with the fft transform and blocking.
  int chunk_length_;
  scoped_ptr<LappedTransform> lapped_transform_;
  float window_[kFftSize];

  // Parameters exposed to the user.
  const int num_input_channels_;
  int sample_rate_hz_;
  const float mic_spacing_;

  // Calculated based on user-input and constants in the .cc file.
  float decay_threshold_;
  int mid_frequency_lower_bin_bound_;
  int mid_frequency_upper_bin_bound_;
  int high_frequency_lower_bin_bound_;
  int high_frequency_upper_bin_bound_;

  // Indices into |postfilter_masks_|.
  int current_block_ix_;
  int previous_block_ix_;

  // Old masks are saved in this ring buffer for smoothing. Array of length
  // |kNumberSavedMasks| matrix of size 1 x |kNumFreqBins|.
  MatrixF postfilter_masks_[kNumberSavedPostfilterMasks];
  float sorted_mask_[kNumFreqBins];

  // Array of length |kNumFreqBins|, Matrix of size |1| x |num_channels_|.
  ComplexMatrixF delay_sum_masks_[kNumFreqBins];

  // Array of length |kNumFreqBins|, Matrix of size |num_input_channels_| x
  // |num_input_channels_|.
  ComplexMatrixF target_cov_mats_[kNumFreqBins];

  // Array of length |kNumFreqBins|, Matrix of size |num_input_channels_| x
  // |num_input_channels_|.
  ComplexMatrixF interf_cov_mats_[kNumFreqBins];
  ComplexMatrixF reflected_interf_cov_mats_[kNumFreqBins];

  // Of length |kNumFreqBins|.
  float mask_thresholds_[kNumFreqBins];
  float wave_numbers_[kNumFreqBins];

  // Preallocated for ProcessAudioBlock()
  // Of length |kNumFreqBins|.
  float rxiws_[kNumFreqBins];
  float rpsiws_[kNumFreqBins];
  float reflected_rpsiws_[kNumFreqBins];

  // The microphone normalization factor.
  ComplexMatrixF eig_m_;

  // For processing the high-frequency input signal.
  bool high_pass_exists_;
  int num_blocks_in_this_chunk_;
  float high_pass_postfilter_mask_;

  // True when the target signal is present.
  bool is_target_present_;
  // Number of blocks after which the data is considered interference if the
  // mask does not pass |kMaskSignalThreshold|.
  int hold_target_blocks_;
  // Number of blocks since the last mask that passed |kMaskSignalThreshold|.
  int interference_blocks_count_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_BEAMFORMER_H_
