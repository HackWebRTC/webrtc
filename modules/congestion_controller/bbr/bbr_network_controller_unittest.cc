/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <memory>

#include "modules/congestion_controller/bbr/bbr_factory.h"
#include "modules/congestion_controller/bbr/bbr_network_controller.h"
#include "modules/congestion_controller/network_control/test/mock_network_control.h"
#include "modules/congestion_controller/network_control/test/network_control_tester.h"
#include "test/gtest.h"

using testing::Field;
using testing::Matcher;
using testing::AllOf;
using testing::Ge;
using testing::Le;
using testing::NiceMock;
using testing::Property;
using testing::StrictMock;
using testing::_;

namespace webrtc {
namespace bbr {
namespace test {
namespace {

const DataRate kInitialBitrate = DataRate::kbps(60);
const Timestamp kDefaultStartTime = Timestamp::ms(10000000);

constexpr double kDataRateMargin = 0.3;
constexpr double kMinDataRateFactor = 1 - kDataRateMargin;
constexpr double kMaxDataRateFactor = 1 + kDataRateMargin;
inline Matcher<TargetTransferRate> TargetRateCloseTo(DataRate rate) {
  DataRate min_data_rate = rate * kMinDataRateFactor;
  DataRate max_data_rate = rate * kMaxDataRateFactor;
  return Field(&TargetTransferRate::target_rate,
               AllOf(Ge(min_data_rate), Le(max_data_rate)));
}

NetworkControllerConfig InitialConfig(
    int starting_bandwidth_kbps = kInitialBitrate.kbps(),
    int min_data_rate_kbps = 0,
    int max_data_rate_kbps = 5 * kInitialBitrate.kbps()) {
  NetworkControllerConfig config;
  config.constraints.at_time = kDefaultStartTime;
  config.constraints.min_data_rate = DataRate::kbps(min_data_rate_kbps);
  config.constraints.max_data_rate = DataRate::kbps(max_data_rate_kbps);
  config.starting_bandwidth = DataRate::kbps(starting_bandwidth_kbps);
  return config;
}

NetworkRouteChange CreateRouteChange(Timestamp at_time,
                                     DataRate start_rate,
                                     DataRate min_rate = DataRate::Zero(),
                                     DataRate max_rate = DataRate::Infinity()) {
  NetworkRouteChange route_change;
  route_change.at_time = at_time;
  route_change.constraints.at_time = at_time;
  route_change.constraints.min_data_rate = min_rate;
  route_change.constraints.max_data_rate = max_rate;
  route_change.starting_rate = start_rate;
  return route_change;
}
}  // namespace

class BbrNetworkControllerTest : public ::testing::Test {
 protected:
  BbrNetworkControllerTest() {}
  ~BbrNetworkControllerTest() override {}
};

TEST_F(BbrNetworkControllerTest, SendsConfigurationOnInitialization) {
  StrictMock<webrtc::test::MockNetworkControllerObserver> observer;
  EXPECT_CALL(observer,
              OnTargetTransferRate(TargetRateCloseTo(kInitialBitrate)));
  EXPECT_CALL(observer, OnPacerConfig(Property(&PacerConfig::data_rate,
                                               Ge(kInitialBitrate))));
  EXPECT_CALL(observer,
              OnCongestionWindow(Field(&CongestionWindow::data_window,
                                       Property(&DataSize::IsFinite, true))));

  std::unique_ptr<NetworkControllerInterface> controller_;
  controller_.reset(new BbrNetworkController(&observer, InitialConfig()));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(BbrNetworkControllerTest, SendsConfigurationOnNetworkRouteChanged) {
  StrictMock<webrtc::test::MockNetworkControllerObserver> observer;
  EXPECT_CALL(observer, OnTargetTransferRate(_));
  EXPECT_CALL(observer, OnPacerConfig(_));
  EXPECT_CALL(observer, OnCongestionWindow(_));
  std::unique_ptr<NetworkControllerInterface> controller_;
  controller_.reset(new BbrNetworkController(&observer, InitialConfig()));

  DataRate new_bitrate = DataRate::bps(200000);
  EXPECT_CALL(observer, OnTargetTransferRate(TargetRateCloseTo(new_bitrate)));
  EXPECT_CALL(observer, OnPacerConfig(Property(&PacerConfig::data_rate,
                                               Ge(kInitialBitrate))));
  EXPECT_CALL(observer, OnCongestionWindow(_));
  controller_->OnNetworkRouteChange(
      CreateRouteChange(kDefaultStartTime, new_bitrate));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

// Bandwidth estimation is updated when feedbacks are received.
// Feedbacks which show an increasing delay cause the estimation to be reduced.
TEST_F(BbrNetworkControllerTest, UpdatesTargetSendRate) {
  BbrNetworkControllerFactory factory;
  webrtc::test::NetworkControllerTester tester(&factory,
                                               InitialConfig(60, 0, 600));
  auto packet_producer = &webrtc::test::SimpleTargetRateProducer::ProduceNext;

  tester.RunSimulation(TimeDelta::seconds(5), TimeDelta::ms(10),
                       DataRate::kbps(300), TimeDelta::ms(100),
                       packet_producer);
  EXPECT_GE(tester.GetState().target_rate->target_rate,
            DataRate::kbps(300) * kMinDataRateFactor);
  EXPECT_LE(tester.GetState().target_rate->target_rate,
            DataRate::kbps(300) * kMaxDataRateFactor);

  tester.RunSimulation(TimeDelta::seconds(30), TimeDelta::ms(10),
                       DataRate::kbps(500), TimeDelta::ms(100),
                       packet_producer);
  EXPECT_GE(tester.GetState().target_rate->target_rate,
            DataRate::kbps(500) * kMinDataRateFactor);
  EXPECT_LE(tester.GetState().target_rate->target_rate,
            DataRate::kbps(500) * kMaxDataRateFactor);

  tester.RunSimulation(TimeDelta::seconds(30), TimeDelta::ms(10),
                       DataRate::kbps(100), TimeDelta::ms(200),
                       packet_producer);
  EXPECT_GE(tester.GetState().target_rate->target_rate,
            DataRate::kbps(100) * kMinDataRateFactor);
  EXPECT_LE(tester.GetState().target_rate->target_rate,
            DataRate::kbps(100) * kMaxDataRateFactor);
}

}  // namespace test
}  // namespace bbr
}  // namespace webrtc
