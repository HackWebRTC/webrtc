/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/fixed_rate_network_controller.h"

#include <memory>

#include "api/environment/environment.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/timestamp.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"

namespace webrtc::roq {
namespace {

constexpr DataRate kMediaRate = DataRate::BitsPerSec(4'128'000);
constexpr Timestamp kNow = Timestamp::Millis(1234);

void ExpectFixedUpdate(const NetworkControlUpdate& update) {
  ASSERT_TRUE(update.target_rate.has_value());
  EXPECT_EQ(update.target_rate->target_rate, kMediaRate);
  EXPECT_FALSE(update.target_rate->is_bandwidth_limited);
  EXPECT_EQ(update.target_rate->cwnd_reduce_ratio, 0);
  ASSERT_TRUE(update.pacer_config.has_value());
  EXPECT_EQ(update.pacer_config->data_rate(), kMediaRate);
  EXPECT_EQ(update.pacer_config->pad_rate(), DataRate::Zero());
  EXPECT_TRUE(update.probe_cluster_configs.empty());
  EXPECT_FALSE(update.congestion_window.has_value());
}

TEST(FixedRateNetworkControllerTest, EmitsFixedRateWithoutProbeOrPadding) {
  Environment env = CreateTestEnvironment();
  NetworkControllerConfig config(env);
  config.constraints.at_time = kNow;
  FixedRateNetworkControllerFactory factory(kMediaRate);
  std::unique_ptr<NetworkControllerInterface> controller =
      factory.Create(config);

  ExpectFixedUpdate(controller->OnProcessInterval({.at_time = kNow}));
}

TEST(FixedRateNetworkControllerTest, IgnoresLossAndRemoteBandwidthChanges) {
  Environment env = CreateTestEnvironment();
  NetworkControllerConfig config(env);
  config.constraints.at_time = kNow;
  FixedRateNetworkControllerFactory factory(kMediaRate);
  std::unique_ptr<NetworkControllerInterface> controller =
      factory.Create(config);

  TransportLossReport loss = {
      .receive_time = kNow,
      .start_time = kNow - TimeDelta::Seconds(1),
      .end_time = kNow,
      .packets_lost_delta = 90,
      .packets_received_delta = 10,
  };
  ExpectFixedUpdate(controller->OnTransportLossReport(loss));
  ExpectFixedUpdate(controller->OnRemoteBitrateReport(
      {.receive_time = kNow, .bandwidth = DataRate::KilobitsPerSec(64)}));
}

}  // namespace
}  // namespace webrtc::roq
