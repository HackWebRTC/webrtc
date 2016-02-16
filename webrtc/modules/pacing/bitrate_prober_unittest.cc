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

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/pacing/bitrate_prober.h"

namespace webrtc {

TEST(BitrateProberTest, VerifyStatesAndTimeBetweenProbes) {
  BitrateProber prober;
  EXPECT_FALSE(prober.IsProbing());
  int64_t now_ms = 0;
  EXPECT_EQ(-1, prober.TimeUntilNextProbe(now_ms));

  prober.SetEnabled(true);
  EXPECT_FALSE(prober.IsProbing());

  prober.OnIncomingPacket(300000, 1000, now_ms);
  EXPECT_TRUE(prober.IsProbing());

  // First packet should probe as soon as possible.
  EXPECT_EQ(0, prober.TimeUntilNextProbe(now_ms));
  prober.PacketSent(now_ms, 1000);

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(8, prober.TimeUntilNextProbe(now_ms));
    now_ms += 4;
    EXPECT_EQ(4, prober.TimeUntilNextProbe(now_ms));
    now_ms += 4;
    EXPECT_EQ(0, prober.TimeUntilNextProbe(now_ms));
    prober.PacketSent(now_ms, 1000);
  }
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(4, prober.TimeUntilNextProbe(now_ms));
    now_ms += 4;
    EXPECT_EQ(0, prober.TimeUntilNextProbe(now_ms));
    prober.PacketSent(now_ms, 1000);
  }

  EXPECT_EQ(-1, prober.TimeUntilNextProbe(now_ms));
  EXPECT_FALSE(prober.IsProbing());
}

TEST(BitrateProberTest, DoesntProbeWithoutRecentPackets) {
  BitrateProber prober;
  EXPECT_FALSE(prober.IsProbing());
  int64_t now_ms = 0;
  EXPECT_EQ(-1, prober.TimeUntilNextProbe(now_ms));

  prober.SetEnabled(true);
  EXPECT_FALSE(prober.IsProbing());

  prober.OnIncomingPacket(300000, 1000, now_ms);
  EXPECT_TRUE(prober.IsProbing());
  EXPECT_EQ(0, prober.TimeUntilNextProbe(now_ms));
  // Let time pass, no large enough packets put into prober.
  now_ms += 6000;
  EXPECT_EQ(-1, prober.TimeUntilNextProbe(now_ms));
  // Insert a small packet, not a candidate for probing.
  prober.OnIncomingPacket(300000, 100, now_ms);
  prober.PacketSent(now_ms, 100);
  EXPECT_EQ(-1, prober.TimeUntilNextProbe(now_ms));
  // Insert a large-enough packet after downtime while probing should reset to
  // perform a new probe since the requested one didn't finish.
  prober.OnIncomingPacket(300000, 1000, now_ms);
  EXPECT_EQ(0, prober.TimeUntilNextProbe(now_ms));
  prober.PacketSent(now_ms, 1000);
  // Next packet should be part of new probe and be sent with non-zero delay.
  prober.OnIncomingPacket(300000, 1000, now_ms);
  EXPECT_GT(prober.TimeUntilNextProbe(now_ms), 0);
}

TEST(BitrateProberTest, DoesntInitializeProbingForSmallPackets) {
  BitrateProber prober;
  prober.SetEnabled(true);
  EXPECT_FALSE(prober.IsProbing());

  prober.OnIncomingPacket(300000, 100, 0);
  EXPECT_FALSE(prober.IsProbing());
}

}  // namespace webrtc
