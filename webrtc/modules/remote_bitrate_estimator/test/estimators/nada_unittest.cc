/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/nada.h"

#include <algorithm>
#include <numeric>

#include "webrtc/base/common.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_sender.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace testing {
namespace bwe {

class FilterTest : public ::testing::Test {
 public:
  void MedianFilterConstantArray() {
    std::fill_n(raw_signal_, kNumElements, kSignalValue);
    for (int i = 0; i < kNumElements; ++i) {
      int size = std::min(5, i + 1);
      median_filtered_[i] =
          NadaBweReceiver::MedianFilter(&raw_signal_[i + 1 - size], size);
    }
  }

  void MedianFilterIntermittentNoise() {
    const int kValue = 500;
    const int kNoise = 100;

    for (int i = 0; i < kNumElements; ++i) {
      raw_signal_[i] = kValue + kNoise * (i % 10 == 9 ? 1 : 0);
    }
    for (int i = 0; i < kNumElements; ++i) {
      int size = std::min(5, i + 1);
      median_filtered_[i] =
          NadaBweReceiver::MedianFilter(&raw_signal_[i + 1 - size], size);
      EXPECT_EQ(median_filtered_[i], kValue);
    }
  }

  void ExponentialSmoothingFilter(const int64_t raw_signal_[],
                                  int num_elements,
                                  int64_t exp_smoothed[]) {
    exp_smoothed[0] =
        NadaBweReceiver::ExponentialSmoothingFilter(raw_signal_[0], -1, kAlpha);
    for (int i = 1; i < num_elements; ++i) {
      exp_smoothed[i] = NadaBweReceiver::ExponentialSmoothingFilter(
          raw_signal_[i], exp_smoothed[i - 1], kAlpha);
    }
  }

  void ExponentialSmoothingConstantArray(int64_t exp_smoothed[]) {
    std::fill_n(raw_signal_, kNumElements, kSignalValue);
    ExponentialSmoothingFilter(raw_signal_, kNumElements, exp_smoothed);
  }

 protected:
  static const int kNumElements = 1000;
  static const int64_t kSignalValue;
  static const float kAlpha;
  int64_t raw_signal_[kNumElements];
  int64_t median_filtered_[kNumElements];
};

const int64_t FilterTest::kSignalValue = 200;
const float FilterTest::kAlpha = 0.1f;

class TestBitrateObserver : public BitrateObserver {
 public:
  TestBitrateObserver()
      : last_bitrate_(0), last_fraction_loss_(0), last_rtt_(0) {}

  virtual void OnNetworkChanged(uint32_t bitrate,
                                uint8_t fraction_loss,
                                int64_t rtt) {
    last_bitrate_ = bitrate;
    last_fraction_loss_ = fraction_loss;
    last_rtt_ = rtt;
  }
  uint32_t last_bitrate_;
  uint8_t last_fraction_loss_;
  int64_t last_rtt_;
};

class NadaSenderSideTest : public ::testing::Test {
 public:
  NadaSenderSideTest()
      : observer_(),
        simulated_clock_(0),
        nada_sender_(&observer_, &simulated_clock_) {}
  ~NadaSenderSideTest() {}

 private:
  TestBitrateObserver observer_;
  SimulatedClock simulated_clock_;

 protected:
  NadaBweSender nada_sender_;
};

class NadaReceiverSideTest : public ::testing::Test {
 protected:
  NadaReceiverSideTest() : nada_receiver_(kFlowId) {}
  ~NadaReceiverSideTest() {}

  const int kFlowId = 0;
  NadaBweReceiver nada_receiver_;
};

class NadaFbGenerator {
 public:
  NadaFbGenerator();

  static NadaFeedback NotCongestedFb(size_t receiving_rate,
                                     int64_t ref_signal_ms,
                                     int64_t send_time_ms) {
    int64_t exp_smoothed_delay_ms = ref_signal_ms;
    int64_t est_queuing_delay_signal_ms = ref_signal_ms;
    int64_t congestion_signal_ms = ref_signal_ms;
    float derivative = 0.0f;
    return NadaFeedback(kFlowId, kNowMs, exp_smoothed_delay_ms,
                        est_queuing_delay_signal_ms, congestion_signal_ms,
                        derivative, receiving_rate, send_time_ms);
  }

