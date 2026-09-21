/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/quic_network_controller.h"

#include <memory>

#include "api/environment/environment.h"
#include "api/roq/roq_session.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"

namespace webrtc::roq {
namespace {

constexpr Timestamp kNow = Timestamp::Millis(100);

void ExpectSingleCcUpdate(const NetworkControlUpdate& update,
                          DataRate target_rate,
                          TimeDelta rtt) {
  ASSERT_TRUE(update.target_rate.has_value());
  EXPECT_EQ(update.target_rate->target_rate, target_rate);
  EXPECT_EQ(update.target_rate->network_estimate.bandwidth, target_rate);
  EXPECT_EQ(update.target_rate->network_estimate.round_trip_time, rtt);
  EXPECT_TRUE(update.target_rate->is_bandwidth_limited);
  EXPECT_EQ(update.target_rate->cwnd_reduce_ratio, 0);
  ASSERT_TRUE(update.pacer_config.has_value());
  EXPECT_NEAR(update.pacer_config->data_rate().bps(), target_rate.bps(), 100);
  EXPECT_EQ(update.pacer_config->pad_rate(), DataRate::Zero());
  EXPECT_TRUE(update.probe_cluster_configs.empty());
  EXPECT_FALSE(update.congestion_window.has_value());
}

TEST(QuicNetworkControllerTest, EmitsStartupBudgetWithoutQuicSample) {
  RoqSessionStats stats;
  Environment env = CreateTestEnvironment();
  NetworkControllerConfig config(env);
  QuicNetworkControllerFactory factory([&] { return stats; });
  std::unique_ptr<NetworkControllerInterface> controller =
      factory.Create(config);

  ExpectSingleCcUpdate(controller->OnProcessInterval({.at_time = kNow}),
                       DataRate::KilobitsPerSec(1000), TimeDelta::Zero());
  EXPECT_EQ(factory.GetProcessInterval(), TimeDelta::Millis(100));
}

TEST(QuicNetworkControllerTest, ConvertsLiveQuicStatsToTargetAndPacer) {
  RoqSessionStats stats = {.rtt_ns = 50'000'000,
                           .delivery_rate_bytes_per_second = 250'000,
                           .max_bandwidth_bytes_per_second = 500'000,
                           .acked_bytes = 1000,
                           .startup_exit_cwnd = 12000};
  Environment env = CreateTestEnvironment();
  NetworkControllerConfig config(env);
  QuicNetworkControllerFactory factory([&] { return stats; });
  std::unique_ptr<NetworkControllerInterface> controller =
      factory.Create(config);

  ExpectSingleCcUpdate(controller->OnProcessInterval({.at_time = kNow}),
                       DataRate::BitsPerSec(1'133'333), TimeDelta::Millis(50));
}

TEST(QuicNetworkControllerTest, IgnoresWebrtcCapacitySignals) {
  RoqSessionStats stats = {.rtt_ns = 50'000'000,
                           .delivery_rate_bytes_per_second = 250'000,
                           .max_bandwidth_bytes_per_second = 500'000,
                           .acked_bytes = 1000,
                           .startup_exit_cwnd = 12000};
  Environment env = CreateTestEnvironment();
  NetworkControllerConfig config(env);
  QuicNetworkControllerFactory factory([&] { return stats; });
  std::unique_ptr<NetworkControllerInterface> controller =
      factory.Create(config);
  NetworkControlUpdate initial =
      controller->OnProcessInterval({.at_time = kNow});
  ASSERT_TRUE(initial.target_rate.has_value());

  ExpectSingleCcUpdate(controller->OnRemoteBitrateReport(
                           {.receive_time = kNow + TimeDelta::Millis(1),
                            .bandwidth = DataRate::KilobitsPerSec(64)}),
                       DataRate::BitsPerSec(1'133'333), TimeDelta::Millis(50));
  ExpectSingleCcUpdate(controller->OnRemoteBitrateReport(
                           {.receive_time = kNow + TimeDelta::Millis(2),
                            .bandwidth = DataRate::KilobitsPerSec(64)}),
                       DataRate::BitsPerSec(1'133'333), TimeDelta::Millis(50));
  ExpectSingleCcUpdate(controller->OnRoundTripTimeUpdate(
                           {.receive_time = kNow + TimeDelta::Millis(3),
                            .round_trip_time = TimeDelta::Seconds(9),
                            .smoothed = true}),
                       DataRate::BitsPerSec(1'133'333), TimeDelta::Millis(50));
}

TEST(QuicNetworkControllerTest, ReportsRtpLossWithoutChangingCapacity) {
  RoqSessionStats stats = {.rtt_ns = 50'000'000,
                           .delivery_rate_bytes_per_second = 250'000,
                           .max_bandwidth_bytes_per_second = 500'000,
                           .acked_bytes = 1000,
                           .startup_exit_cwnd = 12000};
  Environment env = CreateTestEnvironment();
  NetworkControllerConfig config(env);
  QuicNetworkControllerFactory factory([&] { return stats; });
  std::unique_ptr<NetworkControllerInterface> controller =
      factory.Create(config);
  NetworkControlUpdate initial =
      controller->OnProcessInterval({.at_time = kNow});
  ASSERT_TRUE(initial.target_rate.has_value());

  NetworkControlUpdate loss = controller->OnTransportLossReport(
      {.receive_time = kNow + TimeDelta::Millis(1),
       .start_time = kNow,
       .end_time = kNow + TimeDelta::Millis(1),
       .packets_lost_delta = 25,
       .packets_received_delta = 75});

  ExpectSingleCcUpdate(loss, DataRate::BitsPerSec(1'133'333),
                       TimeDelta::Millis(50));
  EXPECT_FLOAT_EQ(loss.target_rate->network_estimate.loss_rate_ratio, 0.25f);
}

TEST(QuicNetworkControllerTest, RoutesStartupEvidenceAndConnectionIdentity) {
  RoqSessionStats stats;
  stats.source_connection_id = {1};
  stats.rtt_ns = 50000000;
  stats.max_bandwidth_bytes_per_second = 20000;
  stats.delivery_rate_bytes_per_second = 20000;
  stats.acked_bytes = 1000;
  stats.configured_max_media_bitrate_bps = 2000000;
  Environment env = CreateTestEnvironment();
  NetworkControllerConfig config(env);
  QuicNetworkControlState state;
  QuicNetworkControllerFactory factory(
      [&] { return stats; },
      [&](const QuicNetworkControlState& s) { state = s; });
  auto controller = factory.Create(config);
  ExpectSingleCcUpdate(controller->OnProcessInterval({.at_time = kNow}),
                       DataRate::KilobitsPerSec(1000), TimeDelta::Millis(50));
  EXPECT_FALSE(state.capacity_known);
  EXPECT_STREQ(state.capacity_state, "discovering");
  stats.expired_video_frames = 1;
  (void)controller->OnProcessInterval(
      {.at_time = kNow + TimeDelta::Millis(100)});
  EXPECT_STREQ(state.capacity_state, "queue_overload");
  EXPECT_LT(state.target_rate.bps(), 160000);
  stats.source_connection_id = {2};
  stats.expired_video_frames = 0;
  (void)controller->OnProcessInterval(
      {.at_time = kNow + TimeDelta::Millis(200)});
  EXPECT_STREQ(state.capacity_state, "discovering");
  EXPECT_EQ(state.target_rate.bps(), 1000000);
  stats.startup_exit_cwnd = 12000;
  (void)controller->OnProcessInterval(
      {.at_time = kNow + TimeDelta::Millis(300)});
  EXPECT_STREQ(state.capacity_state, "bbr_startup_complete");
}

TEST(QuicNetworkControllerTest, RoutesMaintenanceDemandAndNewDeliveryEvidence) {
  RoqSessionStats stats;
  stats.source_connection_id = {1};
  stats.acked_bytes = 1000;
  stats.startup_exit_cwnd = 13500;
  stats.rtt_ns = 130000000;
  stats.max_bandwidth_bytes_per_second = 500000;
  stats.delivery_rate_bytes_per_second = 75000;
  stats.configured_audio_bitrate_bps = 128000;
  stats.configured_max_media_bitrate_bps = 2128000;
  stats.configured_resume_media_bitrate_bps = 1228000;
  stats.bandwidth_evidence_round = 10;
  stats.bandwidth_evidence_bytes_per_second = 500000;
  stats.bandwidth_evidence_age_ns = 0;
  QuicNetworkControlState observed;
  QuicNetworkControllerFactory factory([&] { return stats; },
      [&](const auto& value) { observed = value; },
      DataRate::BitsPerSec(1128000), DataRate::BitsPerSec(2128000),
      DataRate::BitsPerSec(2128000));
  Environment env = CreateTestEnvironment();
  NetworkControllerConfig config(env);
  auto controller = factory.Create(config);
  EXPECT_TRUE(controller->OnProcessInterval({.at_time = Timestamp::Millis(0)}).target_rate);
  EXPECT_GT(observed.maintenance_rate.bps(), 1600000);
  stats.delivery_rate_bytes_per_second = 250000;
  stats.acked_bytes += 10000;
  EXPECT_TRUE(controller->OnProcessInterval({.at_time = Timestamp::Millis(100)}).target_rate);
  EXPECT_FALSE(observed.resume_fast_path);
  stats.bandwidth_evidence_round = 12;
  stats.bandwidth_evidence_bytes_per_second = 250000;
  stats.acked_bytes += 10000;
  EXPECT_TRUE(controller->OnProcessInterval({.at_time = Timestamp::Millis(200)}).target_rate);
  EXPECT_TRUE(observed.resume_fast_path);
  EXPECT_EQ(observed.target_rate, DataRate::BitsPerSec(1228000));
}

TEST(QuicNetworkControllerTest, ResumeThresholdIsNotRoundedBelowNativeMinimum) {
  RoqSessionStats stats;
  stats.acked_bytes = 1000;
  stats.startup_exit_cwnd = 13500;
  stats.rtt_ns = 130000000;
  stats.max_bandwidth_bytes_per_second = 500000;
  stats.delivery_rate_bytes_per_second = 75000;
  stats.configured_audio_bitrate_bps = 128000;
  stats.configured_max_media_bitrate_bps = 2128000;
  // Not representable as an integral byte count in PacerConfig's 40-ms window.
  stats.configured_resume_media_bitrate_bps = 1228099;
  EXPECT_LT(PacerConfig::Create(Timestamp::Millis(0),
                               DataRate::BitsPerSec(1228099), DataRate::Zero(),
                               TimeDelta::Millis(40)).data_rate().bps(),
            1228099);
  stats.bandwidth_evidence_round = 10;
  stats.bandwidth_evidence_age_ns = 0;
  QuicNetworkControlState observed;
  QuicNetworkControllerFactory factory(
      [&] { return stats; }, [&](const auto& value) { observed = value; },
      DataRate::BitsPerSec(1128000), DataRate::BitsPerSec(2128000),
      DataRate::BitsPerSec(2128000));
  Environment env = CreateTestEnvironment();
  auto controller = factory.Create(NetworkControllerConfig(env));
  (void)controller->OnProcessInterval({.at_time = Timestamp::Millis(0)});
  stats.delivery_rate_bytes_per_second = 250000;
  stats.bandwidth_evidence_bytes_per_second = 250000;
  stats.bandwidth_evidence_round = 12;
  stats.acked_bytes += 10000;
  const auto update =
      controller->OnProcessInterval({.at_time = Timestamp::Millis(100)});
  ASSERT_TRUE(observed.resume_fast_path);
  EXPECT_EQ(update.target_rate->target_rate.bps(), 1228099);
  EXPECT_EQ(observed.target_rate.bps(), 1228099);
  EXPECT_LE(observed.target_rate, observed.usable_media_rate);
}
}  // namespace
}  // namespace webrtc::roq
