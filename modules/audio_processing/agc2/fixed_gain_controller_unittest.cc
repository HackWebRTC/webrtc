/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/fixed_gain_controller.h"

#include "api/array_view.h"
#include "modules/audio_processing/agc2/vector_float_frame.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

constexpr float kInputLevelLinear = 15000.f;

constexpr float kGainToApplyDb = 15.f;

float RunFixedGainControllerWithConstantInput(FixedGainController* fixed_gc,
                                              const float input_level,
                                              const size_t num_frames,
                                              const int sample_rate) {
  // Give time to the level etimator to converge.
  for (size_t i = 0; i < num_frames; ++i) {
    VectorFloatFrame vectors_with_float_frame(
        1, rtc::CheckedDivExact(sample_rate, 100), input_level);
    fixed_gc->Process(vectors_with_float_frame.float_frame_view());
  }

  // Process the last frame with constant input level.
  VectorFloatFrame vectors_with_float_frame_last(
      1, rtc::CheckedDivExact(sample_rate, 100), input_level);
  fixed_gc->Process(vectors_with_float_frame_last.float_frame_view());

  // Return the last sample from the last processed frame.
  const auto channel =
      vectors_with_float_frame_last.float_frame_view().channel(0);
  return channel[channel.size() - 1];
}
ApmDataDumper test_data_dumper(0);

FixedGainController CreateFixedGainController(float gain_to_apply,
                                              size_t rate,
                                              bool enable_limiter) {
  FixedGainController fgc(&test_data_dumper);
  fgc.SetGain(gain_to_apply);
  fgc.SetSampleRate(gain_to_apply);
  fgc.EnableLimiter(enable_limiter);
  return fgc;
}

}  // namespace

TEST(AutomaticGainController2FixedDigital, CreateUseWithoutLimiter) {
  const int kSampleRate = 48000;
  FixedGainController fixed_gc =
      CreateFixedGainController(kGainToApplyDb, kSampleRate, false);
  VectorFloatFrame vectors_with_float_frame(
      1, rtc::CheckedDivExact(kSampleRate, 100), kInputLevelLinear);
  auto float_frame = vectors_with_float_frame.float_frame_view();
  fixed_gc.Process(float_frame);
  const auto channel = float_frame.channel(0);
  EXPECT_LT(kInputLevelLinear, channel[0]);
}

TEST(AutomaticGainController2FixedDigital, CreateUseWithLimiter) {
  const int kSampleRate = 44000;
  FixedGainController fixed_gc =
      CreateFixedGainController(kGainToApplyDb, kSampleRate, true);
  VectorFloatFrame vectors_with_float_frame(
      1, rtc::CheckedDivExact(kSampleRate, 100), kInputLevelLinear);
  auto float_frame = vectors_with_float_frame.float_frame_view();
  fixed_gc.Process(float_frame);
  const auto channel = float_frame.channel(0);
  EXPECT_LT(kInputLevelLinear, channel[0]);
}

TEST(AutomaticGainController2FixedDigital, GainShouldChangeOnSetGain) {
  constexpr float input_level = 1000.f;
  constexpr size_t num_frames = 5;
  constexpr size_t kSampleRate = 8000;
  constexpr float gain_db_no_change = 0.f;
  constexpr float gain_db_factor_10 = 20.f;

  FixedGainController fixed_gc_no_saturation =
      CreateFixedGainController(gain_db_no_change, kSampleRate, false);

  // Signal level is unchanged with 0 db gain.
  EXPECT_FLOAT_EQ(
      RunFixedGainControllerWithConstantInput(
          &fixed_gc_no_saturation, input_level, num_frames, kSampleRate),
      input_level);

  fixed_gc_no_saturation.SetGain(gain_db_factor_10);

  // +20db should increase signal by a factor of 10.
  EXPECT_FLOAT_EQ(
      RunFixedGainControllerWithConstantInput(
          &fixed_gc_no_saturation, input_level, num_frames, kSampleRate),
      input_level * 10);
}

}  // namespace webrtc