  static NadaFeedback CongestedFb(size_t receiving_rate, int64_t send_time_ms) {
    int64_t exp_smoothed_delay_ms = 1000;
    int64_t est_queuing_delay_signal_ms = 800;
    int64_t congestion_signal_ms = 1000;
    float derivative = 1.0f;
    return NadaFeedback(kFlowId, kNowMs, exp_smoothed_delay_ms,
                        est_queuing_delay_signal_ms, congestion_signal_ms,
                        derivative, receiving_rate, send_time_ms);
  }

  static NadaFeedback ExtremelyCongestedFb(size_t receiving_rate,
                                           int64_t send_time_ms) {
    int64_t exp_smoothed_delay_ms = 100000;
    int64_t est_queuing_delay_signal_ms = 0;
    int64_t congestion_signal_ms = 100000;
    float derivative = 10000.0f;
    return NadaFeedback(kFlowId, kNowMs, exp_smoothed_delay_ms,
                        est_queuing_delay_signal_ms, congestion_signal_ms,
                        derivative, receiving_rate, send_time_ms);
  }

 private:
  // Arbitrary values, won't change these test results.
  static const int kFlowId = 2;
  static const int64_t kNowMs = 1000;
};

// Verify if AcceleratedRampUp is called and that bitrate increases.
TEST_F(NadaSenderSideTest, AcceleratedRampUp) {
  const int64_t kRefSignalMs = 3;
  const int64_t kOneWayDelayMs = 50;
  int original_bitrate = 2 * NadaBweSender::kMinRefRateKbps;
  size_t receiving_rate = static_cast<size_t>(original_bitrate);
  int64_t send_time_ms = nada_sender_.NowMs() - kOneWayDelayMs;

  NadaFeedback not_congested_fb = NadaFbGenerator::NotCongestedFb(
      receiving_rate, kRefSignalMs, send_time_ms);

  nada_sender_.set_original_operating_mode(true);
  nada_sender_.set_bitrate_kbps(original_bitrate);

  // Trigger AcceleratedRampUp mode.
  nada_sender_.GiveFeedback(not_congested_fb);
  int bitrate_1_kbps = nada_sender_.bitrate_kbps();
  EXPECT_GT(bitrate_1_kbps, original_bitrate);
  // Updates the bitrate according to the receiving rate and other constant
  // parameters.
  nada_sender_.AcceleratedRampUp(not_congested_fb);
  EXPECT_EQ(nada_sender_.bitrate_kbps(), bitrate_1_kbps);

  nada_sender_.set_original_operating_mode(false);
  nada_sender_.set_bitrate_kbps(original_bitrate);
  // Trigger AcceleratedRampUp mode.
  nada_sender_.GiveFeedback(not_congested_fb);
  bitrate_1_kbps = nada_sender_.bitrate_kbps();
  EXPECT_GT(bitrate_1_kbps, original_bitrate);
  nada_sender_.AcceleratedRampUp(not_congested_fb);
  EXPECT_EQ(nada_sender_.bitrate_kbps(), bitrate_1_kbps);
}

// Verify if AcceleratedRampDown is called and if bitrate decreases.
TEST_F(NadaSenderSideTest, AcceleratedRampDown) {
  const int64_t kOneWayDelayMs = 50;
  int original_bitrate = 3 * NadaBweSender::kMinRefRateKbps;
  size_t receiving_rate = static_cast<size_t>(original_bitrate);
  int64_t send_time_ms = nada_sender_.NowMs() - kOneWayDelayMs;

  NadaFeedback congested_fb =
      NadaFbGenerator::CongestedFb(receiving_rate, send_time_ms);

  nada_sender_.set_original_operating_mode(false);
  nada_sender_.set_bitrate_kbps(original_bitrate);
  nada_sender_.GiveFeedback(congested_fb);  // Trigger AcceleratedRampDown mode.
  int bitrate_1_kbps = nada_sender_.bitrate_kbps();
  EXPECT_LE(bitrate_1_kbps, original_bitrate * 0.9f + 0.5f);
  EXPECT_LT(bitrate_1_kbps, original_bitrate);

  // Updates the bitrate according to the receiving rate and other constant
  // parameters.
  nada_sender_.AcceleratedRampDown(congested_fb);
  int bitrate_2_kbps =
      std::max(nada_sender_.bitrate_kbps(), NadaBweSender::kMinRefRateKbps);
  EXPECT_EQ(bitrate_2_kbps, bitrate_1_kbps);
}

TEST_F(NadaSenderSideTest, GradualRateUpdate) {
  const int64_t kDeltaSMs = 20;
  const int64_t kRefSignalMs = 20;
  const int64_t kOneWayDelayMs = 50;
  int original_bitrate = 2 * NadaBweSender::kMinRefRateKbps;
  size_t receiving_rate = static_cast<size_t>(original_bitrate);
  int64_t send_time_ms = nada_sender_.NowMs() - kOneWayDelayMs;

  NadaFeedback congested_fb =
      NadaFbGenerator::CongestedFb(receiving_rate, send_time_ms);
  NadaFeedback not_congested_fb = NadaFbGenerator::NotCongestedFb(
      original_bitrate, kRefSignalMs, send_time_ms);

  nada_sender_.set_bitrate_kbps(original_bitrate);
  double smoothing_factor = 0.0;
  nada_sender_.GradualRateUpdate(congested_fb, kDeltaSMs, smoothing_factor);
  EXPECT_EQ(nada_sender_.bitrate_kbps(), original_bitrate);

  smoothing_factor = 1.0;
  nada_sender_.GradualRateUpdate(congested_fb, kDeltaSMs, smoothing_factor);
  EXPECT_LT(nada_sender_.bitrate_kbps(), original_bitrate);

  nada_sender_.set_bitrate_kbps(original_bitrate);
  nada_sender_.GradualRateUpdate(not_congested_fb, kDeltaSMs, smoothing_factor);
  EXPECT_GT(nada_sender_.bitrate_kbps(), original_bitrate);
}

// Sending bitrate should decrease and reach its Min bound.
TEST_F(NadaSenderSideTest, VeryLowBandwith) {
  const int64_t kOneWayDelayMs = 50;
  const int kMin = NadaBweSender::kMinRefRateKbps;
  size_t receiving_rate = static_cast<size_t>(kMin);
  int64_t send_time_ms = nada_sender_.NowMs() - kOneWayDelayMs;

  NadaFeedback extremely_congested_fb =
      NadaFbGenerator::ExtremelyCongestedFb(receiving_rate, send_time_ms);
  NadaFeedback congested_fb =
      NadaFbGenerator::CongestedFb(receiving_rate, send_time_ms);

  nada_sender_.set_bitrate_kbps(5 * kMin);
  nada_sender_.set_original_operating_mode(true);
  for (int i = 0; i < 100; ++i) {
    // Trigger GradualRateUpdate mode.
    nada_sender_.GiveFeedback(extremely_congested_fb);
  }
  // The original implementation doesn't allow the bitrate to stay at kMin,
  // even if the congestion signal is very high.
  EXPECT_GE(nada_sender_.bitrate_kbps(), kMin);

  nada_sender_.set_original_operating_mode(false);
  nada_sender_.set_bitrate_kbps(5 * kMin);

  for (int i = 0; i < 100; ++i) {
    int previous_bitrate = nada_sender_.bitrate_kbps();
    // Trigger AcceleratedRampDown mode.
    nada_sender_.GiveFeedback(congested_fb);
    EXPECT_LE(nada_sender_.bitrate_kbps(), previous_bitrate);
  }
  EXPECT_EQ(nada_sender_.bitrate_kbps(), kMin);
}

// Sending bitrate should increase and reach its Max bound.
TEST_F(NadaSenderSideTest, VeryHighBandwith) {
  const int64_t kOneWayDelayMs = 50;
  const int kMax = NadaBweSender::kMaxRefRateKbps;
  const size_t kRecentReceivingRate = static_cast<size_t>(kMax);
  const int64_t kRefSignalMs = 5;
  int64_t send_time_ms = nada_sender_.NowMs() - kOneWayDelayMs;

  NadaFeedback not_congested_fb = NadaFbGenerator::NotCongestedFb(
      kRecentReceivingRate, kRefSignalMs, send_time_ms);

  nada_sender_.set_original_operating_mode(true);
  for (int i = 0; i < 100; ++i) {
    int previous_bitrate = nada_sender_.bitrate_kbps();
    nada_sender_.GiveFeedback(not_congested_fb);
    EXPECT_GE(nada_sender_.bitrate_kbps(), previous_bitrate);
  }
  EXPECT_EQ(nada_sender_.bitrate_kbps(), kMax);

  nada_sender_.set_original_operating_mode(false);
  nada_sender_.set_bitrate_kbps(NadaBweSender::kMinRefRateKbps);

  for (int i = 0; i < 100; ++i) {
    int previous_bitrate = nada_sender_.bitrate_kbps();
    nada_sender_.GiveFeedback(not_congested_fb);
    EXPECT_GE(nada_sender_.bitrate_kbps(), previous_bitrate);
  }
  EXPECT_EQ(nada_sender_.bitrate_kbps(), kMax);
}

TEST_F(NadaReceiverSideTest, ReceivingRateNoPackets) {
  EXPECT_EQ(nada_receiver_.RecentReceivingRate(), static_cast<size_t>(0));
}

TEST_F(NadaReceiverSideTest, ReceivingRateSinglePacket) {
  const size_t kPayloadSizeBytes = 500 * 1000;
  const int64_t kSendTimeUs = 300 * 1000;
  const int64_t kArrivalTimeMs = kSendTimeUs / 1000 + 100;
  const uint16_t kSequenceNumber = 1;
  const int64_t kTimeWindowMs = NadaBweReceiver::kReceivingRateTimeWindowMs;

  const MediaPacket media_packet(kFlowId, kSendTimeUs, kPayloadSizeBytes,
                                 kSequenceNumber);
  nada_receiver_.ReceivePacket(kArrivalTimeMs, media_packet);

  const size_t kReceivingRateKbps = 8 * kPayloadSizeBytes / kTimeWindowMs;

  EXPECT_EQ(nada_receiver_.RecentReceivingRate(), kReceivingRateKbps);
}

TEST_F(NadaReceiverSideTest, ReceivingRateLargePackets) {
  const size_t kPayloadSizeBytes = 3000 * 1000;
  const int64_t kTimeGapMs = 3000;  // Between each packet.
  const int64_t kOneWayDelayMs = 1000;

  for (int i = 1; i < 5; ++i) {
    int64_t send_time_us = i * kTimeGapMs * 1000;
    int64_t arrival_time_ms = send_time_us / 1000 + kOneWayDelayMs;
    uint16_t sequence_number = i;
    const MediaPacket media_packet(kFlowId, send_time_us, kPayloadSizeBytes,
                                   sequence_number);
    nada_receiver_.ReceivePacket(arrival_time_ms, media_packet);
  }

  const size_t kReceivingRateKbps = 8 * kPayloadSizeBytes / kTimeGapMs;
  EXPECT_EQ(nada_receiver_.RecentReceivingRate(), kReceivingRateKbps);
}

TEST_F(NadaReceiverSideTest, ReceivingRateSmallPackets) {
  const size_t kPayloadSizeBytes = 100 * 1000;
  const int64_t kTimeGapMs = 50;  // Between each packet.
  const int64_t kOneWayDelayMs = 50;

  for (int i = 1; i < 50; ++i) {
    int64_t send_time_us = i * kTimeGapMs * 1000;
    int64_t arrival_time_ms = send_time_us / 1000 + kOneWayDelayMs;
    uint16_t sequence_number = i;
    const MediaPacket media_packet(kFlowId, send_time_us, kPayloadSizeBytes,
                                   sequence_number);
    nada_receiver_.ReceivePacket(arrival_time_ms, media_packet);
  }

  const size_t kReceivingRateKbps = 8 * kPayloadSizeBytes / kTimeGapMs;
  EXPECT_EQ(nada_receiver_.RecentReceivingRate(), kReceivingRateKbps);
}

TEST_F(NadaReceiverSideTest, ReceivingRateIntermittentPackets) {
  const size_t kPayloadSizeBytes = 100 * 1000;
  const int64_t kTimeGapMs = 50;  // Between each packet.
  const int64_t kFirstSendTimeMs = 0;
  const int64_t kOneWayDelayMs = 50;

  // Gap between first and other packets
  const MediaPacket media_packet(kFlowId, kFirstSendTimeMs, kPayloadSizeBytes,
                                 1);
  nada_receiver_.ReceivePacket(kFirstSendTimeMs + kOneWayDelayMs, media_packet);

  const int64_t kDelayAfterFirstPacketMs = 1000;
  const int kNumPackets = 5;  // Small enough so that all packets are covered.
  EXPECT_LT((kNumPackets - 2) * kTimeGapMs,
            NadaBweReceiver::kReceivingRateTimeWindowMs);
  const int64_t kTimeWindowMs =
      kDelayAfterFirstPacketMs + (kNumPackets - 2) * kTimeGapMs;

  for (int i = 2; i <= kNumPackets; ++i) {
    int64_t send_time_us =
        ((i - 2) * kTimeGapMs + kFirstSendTimeMs + kDelayAfterFirstPacketMs) *
        1000;
    int64_t arrival_time_ms = send_time_us / 1000 + kOneWayDelayMs;
    uint16_t sequence_number = i;
    const MediaPacket media_packet(kFlowId, send_time_us, kPayloadSizeBytes,
                                   sequence_number);
    nada_receiver_.ReceivePacket(arrival_time_ms, media_packet);
  }

  const size_t kTotalReceivedKb = 8 * kNumPackets * kPayloadSizeBytes;
  const int64_t kCorrectedTimeWindowMs =
      (kTimeWindowMs * kNumPackets) / (kNumPackets - 1);
  EXPECT_EQ(nada_receiver_.RecentReceivingRate(),
            kTotalReceivedKb / kCorrectedTimeWindowMs);
}

TEST_F(NadaReceiverSideTest, ReceivingRateDuplicatedPackets) {
  const size_t kPayloadSizeBytes = 500 * 1000;
  const int64_t kSendTimeUs = 300 * 1000;
  const int64_t kArrivalTimeMs = kSendTimeUs / 1000 + 100;
  const uint16_t kSequenceNumber = 1;
  const int64_t kTimeWindowMs = NadaBweReceiver::kReceivingRateTimeWindowMs;

  // Insert the same packet twice.
  for (int i = 0; i < 2; ++i) {
    const MediaPacket media_packet(kFlowId, kSendTimeUs + 50 * i,
                                   kPayloadSizeBytes, kSequenceNumber);
    nada_receiver_.ReceivePacket(kArrivalTimeMs + 50 * i, media_packet);
  }
  // Should be counted only once.
  const size_t kReceivingRateKbps = 8 * kPayloadSizeBytes / kTimeWindowMs;

  EXPECT_EQ(nada_receiver_.RecentReceivingRate(), kReceivingRateKbps);
}

TEST_F(NadaReceiverSideTest, PacketLossNoPackets) {
  EXPECT_EQ(nada_receiver_.RecentPacketLossRatio(), 0.0f);
}

TEST_F(NadaReceiverSideTest, PacketLossSinglePacket) {
  const MediaPacket media_packet(kFlowId, 0, 0, 0);
  nada_receiver_.ReceivePacket(0, media_packet);
  EXPECT_EQ(nada_receiver_.RecentPacketLossRatio(), 0.0f);
}

TEST_F(NadaReceiverSideTest, PacketLossContiguousPackets) {
  const int64_t kTimeWindowMs = NadaBweReceiver::kPacketLossTimeWindowMs;
  const int kSetCapacity = NadaBweReceiver::kSetCapacity;

  for (int i = 0; i < 10; ++i) {
    uint16_t sequence_number = static_cast<uint16_t>(i);
    // Sequence_number and flow_id are the only members that matter here.
    const MediaPacket media_packet(kFlowId, 0, 0, sequence_number);
    // Arrival time = 0, all packets will be considered.
    nada_receiver_.ReceivePacket(0, media_packet);
  }
  EXPECT_EQ(nada_receiver_.RecentPacketLossRatio(), 0.0f);

  for (int i = 30; i > 20; i--) {
    uint16_t sequence_number = static_cast<uint16_t>(i);
    // Sequence_number and flow_id are the only members that matter here.
    const MediaPacket media_packet(kFlowId, 0, 0, sequence_number);
    // Only the packets sent in this for loop will be considered.
    nada_receiver_.ReceivePacket(2 * kTimeWindowMs, media_packet);
  }
  EXPECT_EQ(nada_receiver_.RecentPacketLossRatio(), 0.0f);

  // Should handle uint16_t overflow.
  for (int i = 0xFFFF - 10; i < 0xFFFF + 10; ++i) {
    uint16_t sequence_number = static_cast<uint16_t>(i);
    const MediaPacket media_packet(kFlowId, 0, 0, sequence_number);
    // Only the packets sent in this for loop will be considered.
    nada_receiver_.ReceivePacket(4 * kTimeWindowMs, media_packet);
  }
  EXPECT_EQ(nada_receiver_.RecentPacketLossRatio(), 0.0f);

  // Should handle set overflow.
  for (int i = 0; i < kSetCapacity * 1.5; ++i) {
    uint16_t sequence_number = static_cast<uint16_t>(i);
    const MediaPacket media_packet(kFlowId, 0, 0, sequence_number);
    // Only the packets sent in this for loop will be considered.
    nada_receiver_.ReceivePacket(6 * kTimeWindowMs, media_packet);
  }
  EXPECT_EQ(nada_receiver_.RecentPacketLossRatio(), 0.0f);
}

// Should handle duplicates.
TEST_F(NadaReceiverSideTest, PacketLossDuplicatedPackets) {
  const int64_t kTimeWindowMs = NadaBweReceiver::kPacketLossTimeWindowMs;

  for (int i = 0; i < 10; ++i) {
    const MediaPacket media_packet(kFlowId, 0, 0, 0);
    // Arrival time = 0, all packets will be considered.
    nada_receiver_.ReceivePacket(0, media_packet);
  }
  EXPECT_EQ(nada_receiver_.RecentPacketLossRatio(), 0.0f);

  // Missing the element 5.
  const uint16_t kSequenceNumbers[] = {1, 2, 3, 4, 6, 7, 8};
  const int kNumPackets = ARRAY_SIZE(kSequenceNumbers);

  // Insert each sequence number twice.
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < kNumPackets; j++) {
      const MediaPacket media_packet(kFlowId, 0, 0, kSequenceNumbers[j]);
      // Only the packets sent in this for loop will be considered.
      nada_receiver_.ReceivePacket(2 * kTimeWindowMs, media_packet);
    }
  }

  EXPECT_NEAR(nada_receiver_.RecentPacketLossRatio(), 1.0f / (kNumPackets + 1),
              0.1f / (kNumPackets + 1));
}

