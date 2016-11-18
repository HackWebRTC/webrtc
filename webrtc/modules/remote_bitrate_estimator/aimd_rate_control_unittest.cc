/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <memory>

#include "webrtc/modules/audio_coding/audio_network_adaptor/smoothing_filter.h"
#include "webrtc/modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

constexpr int64_t kClockInitialTime = 123456;

struct AimdRateControlStates {
  std::unique_ptr<AimdRateControl> aimd_rate_control;
  std::unique_ptr<SimulatedClock> simulated_clock;
};

AimdRateControlStates CreateAimdRateControlStates() {
  AimdRateControlStates states;
  states.aimd_rate_control.reset(new AimdRateControl());
  states.simulated_clock.reset(new SimulatedClock(kClockInitialTime));
  return states;
}

void InitBitrate(const AimdRateControlStates& states,
                 int bitrate,
                 int64_t now_ms) {
  // Reduce the bitrate by 1000 to compensate for the Update after SetEstimate.
  bitrate -= 1000;

  states.aimd_rate_control->SetEstimate(bitrate, now_ms);
}

void UpdateRateControl(const AimdRateControlStates& states,
                       const BandwidthUsage& bandwidth_usage,
                       int bitrate,
                       int64_t now_ms) {
  RateControlInput input(bandwidth_usage, rtc::Optional<uint32_t>(bitrate),
                         now_ms);
  states.aimd_rate_control->Update(&input, now_ms);
  states.aimd_rate_control->UpdateBandwidthEstimate(now_ms);
}

}  // namespace

TEST(AimdRateControlTest, MinNearMaxIncreaseRateOnLowBandwith) {
  auto states = CreateAimdRateControlStates();
  constexpr int kBitrate = 30000;
  InitBitrate(states, kBitrate, states.simulated_clock->TimeInMilliseconds());
  EXPECT_EQ(4000, states.aimd_rate_control->GetNearMaxIncreaseRateBps());
}

TEST(AimdRateControlTest, NearMaxIncreaseRateIs5kbpsOn90kbpsAnd200msRtt) {
  auto states = CreateAimdRateControlStates();
  constexpr int kBitrate = 90000;
  InitBitrate(states, kBitrate, states.simulated_clock->TimeInMilliseconds());
  EXPECT_EQ(5000, states.aimd_rate_control->GetNearMaxIncreaseRateBps());
}

TEST(AimdRateControlTest, NearMaxIncreaseRateIs5kbpsOn60kbpsAnd100msRtt) {
  auto states = CreateAimdRateControlStates();
  constexpr int kBitrate = 60000;
  InitBitrate(states, kBitrate, states.simulated_clock->TimeInMilliseconds());
  states.aimd_rate_control->SetRtt(100);
  EXPECT_EQ(5000, states.aimd_rate_control->GetNearMaxIncreaseRateBps());
}

TEST(AimdRateControlTest, UnknownBitrateDecreaseBeforeFirstOveruse) {
  auto states = CreateAimdRateControlStates();
  EXPECT_EQ(rtc::Optional<int>(),
            states.aimd_rate_control->GetLastBitrateDecreaseBps());
}

TEST(AimdRateControlTest, GetLastBitrateDecrease) {
  auto states = CreateAimdRateControlStates();
  constexpr int kBitrate = 300000;
  InitBitrate(states, kBitrate, states.simulated_clock->TimeInMilliseconds());
  UpdateRateControl(states, kBwOverusing, kBitrate - 2000,
                    states.simulated_clock->TimeInMilliseconds());
  EXPECT_EQ(rtc::Optional<int>(46700),
            states.aimd_rate_control->GetLastBitrateDecreaseBps());
}

}  // namespace webrtc
