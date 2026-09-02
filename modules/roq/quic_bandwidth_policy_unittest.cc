/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/quic_bandwidth_policy.h"

#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc::roq {
namespace {

QuicBandwidthSample Sample(int64_t time_ms,
                           int64_t acked_bytes,
                           int64_t max_bandwidth_bps,
                           int64_t delivery_rate_bps,
                           int64_t rtt_ms = 50) {
  return {.at_time = Timestamp::Millis(time_ms),
          .acked_bytes = static_cast<uint64_t>(acked_bytes),
          .startup_complete = true,
          .max_bandwidth = DataRate::BitsPerSec(max_bandwidth_bps),
          .delivery_rate = DataRate::BitsPerSec(delivery_rate_bps),
          .round_trip_time = TimeDelta::Millis(rtt_ms)};
}

TEST(QuicBandwidthPolicyTest, KeepsStartupFallbackUntilBbr2ExitsStartup) {
  QuicBandwidthPolicy policy;
  QuicBandwidthSample sample = Sample(100, 1000, 400'000, 200'000);
  sample.startup_complete = false;

  QuicMediaBudget budget = policy.Update(sample);

  EXPECT_FALSE(budget.fresh);
  EXPECT_EQ(budget.target_rate, DataRate::KilobitsPerSec(1000));
}

TEST(QuicBandwidthPolicyTest, StartsAtOneMegabitWithoutSample) {
  QuicBandwidthPolicy policy;

  QuicMediaBudget budget = policy.Update(
      Sample(/*time_ms=*/0, /*acked_bytes=*/0, /*max_bandwidth_bps=*/0,
             /*delivery_rate_bps=*/0));

  EXPECT_FALSE(budget.fresh);
  EXPECT_EQ(budget.target_rate, DataRate::KilobitsPerSec(1000));
}

TEST(QuicBandwidthPolicyTest, UsesMinimumSignalAndDeductsTwentyFivePercent) {
  QuicBandwidthPolicy policy;

  QuicMediaBudget budget =
      policy.Update(Sample(100, 1000, 4'000'000, 2'000'000));

  EXPECT_TRUE(budget.fresh);
  EXPECT_EQ(budget.raw_capacity, DataRate::BitsPerSec(2'000'000));
  EXPECT_EQ(budget.overhead, DataRate::BitsPerSec(100'000));
  EXPECT_EQ(budget.recovery_reserve, DataRate::BitsPerSec(200'000));
  EXPECT_EQ(budget.safety_headroom, DataRate::BitsPerSec(200'000));
  EXPECT_EQ(budget.usable_media_rate, DataRate::BitsPerSec(1'500'000));
  EXPECT_EQ(budget.target_rate, DataRate::BitsPerSec(1'100'000));
  EXPECT_EQ(budget.round_trip_time, TimeDelta::Millis(50));
}

TEST(QuicBandwidthPolicyTest, AppliesIncreaseEwmaAndImmediateDecrease) {
  QuicBandwidthPolicy policy;

  EXPECT_EQ(policy.Update(Sample(100, 1000, 8'000'000, 4'000'000)).target_rate,
            DataRate::BitsPerSec(1'400'000));
  EXPECT_EQ(policy.Update(Sample(200, 2000, 8'000'000, 8'000'000)).target_rate,
            DataRate::BitsPerSec(2'320'000));
  EXPECT_EQ(policy.Update(Sample(300, 3000, 800'000, 800'000)).target_rate,
            DataRate::BitsPerSec(600'000));
}

TEST(QuicBandwidthPolicyTest, ClampsToConfiguredBounds) {
  QuicBandwidthPolicy policy;

  EXPECT_EQ(policy.Update(Sample(100, 1000, 100'000, 100'000)).target_rate,
            DataRate::KilobitsPerSec(256));
  EXPECT_EQ(
      policy.Update(Sample(200, 2000, 100'000'000, 100'000'000)).target_rate,
      DataRate::BitsPerSec(4'128'000));
}

TEST(QuicBandwidthPolicyTest, FallsBackAfterAckSampleBecomesStale) {
  QuicBandwidthPolicy policy;
  EXPECT_EQ(policy.Update(Sample(100, 1000, 8'000'000, 4'000'000)).target_rate,
            DataRate::BitsPerSec(1'400'000));

  EXPECT_TRUE(policy.Update(Sample(1000, 1000, 8'000'000, 4'000'000)).fresh);
  QuicMediaBudget stale =
      policy.Update(Sample(1200, 1000, 8'000'000, 4'000'000));
  EXPECT_FALSE(stale.fresh);
  EXPECT_EQ(stale.target_rate, DataRate::KilobitsPerSec(1000));
}

TEST(QuicBandwidthPolicyTest, KeepsFreshSampleWithoutNewAckForOneSecond) {
  QuicBandwidthPolicy policy;
  policy.Update(Sample(100, 1000, 8'000'000, 4'000'000));

  QuicMediaBudget budget =
      policy.Update(Sample(1000, 1000, 8'000'000, 4'000'000));

  EXPECT_TRUE(budget.fresh);
  EXPECT_EQ(budget.target_rate, DataRate::BitsPerSec(1'720'000));
}

TEST(QuicBandwidthPolicyTest, RecoversFromFallbackWithEwma) {
  QuicBandwidthPolicy policy;
  policy.Update(Sample(100, 1000, 8'000'000, 4'000'000));
  policy.Update(Sample(1200, 1000, 8'000'000, 4'000'000));

  EXPECT_EQ(policy.Update(Sample(1300, 2000, 8'000'000, 4'000'000)).target_rate,
            DataRate::BitsPerSec(1'400'000));
}

}  // namespace
}  // namespace webrtc::roq
