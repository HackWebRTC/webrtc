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
  EXPECT_EQ(update.pacer_config->data_rate(), target_rate);
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
                       DataRate::BitsPerSec(1'100'000), TimeDelta::Millis(50));
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
                       DataRate::BitsPerSec(1'100'000), TimeDelta::Millis(50));
  ExpectSingleCcUpdate(controller->OnRemoteBitrateReport(
                           {.receive_time = kNow + TimeDelta::Millis(2),
                            .bandwidth = DataRate::KilobitsPerSec(64)}),
                       DataRate::BitsPerSec(1'100'000), TimeDelta::Millis(50));
  ExpectSingleCcUpdate(controller->OnRoundTripTimeUpdate(
                           {.receive_time = kNow + TimeDelta::Millis(3),
                            .round_trip_time = TimeDelta::Seconds(9),
                            .smoothed = true}),
                       DataRate::BitsPerSec(1'100'000), TimeDelta::Millis(50));
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

  ExpectSingleCcUpdate(loss, DataRate::BitsPerSec(1'100'000),
                       TimeDelta::Millis(50));
  EXPECT_FLOAT_EQ(loss.target_rate->network_estimate.loss_rate_ratio, 0.25f);
}

}  // namespace
}  // namespace webrtc::roq
