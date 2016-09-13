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

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/controller_manager.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/mock/mock_controller.h"

namespace webrtc {

using ::testing::NiceMock;

namespace {

constexpr size_t kNumControllers = 3;

struct ControllerManagerStates {
  std::unique_ptr<ControllerManager> controller_manager;
  std::vector<MockController*> mock_controllers;
};

ControllerManagerStates CreateControllerManager() {
  ControllerManagerStates states;
  std::vector<std::unique_ptr<Controller>> controllers;
  for (size_t i = 0; i < kNumControllers; ++i) {
    auto controller =
        std::unique_ptr<MockController>(new NiceMock<MockController>());
    EXPECT_CALL(*controller, Die());
    states.mock_controllers.push_back(controller.get());
    controllers.push_back(std::move(controller));
  }
  states.controller_manager.reset(new ControllerManagerImpl(
      ControllerManagerImpl::Config(), std::move(controllers)));
  return states;
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
  Controller::NetworkMetrics network_metrics;
  auto check = states.controller_manager->GetSortedControllers(network_metrics);
  EXPECT_EQ(states.mock_controllers.size(), check.size());
  for (size_t i = 0; i < states.mock_controllers.size(); ++i)
    EXPECT_EQ(states.mock_controllers[i], check[i]);
}

}  // namespace webrtc
