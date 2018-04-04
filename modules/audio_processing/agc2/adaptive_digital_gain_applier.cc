/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_digital_gain_applier.h"

#include <algorithm>

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace {

// This function maps input level to desired applied gain. We want to
// boost the signal so that peaks are at -kHeadroomDbfs. We can't
// apply more than kMaxGainDb gain.
float ComputeGainDb(float input_level_dbfs) {
  // If the level is very low, boost it as much as we can.
  if (input_level_dbfs < -(kHeadroomDbfs + kMaxGainDb)) {
    return kMaxGainDb;
  }

  // We expect to end up here most of the time: the level is below
  // -headroom, but we can boost it to -headroom.
  if (input_level_dbfs < -kHeadroomDbfs) {
    return -kHeadroomDbfs - input_level_dbfs;
  }

  // Otherwise, the level is too high and we can't boost. The
  // LevelEstimator is responsible for not reporting bogus gain
  // values.
  RTC_DCHECK_LE(input_level_dbfs, 0.f);
  return 0.f;
}

// We require 'gain + noise_level <= kMaxNoiseLevelDbfs'.
float LimitGainByNoise(float target_gain,
                       float input_noise_level_dbfs,
                       ApmDataDumper* apm_data_dumper) {
  const float noise_headroom_db = kMaxNoiseLevelDbfs - input_noise_level_dbfs;
  apm_data_dumper->DumpRaw("agc2_noise_headroom_db", noise_headroom_db);
  return std::min(target_gain, std::max(noise_headroom_db, 0.f));
}

// Computes how the gain should change during this frame.
// Return the gain difference in db to 'last_gain_db'.
float ComputeGainChangeThisFrameDb(float target_gain_db,
                                   float last_gain_db,
                                   bool gain_increase_allowed) {
  float target_gain_difference_db = target_gain_db - last_gain_db;
  if (!gain_increase_allowed) {
    target_gain_difference_db = std::min(target_gain_difference_db, 0.f);
  }

  return rtc::SafeClamp(target_gain_difference_db, -kMaxGainChangePerFrameDb,
                        kMaxGainChangePerFrameDb);
}

// Returns true when the gain factor is so close to 1 that it would
// not affect int16 samples.
bool GainCloseToOne(float gain_factor) {
  return 1.f - 1.f / kMaxFloatS16Value <= gain_factor &&
         gain_factor <= 1.f + 1.f / kMaxFloatS16Value;
}

void ApplyGainWithRamping(float last_gain_linear,
                          float gain_at_end_of_frame_linear,
                          AudioFrameView<float> float_frame) {
  // Do not modify the signal when input is loud.
  if (last_gain_linear == gain_at_end_of_frame_linear &&
      GainCloseToOne(gain_at_end_of_frame_linear)) {
    return;
  }

  // A typical case: gain is constant and different from 1.
  if (last_gain_linear == gain_at_end_of_frame_linear) {
    for (size_t k = 0; k < float_frame.num_channels(); ++k) {
      rtc::ArrayView<float> channel_view = float_frame.channel(k);
      for (auto& sample : channel_view) {
        sample *= gain_at_end_of_frame_linear;
      }
    }
    return;
  }

  // The gain changes. We have to change slowly to avoid discontinuities.
  const size_t samples = float_frame.samples_per_channel();
  RTC_DCHECK_GT(samples, 0);
  const float increment =
      (gain_at_end_of_frame_linear - last_gain_linear) / samples;
  float gain = last_gain_linear;
  for (size_t i = 0; i < samples; ++i) {
    for (size_t ch = 0; ch < float_frame.num_channels(); ++ch) {
      float_frame.channel(ch)[i] *= gain;
    }
    gain += increment;
  }
}
}  // namespace

AdaptiveDigitalGainApplier::AdaptiveDigitalGainApplier(
    ApmDataDumper* apm_data_dumper)
    : apm_data_dumper_(apm_data_dumper) {}

void AdaptiveDigitalGainApplier::Process(
    float input_level_dbfs,
    float input_noise_level_dbfs,
    rtc::ArrayView<const VadWithLevel::LevelAndProbability> vad_results,
    AudioFrameView<float> float_frame) {
  RTC_DCHECK_GE(input_level_dbfs, -150.f);
  RTC_DCHECK_LE(input_level_dbfs, 0.f);
  RTC_DCHECK_GE(float_frame.num_channels(), 1);
  RTC_DCHECK_GE(float_frame.samples_per_channel(), 1);

  const float target_gain_db =
      LimitGainByNoise(ComputeGainDb(input_level_dbfs), input_noise_level_dbfs,
                       apm_data_dumper_);

  // TODO(webrtc:7494): Remove this construct. Remove the vectors from
  // VadWithData after we move to a VAD that outputs an estimate every
  // kFrameDurationMs ms.
  //
  // Forbid increasing the gain when there is no speech. For some
  // VADs, 'vad_results' has either many or 0 results. If there are 0
  // results, keep the old flag. If there are many results, and at
  // least one is confident speech, we allow attenuation.
  if (!vad_results.empty()) {
    gain_increase_allowed_ = std::all_of(
        vad_results.begin(), vad_results.end(),
        [](const VadWithLevel::LevelAndProbability& vad_result) {
          return vad_result.speech_probability > kVadConfidenceThreshold;
        });
  }

  const float gain_change_this_frame_db = ComputeGainChangeThisFrameDb(
      target_gain_db, last_gain_db_, gain_increase_allowed_);

  apm_data_dumper_->DumpRaw("agc2_want_to_change_by_db",
                            target_gain_db - last_gain_db_);
  apm_data_dumper_->DumpRaw("agc2_will_change_by_db",
                            gain_change_this_frame_db);

  // Optimization: avoid calling math functions if gain does not
  // change.
  const float gain_at_end_of_frame =
      gain_change_this_frame_db == 0.f
          ? last_gain_linear_
          : DbToRatio(last_gain_db_ + gain_change_this_frame_db);

  ApplyGainWithRamping(last_gain_linear_, gain_at_end_of_frame, float_frame);

  // Remember that the gain has changed for the next iteration.
  last_gain_linear_ = gain_at_end_of_frame;
  last_gain_db_ = last_gain_db_ + gain_change_this_frame_db;
  apm_data_dumper_->DumpRaw("agc2_applied_gain_db", last_gain_db_);
}
}  // namespace webrtc
