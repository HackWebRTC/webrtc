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

TEST(QuicBandwidthPolicyTest,
     PreservesStartupProductionDespiteFreshLowEstimate) {
  QuicBandwidthPolicy policy;
  QuicBandwidthSample sample = Sample(100, 1000, 400'000, 200'000);
  sample.startup_complete = false;

  QuicMediaBudget budget = policy.Update(sample);

  EXPECT_TRUE(budget.fresh);
  EXPECT_EQ(budget.target_rate, DataRate::KilobitsPerSec(1000));
}

TEST(QuicBandwidthPolicyTest, StopsMediaBudgetWhenAcknowledgmentsGoStale) {
  QuicBandwidthPolicy policy;
  auto sample = Sample(100, 10000, 2'000'000, 2'000'000);
  EXPECT_TRUE(policy.Update(sample).fresh);
  sample.at_time = Timestamp::Millis(2100);
  EXPECT_EQ(policy.Update(sample).target_rate, DataRate::Zero());
  sample.at_time = Timestamp::Millis(2200);
  sample.acked_bytes += 1000;
  EXPECT_GT(policy.Update(sample).target_rate, DataRate::Zero());
}

TEST(QuicBandwidthPolicyTest,
     ExplicitNewConnectionRestoresStartupRegardlessOfAckTotals) {
  QuicBandwidthPolicy policy;
  auto old = Sample(100, 10000000, 2000000, 2000000);
  old.connection_id = {1};
  ASSERT_TRUE(policy.Update(old).capacity_known);
  EXPECT_EQ(policy.Update(Sample(2100, 10000000, 2000000, 2000000)).target_rate,
            DataRate::Zero());
  auto new_connection = Sample(2200, 1000, 2000000, 2000000);
  new_connection.connection_id = {2};
  new_connection.startup_complete = false;
  new_connection.max_bandwidth = new_connection.delivery_rate =
      DataRate::BitsPerSec(10000);
  EXPECT_FALSE(policy.Update(new_connection).capacity_known);
  EXPECT_EQ(policy.Update(new_connection).target_rate,
            DataRate::KilobitsPerSec(1000));
}

TEST(QuicBandwidthPolicyTest, StartsAtOneMegabitWithoutSample) {
  QuicBandwidthPolicy policy;

  QuicMediaBudget budget = policy.Update(
      Sample(/*time_ms=*/0, /*acked_bytes=*/0, /*max_bandwidth_bps=*/0,
             /*delivery_rate_bps=*/0));

  EXPECT_FALSE(budget.fresh);
  EXPECT_EQ(budget.target_rate, DataRate::KilobitsPerSec(1000));
}

