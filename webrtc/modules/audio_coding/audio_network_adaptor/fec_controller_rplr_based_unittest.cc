/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <random>
#include <utility>

#include "webrtc/modules/audio_coding/audio_network_adaptor/fec_controller_rplr_based.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

// The test uses the following settings:
//
// recoverable ^
// packet-loss |   |  |
//             |  A| C|   FEC
//             |    \  \   ON
//             | FEC \ D\_______
//             | OFF B\_________
//             |-----------------> bandwidth
//
// A : (kDisablingBandwidthLow, kDisablingRecoverablePacketLossAtLowBw)
// B : (kDisablingBandwidthHigh, kDisablingRecoverablePacketLossAtHighBw)
// C : (kEnablingBandwidthLow, kEnablingRecoverablePacketLossAtLowBw)
// D : (kEnablingBandwidthHigh, kEnablingRecoverablePacketLossAtHighBw)

constexpr int kDisablingBandwidthLow = 15000;
constexpr float kDisablingRecoverablePacketLossAtLowBw = 0.08f;
constexpr int kDisablingBandwidthHigh = 64000;
constexpr float kDisablingRecoverablePacketLossAtHighBw = 0.01f;
constexpr int kEnablingBandwidthLow = 17000;
constexpr float kEnablingRecoverablePacketLossAtLowBw = 0.1f;
constexpr int kEnablingBandwidthHigh = 64000;
constexpr float kEnablingRecoverablePacketLossAtHighBw = 0.05f;

rtc::Optional<float> GetRandomProbabilityOrUnknown() {
  std::random_device rd;
  std::mt19937 generator(rd());
  std::uniform_real_distribution<> distribution(0, 1);

  if (distribution(generator) < 0.2) {
    return rtc::Optional<float>();
  } else {
    return rtc::Optional<float>(distribution(generator));
  }
}

std::unique_ptr<FecControllerRplrBased> CreateFecControllerRplrBased(
    bool initial_fec_enabled) {
  using Threshold = FecControllerRplrBased::Config::Threshold;
  return std::unique_ptr<FecControllerRplrBased>(
      new FecControllerRplrBased(FecControllerRplrBased::Config(
          initial_fec_enabled,
          Threshold(
              kEnablingBandwidthLow, kEnablingRecoverablePacketLossAtLowBw,
              kEnablingBandwidthHigh, kEnablingRecoverablePacketLossAtHighBw),
          Threshold(kDisablingBandwidthLow,
                    kDisablingRecoverablePacketLossAtLowBw,
                    kDisablingBandwidthHigh,
                    kDisablingRecoverablePacketLossAtHighBw))));
}

void UpdateNetworkMetrics(
    FecControllerRplrBased* controller,
    const rtc::Optional<int>& uplink_bandwidth_bps,
    const rtc::Optional<float>& uplink_packet_loss,
    const rtc::Optional<float>& uplink_recoveralbe_packet_loss) {
  // UpdateNetworkMetrics can accept multiple network metric updates at once.
  // However, currently, the most used case is to update one metric at a time.
  // To reflect this fact, we separate the calls.
  if (uplink_bandwidth_bps) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_bandwidth_bps = uplink_bandwidth_bps;
    controller->UpdateNetworkMetrics(network_metrics);
  }
  if (uplink_packet_loss) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_packet_loss_fraction = uplink_packet_loss;
    controller->UpdateNetworkMetrics(network_metrics);
  }
  if (uplink_recoveralbe_packet_loss) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_recoverable_packet_loss_fraction =
        uplink_recoveralbe_packet_loss;
    controller->UpdateNetworkMetrics(network_metrics);
  }
}

void UpdateNetworkMetrics(
    FecControllerRplrBased* controller,
    const rtc::Optional<int>& uplink_bandwidth_bps,
    const rtc::Optional<float>& uplink_recoveralbe_packet_loss) {
  // FecControllerRplrBased doesn't current use the PLR (general packet-loss
  // rate) at all. (This might be changed in the future.) The unit-tests will
  // use a random value (including unknown), to show this does not interfere.
  UpdateNetworkMetrics(controller, uplink_bandwidth_bps,
                       GetRandomProbabilityOrUnknown(),
                       uplink_recoveralbe_packet_loss);
}

// Checks that the FEC decision and |uplink_packet_loss_fraction| given by
// |states->controller->MakeDecision| matches |expected_enable_fec| and
// |expected_uplink_packet_loss_fraction|, respectively.
void CheckDecision(FecControllerRplrBased* controller,
                   bool expected_enable_fec,
                   float expected_uplink_packet_loss_fraction) {
  AudioNetworkAdaptor::EncoderRuntimeConfig config;
  controller->MakeDecision(&config);

  // Less compact than comparing optionals, but yields more readable errors.
  EXPECT_TRUE(config.enable_fec);
  if (config.enable_fec) {
    EXPECT_EQ(expected_enable_fec, *config.enable_fec);
  }
  EXPECT_TRUE(config.uplink_packet_loss_fraction);
  if (config.uplink_packet_loss_fraction) {
    EXPECT_EQ(expected_uplink_packet_loss_fraction,
              *config.uplink_packet_loss_fraction);
  }
}

}  // namespace