TEST_F(NadaReceiverSideTest, PacketLossLakingPackets) {
  const int kSetCapacity = NadaBweReceiver::kSetCapacity;
  EXPECT_LT(kSetCapacity, 0xFFFF);

  // Missing every other packet.
  for (int i = 0; i < kSetCapacity; ++i) {
    if ((i & 1) == 0) {  // Only even sequence numbers.
      uint16_t sequence_number = static_cast<uint16_t>(i);
      const MediaPacket media_packet(kFlowId, 0, 0, sequence_number);
      // Arrival time = 0, all packets will be considered.
      nada_receiver_.ReceivePacket(0, media_packet);
    }
  }
  EXPECT_NEAR(nada_receiver_.RecentPacketLossRatio(), 0.5f, 0.01f);
}

TEST_F(NadaReceiverSideTest, PacketLossLakingFewPackets) {
  const int kSetCapacity = NadaBweReceiver::kSetCapacity;
  EXPECT_LT(kSetCapacity, 0xFFFF);

  const int kPeriod = 100;
  // Missing one for each kPeriod packets.
  for (int i = 0; i < kSetCapacity; ++i) {
    if ((i % kPeriod) != 0) {
      uint16_t sequence_number = static_cast<uint16_t>(i);
      const MediaPacket media_packet(kFlowId, 0, 0, sequence_number);
      // Arrival time = 0, all packets will be considered.
      nada_receiver_.ReceivePacket(0, media_packet);
    }
  }
  EXPECT_NEAR(nada_receiver_.RecentPacketLossRatio(), 1.0f / kPeriod,
              0.1f / kPeriod);
}