TEST(QuicBandwidthPolicyTest, UpdatingInitialLimitsUsesNewStartRate) {
  QuicBandwidthPolicy policy(DataRate::BitsPerSec(1),
                             DataRate::BitsPerSec(128'000),
                             DataRate::PlusInfinity());

  policy.SetLimits(DataRate::BitsPerSec(384'000),
                   DataRate::BitsPerSec(2'128'000),
                   DataRate::BitsPerSec(4'128'000));

  EXPECT_EQ(
      policy
          .Update(Sample(/*time_ms=*/0, /*acked_bytes=*/0,
                         /*max_bandwidth_bps=*/0, /*delivery_rate_bps=*/0))
          .target_rate,
      DataRate::BitsPerSec(2'128'000));
}

TEST(QuicBandwidthPolicyTest, DynamicLimitsDoNotResetEstablishedEstimate) {
  QuicBandwidthPolicy policy(DataRate::BitsPerSec(1),
                             DataRate::BitsPerSec(128'000),
                             DataRate::BitsPerSec(4'128'000));
  ASSERT_EQ(policy.Update(Sample(100, 1000, 2'000'000, 2'000'000)).target_rate,
            DataRate::BitsPerSec(435'733));

  policy.SetLimits(DataRate::BitsPerSec(384'000),
                   DataRate::BitsPerSec(2'128'000),
                   DataRate::BitsPerSec(4'128'000));

  EXPECT_EQ(policy.Update(Sample(200, 2000, 2'000'000, 2'000'000)).target_rate,
            DataRate::BitsPerSec(681'919));
}

TEST(QuicBandwidthPolicyTest,
     SeparatesMeasuredWireCostAndEnforcedRecoveryReserve) {
  QuicBandwidthPolicy policy;

  QuicMediaBudget budget =
      policy.Update(Sample(100, 1000, 4'000'000, 2'000'000));

  EXPECT_TRUE(budget.fresh);
  EXPECT_EQ(budget.raw_capacity, DataRate::BitsPerSec(2'000'000));
  EXPECT_EQ(budget.overhead, DataRate::BitsPerSec(133334));
  EXPECT_EQ(budget.recovery_reserve, DataRate::BitsPerSec(200'000));
  EXPECT_EQ(budget.safety_headroom, DataRate::Zero());
  EXPECT_EQ(budget.usable_media_rate, DataRate::BitsPerSec(1'666'666));
  EXPECT_EQ(budget.target_rate, DataRate::BitsPerSec(1'133'333));
  EXPECT_EQ(budget.round_trip_time, TimeDelta::Millis(50));
}

TEST(QuicBandwidthPolicyTest, AppliesIncreaseEwmaAndImmediateDecrease) {
  QuicBandwidthPolicy policy;

  EXPECT_EQ(policy.Update(Sample(100, 1000, 8'000'000, 4'000'000)).target_rate,
            DataRate::BitsPerSec(1'466'666));
  EXPECT_EQ(policy.Update(Sample(200, 2000, 8'000'000, 8'000'000)).target_rate,
            DataRate::BitsPerSec(2'506'666));
  EXPECT_EQ(policy.Update(Sample(300, 3000, 800'000, 800'000)).target_rate,
            DataRate::BitsPerSec(666'666));
}

TEST(QuicBandwidthPolicyTest, DoesNotInflateCapacityToEncoderMinimum) {
  QuicBandwidthPolicy policy(DataRate::KilobitsPerSec(256),
                             DataRate::KilobitsPerSec(1000),
                             DataRate::BitsPerSec(4'128'000));

  EXPECT_EQ(policy.Update(Sample(100, 1000, 100'000, 100'000)).target_rate,
            DataRate::BitsPerSec(83333));
  EXPECT_EQ(
      policy.Update(Sample(200, 2000, 100'000'000, 100'000'000)).target_rate,
      DataRate::BitsPerSec(4'128'000));
}

TEST(QuicBandwidthPolicyTest, StopsAfterAckSampleBecomesStale) {
  QuicBandwidthPolicy policy;
  EXPECT_EQ(policy.Update(Sample(100, 1000, 8'000'000, 4'000'000)).target_rate,
            DataRate::BitsPerSec(1'466'666));

  EXPECT_TRUE(policy.Update(Sample(1000, 1000, 8'000'000, 4'000'000)).fresh);
  QuicMediaBudget stale =
      policy.Update(Sample(1200, 1000, 8'000'000, 4'000'000));
  EXPECT_FALSE(stale.fresh);
  EXPECT_EQ(stale.target_rate, DataRate::Zero());
}

TEST(QuicBandwidthPolicyTest, KeepsFreshSampleWithoutNewAckForOneSecond) {
  QuicBandwidthPolicy policy;
  policy.Update(Sample(100, 1000, 8'000'000, 4'000'000));

  QuicMediaBudget budget =
      policy.Update(Sample(1000, 1000, 8'000'000, 4'000'000));

  EXPECT_TRUE(budget.fresh);
  EXPECT_EQ(budget.target_rate, DataRate::BitsPerSec(1'839'999));
}

TEST(QuicBandwidthPolicyTest, RecoversFromFallbackWithEwma) {
  QuicBandwidthPolicy policy;
  policy.Update(Sample(100, 1000, 8'000'000, 4'000'000));
  policy.Update(Sample(1200, 1000, 8'000'000, 4'000'000));

  EXPECT_EQ(policy.Update(Sample(1300, 2000, 8'000'000, 4'000'000)).target_rate,
            DataRate::BitsPerSec(666'666));
}

TEST(QuicBandwidthPolicyTest,
     RedAndPacketizationAreChargedWhileProbeDemandIsIndependent) {
  QuicBandwidthPolicy policy;
  auto sample = Sample(0, 1000, 2000000, 2000000);
  sample.audio_payload_rate = DataRate::BitsPerSec(128000);
  sample.maximum_media_rate = DataRate::BitsPerSec(2128000);
  policy.Update(sample);
  sample.at_time = Timestamp::Millis(1000);
  sample.acked_bytes += 100000;
  sample.audio_payload_bytes = 16000;
  sample.audio_wire_bytes = 35000;
  sample.audio_packets = 50;
  sample.audio_redundant_payload_bytes = 13000;
  sample.audio_redundant_blocks = 50;
  sample.video_payload_bytes = 125000;
  sample.video_wire_bytes = 137500;
  auto budget = policy.Update(sample);
  EXPECT_EQ(budget.recovery_reserve.bps(), 200000);
  EXPECT_GT(budget.overhead.bps(), 250000);
  EXPECT_LT(budget.usable_media_rate.bps(), 1600000);
  EXPECT_GT(budget.probe_max_rate.bps(), 2700000);
  sample.at_time = Timestamp::Millis(1100);
  sample.acked_bytes += 100;
  sample.delivery_rate = DataRate::BitsPerSec(500000);
  auto low = policy.Update(sample);
  EXPECT_LT(low.target_rate.bps(), 300000);
  EXPECT_EQ(low.probe_max_rate, budget.probe_max_rate);
}
TEST(QuicBandwidthPolicyTest, TinyAudioPacketsCannotLockOutFixedRateAudio) {
  QuicBandwidthPolicy policy;
  auto sample = Sample(0, 1000, 3000000, 3000000);
  sample.audio_payload_rate = DataRate::BitsPerSec(128000);
  sample.maximum_media_rate = DataRate::BitsPerSec(2128000);
  policy.Update(sample);
  // Even a short startup burst must not turn fixed packet headers into a
  // payload multiplier subsequently applied to a 128 kbps configuration.
  sample.at_time = Timestamp::Millis(500);
  sample.acked_bytes += 100000;
  sample.audio_payload_bytes = 18;
  sample.audio_wire_bytes = 678;
  sample.audio_packets = 6;
  EXPECT_GT(policy.Update(sample).usable_media_rate.bps(), 1128000);
  for (int i = 1; i <= 80; ++i) {
    sample.at_time = Timestamp::Millis(500 + i * 500);
    sample.acked_bytes += 100000;
    EXPECT_GT(policy.Update(sample).target_rate.bps(), 128000);
  }
}

TEST(QuicBandwidthPolicyTest, StartupTerminatesOnAckStallIncludingNoFirstAck) {
  for (bool ever_ack : {false, true}) {
    QuicBandwidthPolicy policy;
    auto s = Sample(0, ever_ack ? 100 : 0, 100000, 100000);
    s.startup_complete = false;
    s.maximum_media_rate = DataRate::BitsPerSec(2000000);
    EXPECT_FALSE(policy.Update(s).capacity_known);
    s.at_time = Timestamp::Millis(1000);
    EXPECT_FALSE(policy.Update(s).capacity_known);
    s.at_time = Timestamp::Millis(1001);
    const auto stopped = policy.Update(s);
    EXPECT_STREQ(stopped.capacity_state, "no_ack_progress");
    EXPECT_EQ(stopped.target_rate, DataRate::Zero());
    s.at_time = Timestamp::Millis(1100);
    s.acked_bytes += 1000;
    EXPECT_TRUE(policy.Update(s).capacity_known);
    EXPECT_LT(policy.Update(s).target_rate.bps(), 100000);
  }
}

TEST(QuicBandwidthPolicyTest, StartupEndsAtQueueHighWaterOrFrameExpiry) {
  for (bool expired : {false, true}) {
    QuicBandwidthPolicy policy;
    auto s = Sample(0, 1000, 100000, 100000);
    s.startup_complete = false;
    s.maximum_media_rate = DataRate::BitsPerSec(2000000);
    s.oldest_video_queue_age = TimeDelta::Millis(499);
    EXPECT_FALSE(policy.Update(s).capacity_known);
    if (expired)
      s.expired_video_frames = 1;
    else
      s.oldest_video_queue_age = TimeDelta::Millis(500);
    const auto budget = policy.Update(s);
    EXPECT_STREQ(budget.capacity_state, "queue_overload");
    EXPECT_LT(budget.target_rate.bps(), 100000);
    s.oldest_video_queue_age = TimeDelta::Zero();
    s.expired_video_frames = 0;
    EXPECT_TRUE(policy.Update(s).capacity_known);
  }
}

TEST(QuicBandwidthPolicyTest, DemandCoverageMustBeContinuousWithANewAck) {
  QuicBandwidthPolicy policy;
  auto s = Sample(0, 1000, 3000000, 3000000, 100);
  s.startup_complete = false;
  s.maximum_media_rate = DataRate::BitsPerSec(2000000);
  EXPECT_FALSE(policy.Update(s).capacity_known);
  s.at_time = Timestamp::Millis(200);
  EXPECT_FALSE(policy.Update(s).capacity_known);  // no new ACK
  s.maximum_media_rate = DataRate::BitsPerSec(4000000);
  s.acked_bytes += 1000;
  EXPECT_FALSE(policy.Update(s).capacity_known);  // demand no longer fits
  s.at_time = Timestamp::Millis(300);
  s.maximum_media_rate = DataRate::BitsPerSec(2000000);
  EXPECT_FALSE(policy.Update(s).capacity_known);
  s.at_time = Timestamp::Millis(499);
  s.acked_bytes += 1000;
  EXPECT_FALSE(policy.Update(s).capacity_known);
  s.at_time = Timestamp::Millis(500);
  s.acked_bytes += 1000;
  const auto budget = policy.Update(s);
  EXPECT_STREQ(budget.capacity_state, "demand_satisfied");
  EXPECT_EQ(budget.target_rate, budget.usable_media_rate);
  EXPECT_GE(budget.target_rate, s.maximum_media_rate);
}

TEST(QuicBandwidthPolicyTest,
     IdleDoesNotSpendStartupButUpdatesCannotRegrantIt) {
  QuicBandwidthPolicy policy;
  auto s = Sample(0, 0, 0, 0);
  s.startup_complete = false;
  s.connection_id = {1};
  EXPECT_FALSE(policy.Update(s).capacity_known);
  s.at_time = Timestamp::Seconds(60);
  EXPECT_FALSE(policy.Update(s).capacity_known);
  s.maximum_media_rate = DataRate::BitsPerSec(2000000);
  EXPECT_FALSE(policy.Update(s).capacity_known);
  s.at_time = Timestamp::Seconds(62);
  EXPECT_TRUE(policy.Update(s).capacity_known);
  s.acked_bytes = 1000;
  s.max_bandwidth = s.delivery_rate = DataRate::BitsPerSec(100000);
  EXPECT_LT(policy.Update(s).target_rate.bps(), 100000);
  policy.SetLimits(DataRate::BitsPerSec(1000000), DataRate::BitsPerSec(2000000),
                   DataRate::BitsPerSec(4000000));
  s.maximum_media_rate = DataRate::Zero();
  s.acked_bytes = 1;  // counter anomaly is not a new connection
  EXPECT_TRUE(policy.Update(s).capacity_known);
  s.maximum_media_rate = DataRate::BitsPerSec(4000000);
  EXPECT_TRUE(policy.Update(s).capacity_known);
  s.connection_id = {2};
  s.acked_bytes = 100000;  // even when new total exceeds old
  EXPECT_FALSE(policy.Update(s).capacity_known);
  EXPECT_EQ(policy.Update(s).target_rate.bps(), 2000000);
}

TEST(QuicBandwidthPolicyTest, FullTinyAudioWindowUsesHeadersAndRedBlockCounts) {
  QuicBandwidthPolicy policy;
  auto s = Sample(0, 1000, 3000000, 3000000);
  s.audio_payload_rate = DataRate::BitsPerSec(128000);
  s.maximum_media_rate = DataRate::BitsPerSec(2128000);
  policy.Update(s);
  s.at_time = Timestamp::Millis(500);
  s.acked_bytes += 100000;
  s.audio_packets = 25;
  s.audio_payload_bytes = 75;
  // Speech -> silence: old RED payload greatly exceeds current primary.
  s.audio_redundant_payload_bytes = 8000;
  s.audio_redundant_blocks = 25;
  s.audio_wire_bytes = 75 + 8000 + 25 * 114;
  auto b = policy.Update(s);
  EXPECT_EQ(b.audio_redundancy_multiplier, 2.0);
  EXPECT_GE(b.audio_header_rate.bps(), 45600);
  EXPECT_LT(b.audio_header_rate.bps(), 48000);
  EXPECT_GT(b.usable_media_rate.bps(), 2000000);
  EXPECT_LT(b.probe_max_rate.bps(), 3000000);
  s.at_time = Timestamp::Millis(1000);
  s.acked_bytes += 100000;
  s.audio_packets += 1;
  s.audio_payload_bytes += 3;
  s.audio_wire_bytes += 113;
  EXPECT_EQ(policy.Update(s).audio_header_rate, b.audio_header_rate);
  s.at_time = Timestamp::Millis(1500);
  s.acked_bytes += 100000;
  s.audio_packets += 25;
  s.audio_payload_bytes += 8000;
  s.audio_redundant_payload_bytes += 8000;
  s.audio_redundant_blocks += 25;
  s.audio_wire_bytes += 16000 + 25 * 114;
  const auto normal = policy.Update(s);
  EXPECT_GT(normal.usable_media_rate.bps(), 2128000);
  EXPECT_GT(normal.target_rate.bps(), 1128000);  // still honors increase EWMA
  for (int i = 0; i < 10; ++i) {
    s.at_time += TimeDelta::Millis(100);
    s.acked_bytes += 10000;
    policy.Update(s);
  }
  EXPECT_GT(policy.Update(s).target_rate.bps(), 2000000);
}

TEST(QuicBandwidthPolicyTest, ChargesMultipleRedBlocksAndPlainOpusCorrectly) {
  QuicBandwidthPolicy policy;
  auto s = Sample(0, 1000, 3000000, 3000000);
  s.audio_payload_rate = DataRate::BitsPerSec(128000);
  s.maximum_media_rate = s.audio_payload_rate;
  policy.Update(s);
  s.at_time = Timestamp::Millis(500);
  s.acked_bytes += 100000;
  s.audio_packets = 25;
  s.audio_payload_bytes = 8000;
  s.audio_redundant_payload_bytes = 16000;
  s.audio_redundant_blocks = 50;
  s.audio_wire_bytes = 24000 + 25 * 118;
  EXPECT_EQ(policy.Update(s).audio_redundancy_multiplier, 3.0);
  s.at_time = Timestamp::Millis(1000);
  s.acked_bytes += 100000;
  s.audio_packets += 25;
  s.audio_payload_bytes += 8000;
  s.audio_wire_bytes += 8000 + 25 * 109;
  const auto b = policy.Update(s);
  EXPECT_DOUBLE_EQ(b.audio_redundancy_multiplier, 2.6);
  EXPECT_LT(b.audio_header_rate.bps(), 47200);
  EXPECT_GT(b.audio_header_rate.bps(), 43600);
}

TEST(QuicBandwidthPolicyTest, CruiseMaintenanceUsesConfiguredDemandNotEncoderOutput) {
  QuicBandwidthPolicy policy(DataRate::BitsPerSec(1128000),
                             DataRate::BitsPerSec(2128000),
                             DataRate::BitsPerSec(2128000));
  auto s = Sample(100, 1000, 4000000, 4000000, 130);
  s.audio_payload_rate = DataRate::BitsPerSec(128000);
  s.maximum_media_rate = DataRate::BitsPerSec(2128000);
  s.resume_media_rate = DataRate::BitsPerSec(1228000);
  const auto b = policy.Update(s);
  EXPECT_GT(b.maintenance_rate, DataRate::BitsPerSec(1600000));
  EXPECT_EQ(b.maintenance_rate, b.probe_max_rate);
  // No audio-only maintenance, and no padding without ACK progress.
  s.resume_media_rate = DataRate::Zero();
  EXPECT_EQ(policy.Update(s).maintenance_rate, DataRate::Zero());
  s.resume_media_rate = DataRate::BitsPerSec(1228000);
  s.at_time += TimeDelta::Seconds(2);
  EXPECT_EQ(policy.Update(s).maintenance_rate, DataRate::Zero());
}

TEST(QuicBandwidthPolicyTest, ResumeNeedsNewTwoRoundEvidenceNotClearedLowerBound) {
  QuicBandwidthPolicy policy(DataRate::BitsPerSec(1128000),
                             DataRate::BitsPerSec(2128000),
                             DataRate::BitsPerSec(2128000));
  auto s = Sample(0, 1000, 4000000, 600000, 130);
  s.connection_id = {1};
  s.audio_payload_rate = DataRate::BitsPerSec(128000);
  s.maximum_media_rate = DataRate::BitsPerSec(2128000);
  s.resume_media_rate = DataRate::BitsPerSec(1228000);
  s.evidence_round = 10;
  s.evidence_rate = DataRate::BitsPerSec(4000000); // old pre-loss evidence
  s.evidence_age = TimeDelta::Zero();
  EXPECT_LT(policy.Update(s).target_rate, s.resume_media_rate);
  // Lower-bound reset, ACK advancement and old evidence are insufficient.
  s.at_time += TimeDelta::Millis(100);
  s.acked_bytes += 1000;
  s.delivery_rate = DataRate::BitsPerSec(2000000);
  auto b = policy.Update(s);
  EXPECT_FALSE(b.resume_fast_path);
  EXPECT_LT(b.target_rate, s.resume_media_rate);
  s.at_time += TimeDelta::Millis(100);
  s.acked_bytes += 1000;
  s.evidence_round = 11;
  EXPECT_FALSE(policy.Update(s).resume_fast_path);
  s.at_time += TimeDelta::Millis(100);
  s.acked_bytes += 1000;
  s.evidence_round = 12;
  s.evidence_rate = s.delivery_rate;
  b = policy.Update(s);
  EXPECT_TRUE(b.resume_fast_path);
  EXPECT_EQ(b.target_rate, s.resume_media_rate);
  EXPECT_LE(b.target_rate, b.usable_media_rate);
}

TEST(QuicBandwidthPolicyTest, RecoveryCannotOverrideLowCapacityOrStaleEvidence) {
  QuicBandwidthPolicy policy(DataRate::BitsPerSec(1128000),
                             DataRate::BitsPerSec(2128000),
                             DataRate::BitsPerSec(2128000));
  auto s = Sample(0, 1000, 4000000, 600000, 130);
  s.audio_payload_rate = DataRate::BitsPerSec(128000);
  s.maximum_media_rate = DataRate::BitsPerSec(2128000);
  s.resume_media_rate = DataRate::BitsPerSec(1228000);
  s.evidence_round = 10;
  policy.Update(s);
  s.at_time += TimeDelta::Millis(100);
  s.acked_bytes += 1000;
  s.evidence_round = 20;
  s.evidence_rate = DataRate::BitsPerSec(4000000);
  s.evidence_age = TimeDelta::Zero();
  auto b = policy.Update(s);
  EXPECT_FALSE(b.resume_fast_path);
  EXPECT_LT(b.target_rate, s.resume_media_rate);
  s.delivery_rate = DataRate::BitsPerSec(2000000);
  s.evidence_age = TimeDelta::Seconds(2);
  s.at_time += TimeDelta::Millis(100);
  s.acked_bytes += 1000;
  EXPECT_FALSE(policy.Update(s).resume_fast_path);
}

TEST(QuicBandwidthPolicyTest, ResumeEvidenceCannotSurviveConnectionReplacement) {
  QuicBandwidthPolicy policy(DataRate::BitsPerSec(1128000),
                             DataRate::BitsPerSec(2128000),
                             DataRate::BitsPerSec(2128000));
  auto s = Sample(0, 1000, 4000000, 600000, 130);
  s.connection_id = {1};
  s.audio_payload_rate = DataRate::BitsPerSec(128000);
  s.maximum_media_rate = DataRate::BitsPerSec(2128000);
  s.resume_media_rate = DataRate::BitsPerSec(1228000);
  s.evidence_round = 100;
  policy.Update(s);
  s.connection_id = {2};
  s.evidence_round = 102;
  s.evidence_rate = DataRate::BitsPerSec(4000000);
  s.evidence_age = TimeDelta::Zero();
  s.at_time += TimeDelta::Millis(100);
  s.acked_bytes += 1000;
  s.delivery_rate = DataRate::BitsPerSec(2000000);
  EXPECT_FALSE(policy.Update(s).resume_fast_path);
  // Demanding a new publication cannot reuse the old recovery rounds either.
  s.resume_media_rate = DataRate::BitsPerSec(1400000);
  EXPECT_FALSE(policy.Update(s).resume_fast_path);
}

TEST(QuicBandwidthPolicyTest, AckOutageRetainsFreshRoundBarrierForResume) {
  QuicBandwidthPolicy policy(DataRate::BitsPerSec(1128000),
                             DataRate::BitsPerSec(2128000),
                             DataRate::BitsPerSec(2128000));
  auto s = Sample(0, 1000, 4000000, 4000000, 130);
  s.audio_payload_rate = DataRate::BitsPerSec(128000);
  s.maximum_media_rate = DataRate::BitsPerSec(2128000);
  s.resume_media_rate = DataRate::BitsPerSec(1228000);
  s.evidence_round = 10;
  s.evidence_rate = DataRate::BitsPerSec(4000000);
  s.evidence_age = TimeDelta::Zero();
  EXPECT_GE(policy.Update(s).target_rate, s.resume_media_rate);
  s.at_time += TimeDelta::Seconds(2);
  EXPECT_EQ(policy.Update(s).target_rate, DataRate::Zero());
  s.at_time += TimeDelta::Millis(100);
  s.acked_bytes += 10000;
  s.delivery_rate = DataRate::BitsPerSec(2000000);
  EXPECT_FALSE(policy.Update(s).resume_fast_path);
  s.at_time += TimeDelta::Millis(100);
  s.acked_bytes += 10000;
  s.evidence_round = 12;
  s.evidence_rate = s.delivery_rate;
  const auto recovered = policy.Update(s);
  EXPECT_TRUE(recovered.resume_fast_path);
  EXPECT_EQ(recovered.target_rate, s.resume_media_rate);
}
}  // namespace
}  // namespace webrtc::roq