TEST(FecControllerRplrBasedTest, OutputInitValueWhenUplinkBandwidthUnknown) {
  for (bool initial_fec_enabled : {false, true}) {
    auto controller = CreateFecControllerRplrBased(initial_fec_enabled);
    // Let uplink recoverable packet loss fraction be so low that it
    // would cause FEC to turn off if uplink bandwidth was known.
    UpdateNetworkMetrics(
        controller.get(), rtc::Optional<int>(),
        rtc::Optional<float>(kDisablingRecoverablePacketLossAtHighBw));
    CheckDecision(controller.get(), initial_fec_enabled,
                  kDisablingRecoverablePacketLossAtHighBw);
  }
}

TEST(FecControllerRplrBasedTest,
     OutputInitValueWhenUplinkPacketLossFractionUnknown) {
  for (bool initial_fec_enabled : {false, true}) {
    auto controller = CreateFecControllerRplrBased(initial_fec_enabled);
    // Let uplink bandwidth be so low that it would cause FEC to turn off
    // if uplink bandwidth packet loss fraction was known.
    UpdateNetworkMetrics(controller.get(),
                         rtc::Optional<int>(kDisablingBandwidthLow - 1),
                         rtc::Optional<float>());
    CheckDecision(controller.get(), initial_fec_enabled, 0.0);
  }
}

TEST(FecControllerRplrBasedTest, EnableFecForHighBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  UpdateNetworkMetrics(
      controller.get(), rtc::Optional<int>(kEnablingBandwidthHigh),
      rtc::Optional<float>(kEnablingRecoverablePacketLossAtHighBw));
  CheckDecision(controller.get(), true, kEnablingRecoverablePacketLossAtHighBw);
}

TEST(FecControllerRplrBasedTest, UpdateMultipleNetworkMetricsAtOnce) {
  // This test is similar to EnableFecForHighBandwidth. But instead of
  // using ::UpdateNetworkMetrics(...), which calls
  // FecControllerRplrBasedTest::UpdateNetworkMetrics(...) multiple times, we
  // we call it only once. This is to verify that
  // FecControllerRplrBasedTest::UpdateNetworkMetrics(...) can handle multiple
  // network updates at once. This is, however, not a common use case in current
  // audio_network_adaptor_impl.cc.
  auto controller = CreateFecControllerRplrBased(false);
  Controller::NetworkMetrics network_metrics;
  network_metrics.uplink_bandwidth_bps =
      rtc::Optional<int>(kEnablingBandwidthHigh);
  network_metrics.uplink_packet_loss_fraction =
      rtc::Optional<float>(GetRandomProbabilityOrUnknown());
  network_metrics.uplink_recoverable_packet_loss_fraction =
      rtc::Optional<float>(kEnablingRecoverablePacketLossAtHighBw);
  controller->UpdateNetworkMetrics(network_metrics);
  CheckDecision(controller.get(), true, kEnablingRecoverablePacketLossAtHighBw);
}

TEST(FecControllerRplrBasedTest, MaintainFecOffForHighBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  constexpr float kPacketLoss = kEnablingRecoverablePacketLossAtHighBw * 0.99f;
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kEnablingBandwidthHigh),
                       rtc::Optional<float>(kPacketLoss));
  CheckDecision(controller.get(), false, kPacketLoss);
}

TEST(FecControllerRplrBasedTest, EnableFecForMediumBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  constexpr float kPacketLoss = (kEnablingRecoverablePacketLossAtLowBw +
                                 kEnablingRecoverablePacketLossAtHighBw) /
                                2.0;
  UpdateNetworkMetrics(
      controller.get(),
      rtc::Optional<int>((kEnablingBandwidthHigh + kEnablingBandwidthLow) / 2),
      rtc::Optional<float>(kPacketLoss));
  CheckDecision(controller.get(), true, kPacketLoss);
}

TEST(FecControllerRplrBasedTest, MaintainFecOffForMediumBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  constexpr float kPacketLoss = kEnablingRecoverablePacketLossAtLowBw * 0.49f +
                                kEnablingRecoverablePacketLossAtHighBw * 0.51f;
  UpdateNetworkMetrics(
      controller.get(),
      rtc::Optional<int>((kEnablingBandwidthHigh + kEnablingBandwidthLow) / 2),
      rtc::Optional<float>(kPacketLoss));
  CheckDecision(controller.get(), false, kPacketLoss);
}

