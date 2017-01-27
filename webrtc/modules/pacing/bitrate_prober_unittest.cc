/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>

#include "webrtc/modules/pacing/bitrate_prober.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

TEST(BitrateProberTest, VerifyStatesAndTimeBetweenProbes) {
  BitrateProber prober;
  EXPECT_FALSE(prober.IsProbing());
  int64_t now_ms = 0;
  EXPECT_EQ(-1, prober.TimeUntilNextProbe(now_ms));

  const int kTestBitrate1 = 900000;
  const int kTestBitrate2 = 1800000;
  const int kClusterSize = 5;
  const int kProbeSize = 1000;
  const int kMinProbeDurationMs = 15;

  prober.CreateProbeCluster(kTestBitrate1);
  prober.CreateProbeCluster(kTestBitrate2);
  EXPECT_FALSE(prober.IsProbing());

  prober.OnIncomingPacket(kProbeSize);
  EXPECT_TRUE(prober.IsProbing());
  EXPECT_EQ(0, prober.CurrentClusterId());

  // First packet should probe as soon as possible.
  EXPECT_EQ(0, prober.TimeUntilNextProbe(now_ms));

  for (int i = 0; i < kClusterSize; ++i) {
    now_ms += prober.TimeUntilNextProbe(now_ms);
    EXPECT_EQ(0, prober.TimeUntilNextProbe(now_ms));
    EXPECT_EQ(0, prober.CurrentClusterId());
    prober.ProbeSent(now_ms, kProbeSize);
  }

  EXPECT_GE(now_ms, kMinProbeDurationMs);
  // Verify that the actual bitrate is withing 10% of the target.
  double bitrate = kProbeSize * (kClusterSize - 1) * 8 * 1000.0 / now_ms;
  EXPECT_GT(bitrate, kTestBitrate1 * 0.9);
  EXPECT_LT(bitrate,  kTestBitrate1 * 1.1);

  now_ms += prober.TimeUntilNextProbe(now_ms);
  int64_t probe2_started = now_ms;

  for (int i = 0; i < kClusterSize; ++i) {
    now_ms += prober.TimeUntilNextProbe(now_ms);
    EXPECT_EQ(0, prober.TimeUntilNextProbe(now_ms));
    EXPECT_EQ(1, prober.CurrentClusterId());
    prober.ProbeSent(now_ms, kProbeSize);
  }

  // Verify that the actual bitrate is withing 10% of the target.
  int duration = now_ms - probe2_started;
  EXPECT_GE(duration, kMinProbeDurationMs);
  bitrate = kProbeSize * (kClusterSize - 1) * 8 * 1000.0 / duration;
  EXPECT_GT(bitrate, kTestBitrate2 * 0.9);
  EXPECT_LT(bitrate,  kTestBitrate2 * 1.1);

  EXPECT_EQ(-1, prober.TimeUntilNextProbe(now_ms));
  EXPECT_FALSE(prober.IsProbing());
}

TEST(BitrateProberTest, DoesntProbeWithoutRecentPackets) {
  BitrateProber prober;
  EXPECT_FALSE(prober.IsProbing());
  int64_t now_ms = 0;
  EXPECT_EQ(-1, prober.TimeUntilNextProbe(now_ms));

  prober.CreateProbeCluster(900000);
  EXPECT_FALSE(prober.IsProbing());

  prober.OnIncomingPacket(1000);
  EXPECT_TRUE(prober.IsProbing());
  EXPECT_EQ(0, prober.TimeUntilNextProbe(now_ms));
  prober.ProbeSent(now_ms, 1000);
  // Let time pass, no large enough packets put into prober.
  now_ms += 6000;
  EXPECT_EQ(-1, prober.TimeUntilNextProbe(now_ms));
  // Insert a large-enough packet after downtime while probing should reset to
  // perform a new probe since the requested one didn't finish.
  prober.OnIncomingPacket(1000);
  EXPECT_EQ(0, prober.TimeUntilNextProbe(now_ms));
  prober.ProbeSent(now_ms, 1000);
  // Next packet should be part of new probe and be sent with non-zero delay.
  prober.OnIncomingPacket(1000);
  EXPECT_GT(prober.TimeUntilNextProbe(now_ms), 0);
}

TEST(BitrateProberTest, DoesntInitializeProbingForSmallPackets) {
  BitrateProber prober;
  prober.SetEnabled(true);
  EXPECT_FALSE(prober.IsProbing());

  prober.OnIncomingPacket(100);
  EXPECT_FALSE(prober.IsProbing());
}

TEST(BitrateProberTest, VerifyProbeSizeOnHighBitrate) {
  BitrateProber prober;
  constexpr unsigned kHighBitrateBps = 10000000;  // 10 Mbps

  prober.CreateProbeCluster(kHighBitrateBps);
  // Probe size should ensure a minimum of 1 ms interval.
  EXPECT_GT(prober.RecommendedMinProbeSize(), kHighBitrateBps / 8000);
}

TEST(BitrateProberTest, MinumumNumberOfProbingPackets) {
  BitrateProber prober;
  // Even when probing at a low bitrate we expect a minimum number
  // of packets to be sent.
  constexpr int kBitrateBps = 100000;  // 100 kbps
  constexpr int kPacketSizeBytes = 1000;

  prober.CreateProbeCluster(kBitrateBps);
  prober.OnIncomingPacket(kPacketSizeBytes);
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(prober.IsProbing());
    prober.ProbeSent(0, kPacketSizeBytes);
  }

  EXPECT_FALSE(prober.IsProbing());
}

TEST(BitrateProberTest, ScaleBytesUsedForProbing) {
  BitrateProber prober;
  constexpr int kBitrateBps = 10000000;  // 10 Mbps
  constexpr int kPacketSizeBytes = 1000;
  constexpr int kExpectedBytesSent = kBitrateBps * 15 / 8000;

  prober.CreateProbeCluster(kBitrateBps);
  prober.OnIncomingPacket(kPacketSizeBytes);
  int bytes_sent = 0;
  while (bytes_sent < kExpectedBytesSent) {
    ASSERT_TRUE(prober.IsProbing());
    prober.ProbeSent(0, kPacketSizeBytes);
    bytes_sent += kPacketSizeBytes;
  }

  EXPECT_FALSE(prober.IsProbing());
}

}  // namespace webrtc
