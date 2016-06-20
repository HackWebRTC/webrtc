/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/delay_based_bwe.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

class TestDelayBasedBwe : public ::testing::Test, public RemoteBitrateObserver {
 public:
  static constexpr int kArrivalTimeClockOffsetMs = 60000;
  static constexpr int kNumProbes = 5;

  TestDelayBasedBwe()
      : bwe_(this), clock_(0), bitrate_updated_(false), latest_bitrate_(0) {}

  uint32_t AbsSendTime(int64_t t, int64_t denom) {
    return (((t << 18) + (denom >> 1)) / denom) & 0x00fffffful;
  }

  void IncomingPacket(uint32_t ssrc,
                      size_t payload_size,
                      int64_t arrival_time,
                      uint32_t rtp_timestamp,
                      uint32_t absolute_send_time,
                      int probe_cluster_id) {
    RTPHeader header;
    memset(&header, 0, sizeof(header));
    header.ssrc = ssrc;
    header.timestamp = rtp_timestamp;
    header.extension.hasAbsoluteSendTime = true;
    header.extension.absoluteSendTime = absolute_send_time;
    bwe_.IncomingPacket(arrival_time + kArrivalTimeClockOffsetMs, payload_size,
                        header, probe_cluster_id);
  }

  void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                               uint32_t bitrate) {
    bitrate_updated_ = true;
    latest_bitrate_ = bitrate;
  }

  bool bitrate_updated() {
    bool res = bitrate_updated_;
    bitrate_updated_ = false;
    return res;
  }

  int latest_bitrate() { return latest_bitrate_; }

  DelayBasedBwe bwe_;
  SimulatedClock clock_;

 private:
  bool bitrate_updated_;
  int latest_bitrate_;
};

TEST_F(TestDelayBasedBwe, ProbeDetection) {
  int64_t now_ms = clock_.TimeInMilliseconds();

  // First burst sent at 8 * 1000 / 10 = 800 kbps.
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(10);
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * now_ms, AbsSendTime(now_ms, 1000), 0);
  }
  EXPECT_TRUE(bitrate_updated());

  // Second burst sent at 8 * 1000 / 5 = 1600 kbps.
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(5);
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * now_ms, AbsSendTime(now_ms, 1000), 1);
  }

  EXPECT_TRUE(bitrate_updated());
  EXPECT_GT(latest_bitrate(), 1500000);
}

TEST_F(TestDelayBasedBwe, ProbeDetectionNonPacedPackets) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  // First burst sent at 8 * 1000 / 10 = 800 kbps, but with every other packet
  // not being paced which could mess things up.
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(5);
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * now_ms, AbsSendTime(now_ms, 1000), 0);
    // Non-paced packet, arriving 5 ms after.
    clock_.AdvanceTimeMilliseconds(5);
    IncomingPacket(0, PacedSender::kMinProbePacketSize + 1, now_ms, 90 * now_ms,
                   AbsSendTime(now_ms, 1000), PacketInfo::kNotAProbe);
  }

  EXPECT_TRUE(bitrate_updated());
  EXPECT_GT(latest_bitrate(), 800000);
}

// Packets will require 5 ms to be transmitted to the receiver, causing packets
// of the second probe to be dispersed.
TEST_F(TestDelayBasedBwe, ProbeDetectionTooHighBitrate) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  int64_t send_time_ms = 0;
  // First burst sent at 8 * 1000 / 10 = 800 kbps.
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(10);
    now_ms = clock_.TimeInMilliseconds();
    send_time_ms += 10;
    IncomingPacket(0, 1000, now_ms, 90 * send_time_ms,
                   AbsSendTime(send_time_ms, 1000), 0);
  }

  // Second burst sent at 8 * 1000 / 5 = 1600 kbps, arriving at 8 * 1000 / 8 =
  // 1000 kbps.
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(8);
    now_ms = clock_.TimeInMilliseconds();
    send_time_ms += 5;
    IncomingPacket(0, 1000, now_ms, send_time_ms,
                   AbsSendTime(send_time_ms, 1000), 1);
  }

  EXPECT_TRUE(bitrate_updated());
  EXPECT_NEAR(latest_bitrate(), 800000, 10000);
}

TEST_F(TestDelayBasedBwe, ProbeDetectionSlightlyFasterArrival) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  // First burst sent at 8 * 1000 / 10 = 800 kbps.
  // Arriving at 8 * 1000 / 5 = 1600 kbps.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(5);
    send_time_ms += 10;
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * send_time_ms,
                   AbsSendTime(send_time_ms, 1000), 23);
  }

  EXPECT_TRUE(bitrate_updated());
  EXPECT_GT(latest_bitrate(), 800000);
}

TEST_F(TestDelayBasedBwe, ProbeDetectionFasterArrival) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  // First burst sent at 8 * 1000 / 10 = 800 kbps.
  // Arriving at 8 * 1000 / 5 = 1600 kbps.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(1);
    send_time_ms += 10;
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * send_time_ms,
                   AbsSendTime(send_time_ms, 1000), 0);
  }

  EXPECT_FALSE(bitrate_updated());
}

TEST_F(TestDelayBasedBwe, ProbeDetectionSlowerArrival) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  // First burst sent at 8 * 1000 / 5 = 1600 kbps.
  // Arriving at 8 * 1000 / 7 = 1142 kbps.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(7);
    send_time_ms += 5;
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * send_time_ms,
                   AbsSendTime(send_time_ms, 1000), 1);
  }

  EXPECT_TRUE(bitrate_updated());
  EXPECT_NEAR(latest_bitrate(), 1140000, 10000);
}

TEST_F(TestDelayBasedBwe, ProbeDetectionSlowerArrivalHighBitrate) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  // Burst sent at 8 * 1000 / 1 = 8000 kbps.
  // Arriving at 8 * 1000 / 2 = 4000 kbps.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(2);
    send_time_ms += 1;
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * send_time_ms,
                   AbsSendTime(send_time_ms, 1000), 1);
  }

  EXPECT_TRUE(bitrate_updated());
  EXPECT_NEAR(latest_bitrate(), 4000000u, 10000);
}

TEST_F(TestDelayBasedBwe, ProbingIgnoresSmallPackets) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  // Probing with 200 bytes every 10 ms, should be ignored by the probe
  // detection.
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(10);
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, PacedSender::kMinProbePacketSize, now_ms, 90 * now_ms,
                   AbsSendTime(now_ms, 1000), 1);
  }

  EXPECT_FALSE(bitrate_updated());

  // Followed by a probe with 1000 bytes packets, should be detected as a
  // probe.
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(10);
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * now_ms, AbsSendTime(now_ms, 1000), 1);
  }

  // Wait long enough so that we can call Process again.
  clock_.AdvanceTimeMilliseconds(1000);

  EXPECT_TRUE(bitrate_updated());
  EXPECT_NEAR(latest_bitrate(), 800000u, 10000);
}
}  // namespace webrtc
