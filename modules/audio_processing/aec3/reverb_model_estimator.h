/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_REVERB_MODEL_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_REVERB_MODEL_ESTIMATOR_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

// The ReverbModelEstimator class describes an estimator of the parameters
// that are used for the reverberant model.
class ReverbModelEstimator {
 public:
  explicit ReverbModelEstimator(const EchoCanceller3Config& config);
  ~ReverbModelEstimator();
  // Updates the model.
  void Update(const std::vector<float>& impulse_response,
              const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                  filter_freq_response,
              const absl::optional<float>& quality_linear,
              int filter_delay_blocks,
              bool usable_linear_estimate,
              float default_decay,
              bool stationary_block);
  // Returns the decay for the exponential model.
  float ReverbDecay() const { return reverb_decay_; }

  void Dump(const std::unique_ptr<ApmDataDumper>& data_dumper);

  // Return the estimated freq. response of the tail of the filter.
  rtc::ArrayView<const float> GetFreqRespTail() const {
    return freq_resp_tail_;
  }

 private:
  bool IsAGoodFilterForDecayEstimation(int filter_delay_blocks,
                                       bool usable_linear_estimate,
                                       size_t length_filter);
  void UpdateReverbDecay(const std::vector<float>& impulse_response);

  void UpdateFreqRespTail(
      const std::vector<std::array<float, kFftLengthBy2Plus1>>&
          filter_freq_response,
      int filter_delay_blocks,
      float alpha);

  void ResetDecayEstimation();

  const size_t filter_main_length_blocks_;

  float accumulated_nz_ = 0.f;
  float accumulated_nn_ = 0.f;
  float accumulated_count_ = 0.f;
  size_t current_reverb_decay_section_ = 0;
  size_t num_reverb_decay_sections_ = 0;
  size_t num_reverb_decay_sections_next_ = 0;
  bool found_end_of_reverb_decay_ = false;
  std::array<float, kMaxAdaptiveFilterLength> block_energies_;
  float reverb_decay_;
  float tail_energy_ = 0.f;
  float alpha_ = 0.f;
  std::array<float, kFftLengthBy2Plus1> freq_resp_tail_;
  float ratio_tail_to_direct_path_ = 0.f;
  bool enable_smooth_freq_resp_tail_updates_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_REVERB_MODEL_ESTIMATOR_H_
