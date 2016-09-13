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
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/audio_network_adaptor_impl.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/mock/mock_controller.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/mock/mock_controller_manager.h"

namespace webrtc {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

constexpr size_t kNumControllers = 2;

MATCHER_P(NetworkMetricsIs, metric, "") {
  return arg.uplink_bandwidth_bps == metric.uplink_bandwidth_bps &&
         arg.uplink_packet_loss_fraction == metric.uplink_packet_loss_fraction;
}

MATCHER_P(ConstraintsReceiverFrameLengthRangeIs, frame_length_range, "") {
  return arg.receiver_frame_length_range->min_frame_length_ms ==
             frame_length_range.min_frame_length_ms &&
         arg.receiver_frame_length_range->max_frame_length_ms ==
             frame_length_range.max_frame_length_ms;
}

struct AudioNetworkAdaptorStates {
  std::unique_ptr<AudioNetworkAdaptorImpl> audio_network_adaptor;
  std::vector<std::unique_ptr<MockController>> mock_controllers;
};

AudioNetworkAdaptorStates CreateAudioNetworkAdaptor() {
  AudioNetworkAdaptorStates states;
  std::vector<Controller*> controllers;
  for (size_t i = 0; i < kNumControllers; ++i) {
    auto controller =
        std::unique_ptr<MockController>(new NiceMock<MockController>());
    EXPECT_CALL(*controller, Die());
    controllers.push_back(controller.get());
    states.mock_controllers.push_back(std::move(controller));
  }

  auto controller_manager = std::unique_ptr<MockControllerManager>(
      new NiceMock<MockControllerManager>());

  EXPECT_CALL(*controller_manager, Die());
  EXPECT_CALL(*controller_manager, GetControllers())
      .WillRepeatedly(Return(controllers));
  EXPECT_CALL(*controller_manager, GetSortedControllers(_))
      .WillRepeatedly(Return(controllers));

  // AudioNetworkAdaptorImpl governs the lifetime of controller manager.
  states.audio_network_adaptor.reset(new AudioNetworkAdaptorImpl(
      AudioNetworkAdaptorImpl::Config(), std::move(controller_manager)));

  return states;
}

}  // namespace

TEST(AudioNetworkAdaptorImplTest,
     MakeDecisionIsCalledOnGetEncoderRuntimeConfig) {
  auto states = CreateAudioNetworkAdaptor();

  constexpr int kBandwidth = 16000;
  constexpr float kPacketLoss = 0.7f;

  Controller::NetworkMetrics check;
  check.uplink_bandwidth_bps = rtc::Optional<int>(kBandwidth);

  for (auto& mock_controller : states.mock_controllers) {
    EXPECT_CALL(*mock_controller, MakeDecision(NetworkMetricsIs(check), _));
  }
  states.audio_network_adaptor->SetUplinkBandwidth(kBandwidth);
  states.audio_network_adaptor->GetEncoderRuntimeConfig();

  check.uplink_packet_loss_fraction = rtc::Optional<float>(kPacketLoss);
  for (auto& mock_controller : states.mock_controllers) {
    EXPECT_CALL(*mock_controller, MakeDecision(NetworkMetricsIs(check), _));
  }
  states.audio_network_adaptor->SetUplinkPacketLossFraction(kPacketLoss);
  states.audio_network_adaptor->GetEncoderRuntimeConfig();
}

TEST(AudioNetworkAdaptorImplTest, SetConstraintsIsCalledOnSetFrameLengthRange) {
  auto states = CreateAudioNetworkAdaptor();

  for (auto& mock_controller : states.mock_controllers) {
    EXPECT_CALL(*mock_controller,
                SetConstraints(ConstraintsReceiverFrameLengthRangeIs(
                    Controller::Constraints::FrameLengthRange(20, 120))));
  }
  states.audio_network_adaptor->SetReceiverFrameLengthRange(20, 120);
}

}  // namespace webrtc
