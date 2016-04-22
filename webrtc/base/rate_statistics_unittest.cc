/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/rate_statistics.h"

namespace {

using webrtc::RateStatistics;

const int64_t kWindowMs = 500;

class RateStatisticsTest : public ::testing::Test {
 protected:
  RateStatisticsTest() : stats_(kWindowMs, 8000) {}
  RateStatistics stats_;
};

TEST_F(RateStatisticsTest, TestStrictMode) {
  int64_t now_ms = 0;
  // Should be initialized to 0.
  EXPECT_EQ(0u, stats_.Rate(now_ms));
  stats_.Update(1500, now_ms);
  // Expecting 1200 kbps since the window is initially kept small and grows as
  // we have more data.
  EXPECT_EQ(12000000u, stats_.Rate(now_ms));
  stats_.Reset();
  // Expecting 0 after init.
  EXPECT_EQ(0u, stats_.Rate(now_ms));
  for (int i = 0; i < 100000; ++i) {
    if (now_ms % 10 == 0) {
      stats_.Update(1500, now_ms);
    }
    // Approximately 1200 kbps expected. Not exact since when packets
    // are removed we will jump 10 ms to the next packet.
    if (now_ms > 0 && now_ms % kWindowMs == 0) {
      EXPECT_NEAR(1200000u, stats_.Rate(now_ms), 22000u);
    }
    now_ms += 1;
  }
  now_ms += kWindowMs;
  // The window is 2 seconds. If nothing has been received for that time
  // the estimate should be 0.
  EXPECT_EQ(0u, stats_.Rate(now_ms));
}

TEST_F(RateStatisticsTest, IncreasingThenDecreasingBitrate) {
  int64_t now_ms = 0;
  stats_.Reset();
  // Expecting 0 after init.
  uint32_t bitrate = stats_.Rate(now_ms);
  EXPECT_EQ(0u, bitrate);
  const uint32_t kExpectedBitrate = 8000000;
  // 1000 bytes per millisecond until plateau is reached.
  int prev_error = kExpectedBitrate;
  while (++now_ms < 10000) {
    stats_.Update(1000, now_ms);
    bitrate = stats_.Rate(now_ms);
    int error = kExpectedBitrate - bitrate;
    error = std::abs(error);
    // Expect the estimation error to decrease as the window is extended.
    EXPECT_LE(error, prev_error + 1);
    prev_error = error;
  }
  // Window filled, expect to be close to 8000000.
  EXPECT_EQ(kExpectedBitrate, bitrate);

  // 1000 bytes per millisecond until 10-second mark, 8000 kbps expected.
  while (++now_ms < 10000) {
    stats_.Update(1000, now_ms);
    bitrate = stats_.Rate(now_ms);
    EXPECT_EQ(kExpectedBitrate, bitrate);
  }
  // Zero bytes per millisecond until 0 is reached.
  while (++now_ms < 20000) {
    stats_.Update(0, now_ms);
    uint32_t new_bitrate = stats_.Rate(now_ms);
    if (new_bitrate != bitrate) {
      // New bitrate must be lower than previous one.
      EXPECT_LT(new_bitrate, bitrate);
    } else {
      // 0 kbps expected.
      EXPECT_EQ(0u, bitrate);
      break;
    }
    bitrate = new_bitrate;
  }
  // Zero bytes per millisecond until 20-second mark, 0 kbps expected.
  while (++now_ms < 20000) {
    stats_.Update(0, now_ms);
    EXPECT_EQ(0u, stats_.Rate(now_ms));
  }
}

TEST_F(RateStatisticsTest, ResetAfterSilence) {
  int64_t now_ms = 0;
  stats_.Reset();
  // Expecting 0 after init.
  uint32_t bitrate = stats_.Rate(now_ms);
  EXPECT_EQ(0u, bitrate);
  const uint32_t kExpectedBitrate = 8000000;
  // 1000 bytes per millisecond until the window has been filled.
  int prev_error = kExpectedBitrate;
  while (++now_ms < 10000) {
    stats_.Update(1000, now_ms);
    bitrate = stats_.Rate(now_ms);
    int error = kExpectedBitrate - bitrate;
    error = std::abs(error);
    // Expect the estimation error to decrease as the window is extended.
    EXPECT_LE(error, prev_error + 1);
    prev_error = error;
  }
  // Window filled, expect to be close to 8000000.
  EXPECT_EQ(kExpectedBitrate, bitrate);

  now_ms += kWindowMs + 1;
  EXPECT_EQ(0u, stats_.Rate(now_ms));
  stats_.Update(1000, now_ms);
  // We expect one sample of 1000 bytes, and that the bitrate is measured over
  // 1 ms, i.e., 8 * 1000 / 0.001 = 8000000.
  EXPECT_EQ(kExpectedBitrate, stats_.Rate(now_ms));
}
}  // namespace