TEST(FecControllerRplrBasedTest, EnableFecForLowBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  UpdateNetworkMetrics(
      controller.get(), rtc::Optional<int>(kEnablingBandwidthLow),
      rtc::Optional<float>(kEnablingRecoverablePacketLossAtLowBw));
  CheckDecision(controller.get(), true, kEnablingRecoverablePacketLossAtLowBw);
}

TEST(FecControllerRplrBasedTest, MaintainFecOffForLowBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  constexpr float kPacketLoss = kEnablingRecoverablePacketLossAtLowBw * 0.99f;
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kEnablingBandwidthLow),
                       rtc::Optional<float>(kPacketLoss));
  CheckDecision(controller.get(), false, kPacketLoss);
}

TEST(FecControllerRplrBasedTest, MaintainFecOffForVeryLowBandwidth) {
  auto controller = CreateFecControllerRplrBased(false);
  // Below |kEnablingBandwidthLow|, no recoverable packet loss fraction can
  // cause FEC to turn on.
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kEnablingBandwidthLow - 1),
                       rtc::Optional<float>(1.0));
  CheckDecision(controller.get(), false, 1.0);
}

TEST(FecControllerRplrBasedTest, DisableFecForHighBandwidth) {
  auto controller = CreateFecControllerRplrBased(true);
  UpdateNetworkMetrics(
      controller.get(), rtc::Optional<int>(kDisablingBandwidthHigh),
      rtc::Optional<float>(kDisablingRecoverablePacketLossAtHighBw));
  CheckDecision(controller.get(), false,
                kDisablingRecoverablePacketLossAtHighBw);
}

TEST(FecControllerRplrBasedTest, MaintainFecOnForHighBandwidth) {
  auto controller = CreateFecControllerRplrBased(true);
  constexpr float kPacketLoss = kDisablingRecoverablePacketLossAtHighBw * 1.01f;
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kDisablingBandwidthHigh),
                       rtc::Optional<float>(kPacketLoss));
  CheckDecision(controller.get(), true, kPacketLoss);
}

TEST(FecControllerRplrBasedTest, DisableFecOnMediumBandwidth) {
  auto controller = CreateFecControllerRplrBased(true);
  constexpr float kPacketLoss = (kDisablingRecoverablePacketLossAtLowBw +
                                 kDisablingRecoverablePacketLossAtHighBw) /
                                2.0f;
  UpdateNetworkMetrics(
      controller.get(),
      rtc::Optional<int>((kDisablingBandwidthHigh + kDisablingBandwidthLow) /
                         2),
      rtc::Optional<float>(kPacketLoss));
  CheckDecision(controller.get(), false, kPacketLoss);
}

TEST(FecControllerRplrBasedTest, MaintainFecOnForMediumBandwidth) {
  auto controller = CreateFecControllerRplrBased(true);
  constexpr float kPacketLoss = kDisablingRecoverablePacketLossAtLowBw * 0.51f +
                                kDisablingRecoverablePacketLossAtHighBw * 0.49f;
  UpdateNetworkMetrics(
      controller.get(),
      rtc::Optional<int>((kEnablingBandwidthHigh + kDisablingBandwidthLow) / 2),
      rtc::Optional<float>(kPacketLoss));
  CheckDecision(controller.get(), true, kPacketLoss);
}

TEST(FecControllerRplrBasedTest, DisableFecForLowBandwidth) {
  auto controller = CreateFecControllerRplrBased(true);
  UpdateNetworkMetrics(
      controller.get(), rtc::Optional<int>(kDisablingBandwidthLow),
      rtc::Optional<float>(kDisablingRecoverablePacketLossAtLowBw));
  CheckDecision(controller.get(), false,
                kDisablingRecoverablePacketLossAtLowBw);
}

TEST(FecControllerRplrBasedTest, DisableFecForVeryLowBandwidth) {
  auto controller = CreateFecControllerRplrBased(true);
  // Below |kEnablingBandwidthLow|, any recoverable packet loss fraction can
  // cause FEC to turn off.
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kDisablingBandwidthLow - 1),
                       rtc::Optional<float>(1.0));
  CheckDecision(controller.get(), false, 1.0);
}

