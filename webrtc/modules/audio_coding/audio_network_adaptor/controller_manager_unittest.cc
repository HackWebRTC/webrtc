/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "webrtc/test/gtest.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/controller_manager.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/mock/mock_controller.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

using ::testing::NiceMock;

namespace {

constexpr size_t kNumControllers = 4;
constexpr int kChracteristicBandwithBps[2] = {15000, 0};
constexpr float kChracteristicPacketLossFraction[2] = {0.2f, 0.0f};
constexpr int kMinReorderingTimeMs = 200;
constexpr int kFactor = 100;
constexpr float kMinReorderingSquareDistance = 1.0f / kFactor / kFactor;

// |kMinUplinkBandwidthBps| and |kMaxUplinkBandwidthBps| are copied from
// controller_manager.cc
constexpr int kMinUplinkBandwidthBps = 0;
constexpr int kMaxUplinkBandwidthBps = 120000;
constexpr int kMinBandwithChangeBps =
    (kMaxUplinkBandwidthBps - kMinUplinkBandwidthBps) / kFactor;

constexpr int64_t kClockInitialTime = 123456789;

struct ControllerManagerStates {
  std::unique_ptr<ControllerManager> controller_manager;
  std::vector<MockController*> mock_controllers;
  std::unique_ptr<SimulatedClock> simulated_clock;
};

ControllerManagerStates CreateControllerManager() {
  ControllerManagerStates states;
  std::vector<std::unique_ptr<Controller>> controllers;
  std::map<const Controller*, std::pair<int, float>> chracteristic_points;
  for (size_t i = 0; i < kNumControllers; ++i) {
    auto controller =
        std::unique_ptr<MockController>(new NiceMock<MockController>());
    EXPECT_CALL(*controller, Die());
    states.mock_controllers.push_back(controller.get());
    controllers.push_back(std::move(controller));
  }

  // Assign characteristic points to the last two controllers.
  chracteristic_points[states.mock_controllers[kNumControllers - 2]] =
      std::make_pair(kChracteristicBandwithBps[0],
                     kChracteristicPacketLossFraction[0]);
  chracteristic_points[states.mock_controllers[kNumControllers - 1]] =
      std::make_pair(kChracteristicBandwithBps[1],
                     kChracteristicPacketLossFraction[1]);

  states.simulated_clock.reset(new SimulatedClock(kClockInitialTime));
  states.controller_manager.reset(new ControllerManagerImpl(
      ControllerManagerImpl::Config(kMinReorderingTimeMs,
                                    kMinReorderingSquareDistance,
                                    states.simulated_clock.get()),
      std::move(controllers), chracteristic_points));
  return states;
}

// |expected_order| contains the expected indices of all controllers in the
// vector of controllers returned by GetSortedControllers(). A negative index
// means that we do not care about its exact place, but we do check that it
// exists in the vector.
void CheckControllersOrder(
    ControllerManagerStates* states,
    const rtc::Optional<int>& uplink_bandwidth_bps,
    const rtc::Optional<float>& uplink_packet_loss_fraction,
    const std::vector<int>& expected_order) {
  RTC_DCHECK_EQ(kNumControllers, expected_order.size());
  Controller::NetworkMetrics metrics;
  metrics.uplink_bandwidth_bps = uplink_bandwidth_bps;
  metrics.uplink_packet_loss_fraction = uplink_packet_loss_fraction;
  auto check = states->controller_manager->GetSortedControllers(metrics);
  EXPECT_EQ(states->mock_controllers.size(), check.size());
  for (size_t i = 0; i < states->mock_controllers.size(); ++i) {
    if (expected_order[i] >= 0) {
      EXPECT_EQ(states->mock_controllers[i], check[expected_order[i]]);
    } else {
      EXPECT_NE(check.end(), std::find(check.begin(), check.end(),
                                       states->mock_controllers[i]));
    }
  }
}

}  // namespace

TEST(ControllerManagerTest, GetControllersReturnAllControllers) {
  auto states = CreateControllerManager();
  auto check = states.controller_manager->GetControllers();
  // Verify that controllers in |check| are one-to-one mapped to those in
  // |mock_controllers_|.
  EXPECT_EQ(states.mock_controllers.size(), check.size());
  for (auto& controller : check)
    EXPECT_NE(states.mock_controllers.end(),
              std::find(states.mock_controllers.begin(),
                        states.mock_controllers.end(), controller));
}

