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
#include <utility>

#include "webrtc/modules/audio_coding/audio_network_adaptor/frame_length_controller.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

constexpr float kFlIncreasingPacketLossFraction = 0.04f;
constexpr float kFlDecreasingPacketLossFraction = 0.05f;
constexpr int kFl20msTo60msBandwidthBps = 22000;
constexpr int kFl60msTo20msBandwidthBps = 88000;
constexpr int kMediumBandwidthBps =
    (kFl60msTo20msBandwidthBps + kFl20msTo60msBandwidthBps) / 2;
constexpr float kMediumPacketLossFraction =
    (kFlDecreasingPacketLossFraction + kFlIncreasingPacketLossFraction) / 2;

std::unique_ptr<FrameLengthController> CreateController(
    const std::vector<int>& encoder_frame_lengths_ms,
    int initial_frame_length_ms) {
  std::unique_ptr<FrameLengthController> controller(
      new FrameLengthController(FrameLengthController::Config(
          encoder_frame_lengths_ms, initial_frame_length_ms,
          kFlIncreasingPacketLossFraction, kFlDecreasingPacketLossFraction,
          kFl20msTo60msBandwidthBps, kFl60msTo20msBandwidthBps)));
  return controller;
}

void UpdateNetworkMetrics(
    FrameLengthController* controller,
    const rtc::Optional<int>& uplink_bandwidth_bps,
    const rtc::Optional<float>& uplink_packet_loss_fraction) {
  // UpdateNetworkMetrics can accept multiple network metric updates at once.
  // However, currently, the most used case is to update one metric at a time.
  // To reflect this fact, we separate the calls.
  if (uplink_bandwidth_bps) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_bandwidth_bps = uplink_bandwidth_bps;
    controller->UpdateNetworkMetrics(network_metrics);
  }
  if (uplink_packet_loss_fraction) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_packet_loss_fraction = uplink_packet_loss_fraction;
    controller->UpdateNetworkMetrics(network_metrics);
  }
}

void CheckDecision(FrameLengthController* controller,
                   const rtc::Optional<bool>& enable_fec,
                   int expected_frame_length_ms) {
  AudioNetworkAdaptor::EncoderRuntimeConfig config;
  config.enable_fec = enable_fec;
  controller->MakeDecision(&config);
  EXPECT_EQ(rtc::Optional<int>(expected_frame_length_ms),
            config.frame_length_ms);
}

}  // namespace

TEST(FrameLengthControllerTest, DecreaseTo20MsOnHighUplinkBandwidth) {
  auto controller = CreateController({20, 60}, 60);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo20msBandwidthBps),
                       rtc::Optional<float>());
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, DecreaseTo20MsOnHighUplinkPacketLossFraction) {
  auto controller = CreateController({20, 60}, 60);
  UpdateNetworkMetrics(controller.get(), rtc::Optional<int>(),
                       rtc::Optional<float>(kFlDecreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, DecreaseTo20MsWhenFecIsOn) {
  auto controller = CreateController({20, 60}, 60);
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 20);
}

TEST(FrameLengthControllerTest,
     Maintain60MsIf20MsNotInReceiverFrameLengthRange) {
  auto controller = CreateController({60}, 60);
  // Set FEC on that would cause frame length to decrease if receiver frame
  // length range included 20ms.
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 60);
}

TEST(FrameLengthControllerTest, Maintain60MsOnMultipleConditions) {
  // Maintain 60ms frame length if
  // 1. |uplink_bandwidth_bps| is at medium level,
  // 2. |uplink_packet_loss_fraction| is at medium,
  // 3. FEC is not decided ON.
  auto controller = CreateController({20, 60}, 60);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kMediumBandwidthBps),
                       rtc::Optional<float>(kMediumPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);
}

TEST(FrameLengthControllerTest, IncreaseTo60MsOnMultipleConditions) {
  // Increase to 60ms frame length if
  // 1. |uplink_bandwidth_bps| is known to be smaller than a threshold AND
  // 2. |uplink_packet_loss_fraction| is known to be smaller than a threshold
  //    AND
  // 3. FEC is not decided or OFF.
  auto controller = CreateController({20, 60}, 20);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl20msTo60msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);
}

TEST(FrameLengthControllerTest, UpdateMultipleNetworkMetricsAtOnce) {
  // This test is similar to IncreaseTo60MsOnMultipleConditions. But instead of
  // using ::UpdateNetworkMetrics(...), which calls
  // FrameLengthController::UpdateNetworkMetrics(...) multiple times, we
  // we call it only once. This is to verify that
  // FrameLengthController::UpdateNetworkMetrics(...) can handle multiple
  // network updates at once. This is, however, not a common use case in current
  // audio_network_adaptor_impl.cc.
  auto controller = CreateController({20, 60}, 20);
  Controller::NetworkMetrics network_metrics;
  network_metrics.uplink_bandwidth_bps =
      rtc::Optional<int>(kFl20msTo60msBandwidthBps);
  network_metrics.uplink_packet_loss_fraction =
      rtc::Optional<float>(kFlIncreasingPacketLossFraction);
  controller->UpdateNetworkMetrics(network_metrics);
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);
}