TEST(FecControllerRplrBasedTest, CheckBehaviorOnChangingNetworkMetrics) {
  // In this test, we let the network metrics to traverse from 1 to 5.
  //
  // recoverable ^
  // packet-loss | 1 |  |
  //             |   | 2|
  //             |    \  \ 3
  //             |     \4 \_______
  //             |      \_________
  //             |---------5-------> bandwidth

  auto controller = CreateFecControllerRplrBased(true);
  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kDisablingBandwidthLow - 1),
                       rtc::Optional<float>(1.0));
  CheckDecision(controller.get(), false, 1.0);

  UpdateNetworkMetrics(
      controller.get(), rtc::Optional<int>(kEnablingBandwidthLow),
      rtc::Optional<float>(kEnablingRecoverablePacketLossAtLowBw * 0.99f));
  CheckDecision(controller.get(), false,
                kEnablingRecoverablePacketLossAtLowBw * 0.99f);

  UpdateNetworkMetrics(
      controller.get(), rtc::Optional<int>(kEnablingBandwidthHigh),
      rtc::Optional<float>(kEnablingRecoverablePacketLossAtHighBw));
  CheckDecision(controller.get(), true, kEnablingRecoverablePacketLossAtHighBw);

  UpdateNetworkMetrics(
      controller.get(), rtc::Optional<int>(kDisablingBandwidthHigh),
      rtc::Optional<float>(kDisablingRecoverablePacketLossAtHighBw * 1.01f));
  CheckDecision(controller.get(), true,
                kDisablingRecoverablePacketLossAtHighBw * 1.01f);

  UpdateNetworkMetrics(controller.get(),
                       rtc::Optional<int>(kDisablingBandwidthHigh + 1),
                       rtc::Optional<float>(0.0));
  CheckDecision(controller.get(), false, 0.0);
}

TEST(FecControllerRplrBasedTest, CheckBehaviorOnSpecialCurves) {
  // We test a special configuration, where the points to define the FEC
  // enabling/disabling curves are placed like the following, otherwise the test
  // is the same as CheckBehaviorOnChangingNetworkMetrics.
  //
  // recoverable ^
  // packet-loss |   |  |
  //             |   | C|
  //             |   |  |
  //             |   | D|_______
  //             |  A|___B______
  //             |-----------------> bandwidth

  constexpr int kEnablingBandwidthHigh = kEnablingBandwidthLow;
  constexpr float kDisablingRecoverablePacketLossAtLowBw =
      kDisablingRecoverablePacketLossAtHighBw;
  using Threshold = FecControllerRplrBased::Config::Threshold;
  FecControllerRplrBased controller(FecControllerRplrBased::Config(
      true,
      Threshold(kEnablingBandwidthLow, kEnablingRecoverablePacketLossAtLowBw,
                kEnablingBandwidthHigh, kEnablingRecoverablePacketLossAtHighBw),
      Threshold(kDisablingBandwidthLow, kDisablingRecoverablePacketLossAtLowBw,
                kDisablingBandwidthHigh,
                kDisablingRecoverablePacketLossAtHighBw)));

  UpdateNetworkMetrics(&controller,
                       rtc::Optional<int>(kDisablingBandwidthLow - 1),
                       rtc::Optional<float>(1.0));
  CheckDecision(&controller, false, 1.0);

  UpdateNetworkMetrics(
      &controller, rtc::Optional<int>(kEnablingBandwidthLow),
      rtc::Optional<float>(kEnablingRecoverablePacketLossAtHighBw * 0.99f));
  CheckDecision(&controller, false,
                kEnablingRecoverablePacketLossAtHighBw * 0.99f);

  UpdateNetworkMetrics(
      &controller, rtc::Optional<int>(kEnablingBandwidthHigh),
      rtc::Optional<float>(kEnablingRecoverablePacketLossAtHighBw));
  CheckDecision(&controller, true, kEnablingRecoverablePacketLossAtHighBw);

  UpdateNetworkMetrics(
      &controller, rtc::Optional<int>(kDisablingBandwidthHigh),
      rtc::Optional<float>(kDisablingRecoverablePacketLossAtHighBw * 1.01f));
  CheckDecision(&controller, true,
                kDisablingRecoverablePacketLossAtHighBw * 1.01f);

  UpdateNetworkMetrics(&controller,
                       rtc::Optional<int>(kDisablingBandwidthHigh + 1),
                       rtc::Optional<float>(0.0));
  CheckDecision(&controller, false, 0.0);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(FecControllerRplrBasedDeathTest, InvalidConfig) {
  using Threshold = FecControllerRplrBased::Config::Threshold;
  EXPECT_DEATH(
      FecControllerRplrBased controller(FecControllerRplrBased::Config(
          true,
          Threshold(
              kDisablingBandwidthLow - 1, kEnablingRecoverablePacketLossAtLowBw,
              kEnablingBandwidthHigh, kEnablingRecoverablePacketLossAtHighBw),
          Threshold(kDisablingBandwidthLow,
                    kDisablingRecoverablePacketLossAtLowBw,
                    kDisablingBandwidthHigh,
                    kDisablingRecoverablePacketLossAtHighBw))),
      "Check failed");
}
#endif

}  // namespace webrtc