TEST(ControllerManagerTest, ControllersInDefaultOrderOnEmptyNetworkMetrics) {
  auto states = CreateControllerManager();
  // |network_metrics| are empty, and the controllers are supposed to follow the
  // default order.
  CheckControllersOrder(&states, rtc::Optional<int>(), rtc::Optional<float>(),
                        {0, 1, 2, 3});
}

TEST(ControllerManagerTest, ControllersWithoutCharPointAtEndAndInDefaultOrder) {
  auto states = CreateControllerManager();
  CheckControllersOrder(&states, rtc::Optional<int>(0),
                        rtc::Optional<float>(0.0),
                        {kNumControllers - 2, kNumControllers - 1, -1, -1});
}

TEST(ControllerManagerTest, ControllersWithCharPointDependOnNetworkMetrics) {
  auto states = CreateControllerManager();
  CheckControllersOrder(
      &states, rtc::Optional<int>(kChracteristicBandwithBps[1]),
      rtc::Optional<float>(kChracteristicPacketLossFraction[1]),
      {kNumControllers - 2, kNumControllers - 1, 1, 0});
}

TEST(ControllerManagerTest, DoNotReorderBeforeMinReordingTime) {
  auto states = CreateControllerManager();
  CheckControllersOrder(
      &states, rtc::Optional<int>(kChracteristicBandwithBps[0]),
      rtc::Optional<float>(kChracteristicPacketLossFraction[0]),
      {kNumControllers - 2, kNumControllers - 1, 0, 1});
  states.simulated_clock->AdvanceTimeMilliseconds(kMinReorderingTimeMs - 1);
  // Move uplink bandwidth and packet loss fraction to the other controller's
  // characteristic point, which would cause controller manager to reorder the
  // controllers if time had reached min reordering time.
  CheckControllersOrder(
      &states, rtc::Optional<int>(kChracteristicBandwithBps[1]),
      rtc::Optional<float>(kChracteristicPacketLossFraction[1]),
      {kNumControllers - 2, kNumControllers - 1, 0, 1});
}

TEST(ControllerManagerTest, ReorderBeyondMinReordingTimeAndMinDistance) {
  auto states = CreateControllerManager();
  constexpr int kBandwidthBps =
      (kChracteristicBandwithBps[0] + kChracteristicBandwithBps[1]) / 2;
  constexpr float kPacketLossFraction = (kChracteristicPacketLossFraction[0] +
                                         kChracteristicPacketLossFraction[1]) /
                                        2.0f;
  // Set network metrics to be in the middle between the characteristic points
  // of two controllers.
  CheckControllersOrder(&states, rtc::Optional<int>(kBandwidthBps),
                        rtc::Optional<float>(kPacketLossFraction),
                        {kNumControllers - 2, kNumControllers - 1, 0, 1});
  states.simulated_clock->AdvanceTimeMilliseconds(kMinReorderingTimeMs);
  // Then let network metrics move a little towards the other controller.
  CheckControllersOrder(
      &states, rtc::Optional<int>(kBandwidthBps - kMinBandwithChangeBps - 1),
      rtc::Optional<float>(kPacketLossFraction),
      {kNumControllers - 2, kNumControllers - 1, 1, 0});
}

TEST(ControllerManagerTest, DoNotReorderIfNetworkMetricsChangeTooSmall) {
  auto states = CreateControllerManager();
  constexpr int kBandwidthBps =
      (kChracteristicBandwithBps[0] + kChracteristicBandwithBps[1]) / 2;
  constexpr float kPacketLossFraction = (kChracteristicPacketLossFraction[0] +
                                         kChracteristicPacketLossFraction[1]) /
                                        2.0f;
  // Set network metrics to be in the middle between the characteristic points
  // of two controllers.
  CheckControllersOrder(&states, rtc::Optional<int>(kBandwidthBps),
                        rtc::Optional<float>(kPacketLossFraction),
                        {kNumControllers - 2, kNumControllers - 1, 0, 1});
  states.simulated_clock->AdvanceTimeMilliseconds(kMinReorderingTimeMs);
  // Then let network metrics move a little towards the other controller.
  CheckControllersOrder(
      &states, rtc::Optional<int>(kBandwidthBps - kMinBandwithChangeBps + 1),
      rtc::Optional<float>(kPacketLossFraction),
      {kNumControllers - 2, kNumControllers - 1, 0, 1});
}

}  // namespace webrtc
