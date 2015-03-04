/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"

#include <algorithm>
#include <vector>

#include "webrtc/modules/bitrate_controller/remb_suppressor.h"

class TestSuppressor : public webrtc::RembSuppressor {
 public:
  explicit TestSuppressor(webrtc::Clock* clock) : RembSuppressor(clock) {}
  virtual ~TestSuppressor() {}

  bool Enabled() override { return true; }
};

class RembSuppressorTest : public ::testing::Test {
 protected:
  RembSuppressorTest() : clock_(0), suppressor_(&clock_) {}
  ~RembSuppressorTest() {}

  virtual void SetUp() {}

  virtual void TearDown() {}

  bool NewRemb(uint32_t bitrate_bps) {
    suppressor_.SetBitrateSent(bitrate_bps);
    bool suppress = suppressor_.SuppresNewRemb(bitrate_bps);
    // Default one REMB per second.
    clock_.AdvanceTimeMilliseconds(1000);
    return suppress;
  }

  webrtc::SimulatedClock clock_;
  TestSuppressor suppressor_;
};

TEST_F(RembSuppressorTest, Basic) {
  // Never true on first sample.
  EXPECT_FALSE(NewRemb(50000));
  // Some rampup.
  EXPECT_FALSE(NewRemb(55000));
  EXPECT_FALSE(NewRemb(60500));
  EXPECT_FALSE(NewRemb(66550));
  EXPECT_FALSE(NewRemb(73250));

  // Reached limit, some fluctuation ok.
  EXPECT_FALSE(NewRemb(72100));
  EXPECT_FALSE(NewRemb(75500));
  EXPECT_FALSE(NewRemb(69250));
  EXPECT_FALSE(NewRemb(73250));
}

TEST_F(RembSuppressorTest, RecoveryTooSlow) {
  // Never true on first sample.
  EXPECT_FALSE(NewRemb(50000));

  // Large drop.
  EXPECT_TRUE(NewRemb(22499));

  // No new estimate, still suppressing.
  EXPECT_TRUE(NewRemb(22499));

  // Too little increase - stop suppressing.
  EXPECT_FALSE(NewRemb(22835));
}

TEST_F(RembSuppressorTest, RembDownDurinSupression) {
  // Never true on first sample.
  EXPECT_FALSE(NewRemb(50000));

  // Large drop.
  EXPECT_TRUE(NewRemb(22499));

  // Remb is not allowed to fall.
  EXPECT_FALSE(NewRemb(22498));
}

TEST_F(RembSuppressorTest, GlitchWithRecovery) {
  const uint32_t start_bitrate = 300000;
  uint32_t bitrate = start_bitrate;
  // Never true on first sample.
  EXPECT_FALSE(NewRemb(bitrate));

  bitrate = static_cast<uint32_t>(bitrate * 0.44);
  EXPECT_TRUE(NewRemb(bitrate));

  while (bitrate < start_bitrate) {
    EXPECT_TRUE(NewRemb(bitrate));
    bitrate = static_cast<uint32_t>(bitrate * 1.10);
  }

  EXPECT_FALSE(NewRemb(bitrate));
}

TEST_F(RembSuppressorTest, BitrateSent) {
  // Never true on first sample.
  EXPECT_FALSE(NewRemb(50000));

  // Only suppress large drop, if we are not sending at full capacity.
  suppressor_.SetBitrateSent(37500);
  EXPECT_FALSE(suppressor_.SuppresNewRemb(22499));
}