TEST(FrameLengthControllerTest,
     Maintain20MsIf60MsNotInReceiverFrameLengthRange) {
  auto controller = CreateController({20}, 20);
  // Use a low uplink bandwidth and a low uplink packet loss fraction that would
  // cause frame length to increase if receiver frame length included 60ms.
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl20msTo60msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, Maintain20MsOnMediumUplinkBandwidth) {
  auto controller = CreateController({20, 60}, 20);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kMediumBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, Maintain20MsOnMediumUplinkPacketLossFraction) {
  auto controller = CreateController({20, 60}, 20);
  // Use a low uplink bandwidth that would cause frame length to increase if
  // uplink packet loss fraction was low.
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl20msTo60msBandwidthBps),
                       rtc::Optional<float>(kMediumPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, Maintain20MsWhenFecIsOn) {
  auto controller = CreateController({20, 60}, 20);
  // Use a low uplink bandwidth and a low uplink packet loss fraction that would
  // cause frame length to increase if FEC was not ON.
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl20msTo60msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 20);
}

namespace {
constexpr int kFl60msTo120msBandwidthBps = 18000;
constexpr int kFl120msTo60msBandwidthBps = 72000;
}

class FrameLengthControllerForTest {
 public:
  // This class is to test multiple frame lengths. FrameLengthController is
  // implemented to support this, but is not enabled for the default constructor
  // for the time being. We use this class to test it.
  FrameLengthControllerForTest(const std::vector<int>& encoder_frame_lengths_ms,
                               int initial_frame_length_ms)
      : frame_length_controller_(
            FrameLengthController::Config(encoder_frame_lengths_ms,
                                          initial_frame_length_ms,
                                          kFlIncreasingPacketLossFraction,
                                          kFlDecreasingPacketLossFraction,
                                          kFl20msTo60msBandwidthBps,
                                          kFl60msTo20msBandwidthBps)) {
    frame_length_controller_.frame_length_change_criteria_.insert(
        std::make_pair(FrameLengthController::FrameLengthChange(60, 120),
                       kFl60msTo120msBandwidthBps));
    frame_length_controller_.frame_length_change_criteria_.insert(
        std::make_pair(FrameLengthController::FrameLengthChange(120, 60),
                       kFl120msTo60msBandwidthBps));
  }
  FrameLengthController* get() { return &frame_length_controller_; }

 private:
  FrameLengthController frame_length_controller_;
};

TEST(FrameLengthControllerTest, From120MsTo20MsOnHighUplinkBandwidth) {
  FrameLengthControllerForTest controller({20, 60, 120}, 120);
  // It takes two steps for frame length to go from 120ms to 20ms.
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo20msBandwidthBps),
                       rtc::Optional<float>());
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);

  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo20msBandwidthBps),
                       rtc::Optional<float>());
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, From120MsTo20MsOnHighUplinkPacketLossFraction) {
  FrameLengthControllerForTest controller({20, 60, 120}, 120);
  // It takes two steps for frame length to go from 120ms to 20ms.
  UpdateNetworkMetrics(controller.get(), rtc::Optional<int>(),
                       rtc::Optional<float>(kFlDecreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);

  UpdateNetworkMetrics(controller.get(), rtc::Optional<int>(),
                       rtc::Optional<float>(kFlDecreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

TEST(FrameLengthControllerTest, From120MsTo20MsWhenFecIsOn) {
  FrameLengthControllerForTest controller({20, 60, 120}, 120);
  // It takes two steps for frame length to go from 120ms to 20ms.
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 60);
  CheckDecision(controller.get(), rtc::Optional<bool>(true), 20);
}

TEST(FrameLengthControllerTest, From20MsTo120MsOnMultipleConditions) {
  // Increase to 120ms frame length if
  // 1. |uplink_bandwidth_bps| is known to be smaller than a threshold AND
  // 2. |uplink_packet_loss_fraction| is known to be smaller than a threshold
  //    AND
  // 3. FEC is not decided or OFF.
  FrameLengthControllerForTest controller({20, 60, 120}, 20);
  // It takes two steps for frame length to go from 20ms to 120ms.
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo120msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo120msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 120);
}

TEST(FrameLengthControllerTest, Stall60MsIf120MsNotInReceiverFrameLengthRange) {
  FrameLengthControllerForTest controller({20, 60}, 20);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo120msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo120msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);
}

TEST(FrameLengthControllerTest, CheckBehaviorOnChangingNetworkMetrics) {
  FrameLengthControllerForTest controller({20, 60, 120}, 20);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kMediumBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);

  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl20msTo60msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);

  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo120msBandwidthBps),
                       rtc::Optional<float>(kMediumPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);

  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl60msTo120msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 120);

  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kFl120msTo60msBandwidthBps),
                       rtc::Optional<float>(kFlIncreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 60);

  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kMediumPacketLossFraction),
                       rtc::Optional<float>(kFlDecreasingPacketLossFraction));
  CheckDecision(controller.get(), rtc::Optional<bool>(), 20);
}

}  // namespace webrtc