// Packet's sequence numbers greatly apart, expect high loss.
TEST_F(NadaReceiverSideTest, PacketLossWideGap) {
  const int64_t kTimeWindowMs = NadaBweReceiver::kPacketLossTimeWindowMs;

  const MediaPacket media_packet1(0, 0, 0, 1);
  const MediaPacket media_packet2(0, 0, 0, 1000);
  // Only these two packets will be considered.
  nada_receiver_.ReceivePacket(0, media_packet1);
  nada_receiver_.ReceivePacket(0, media_packet2);
  EXPECT_NEAR(nada_receiver_.RecentPacketLossRatio(), 0.998f, 0.0001f);

  const MediaPacket media_packet3(0, 0, 0, 0);
  const MediaPacket media_packet4(0, 0, 0, 0x8000);
  // Only these two packets will be considered.
  nada_receiver_.ReceivePacket(2 * kTimeWindowMs, media_packet3);
  nada_receiver_.ReceivePacket(2 * kTimeWindowMs, media_packet4);
  EXPECT_NEAR(nada_receiver_.RecentPacketLossRatio(), 0.99994f, 0.00001f);
}

// Packets arriving unordered should not be counted as losted.
TEST_F(NadaReceiverSideTest, PacketLossUnorderedPackets) {
  const int kNumPackets = NadaBweReceiver::kSetCapacity / 2;
  std::vector<uint16_t> sequence_numbers;

  for (int i = 0; i < kNumPackets; ++i) {
    sequence_numbers.push_back(static_cast<uint16_t>(i + 1));
  }

  random_shuffle(sequence_numbers.begin(), sequence_numbers.end());

  for (int i = 0; i < kNumPackets; ++i) {
    const MediaPacket media_packet(kFlowId, 0, 0, sequence_numbers[i]);
    // Arrival time = 0, all packets will be considered.
    nada_receiver_.ReceivePacket(0, media_packet);
  }

  EXPECT_EQ(nada_receiver_.RecentPacketLossRatio(), 0.0f);
}

TEST_F(FilterTest, MedianConstantArray) {
  MedianFilterConstantArray();
  for (int i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(median_filtered_[i], raw_signal_[i]);
  }
}

TEST_F(FilterTest, MedianIntermittentNoise) {
  MedianFilterIntermittentNoise();
}

TEST_F(FilterTest, ExponentialSmoothingConstantArray) {
  int64_t exp_smoothed[kNumElements];
  ExponentialSmoothingConstantArray(exp_smoothed);
  for (int i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(exp_smoothed[i], kSignalValue);
  }
}

TEST_F(FilterTest, ExponentialSmoothingInitialPertubation) {
  const int64_t kSignal[] = {90000, 0, 0, 0, 0, 0};
  const int kNumElements = ARRAY_SIZE(kSignal);
  int64_t exp_smoothed[kNumElements];
  ExponentialSmoothingFilter(kSignal, kNumElements, exp_smoothed);
  for (int i = 1; i < kNumElements; ++i) {
    EXPECT_EQ(
        exp_smoothed[i],
        static_cast<int64_t>(exp_smoothed[i - 1] * (1.0f - kAlpha) + 0.5f));
  }
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
