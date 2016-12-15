/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/gtest.h"
#include "webrtc/base/random.h"
#include "webrtc/modules/congestion_controller/trendline_estimator.h"

namespace webrtc {

namespace {
constexpr size_t kWindowSize = 15;
constexpr double kSmoothing = 0.0;
constexpr double kGain = 1;
constexpr int64_t kAvgTimeBetweenPackets = 10;
}  // namespace

TEST(TrendlineEstimator, PerfectLineSlopeOneHalf) {
  TrendlineEstimator estimator(kWindowSize, kSmoothing, kGain);
  Random rand(0x1234567);
  double now_ms = rand.Rand<double>() * 10000;
  for (size_t i = 1; i < 2 * kWindowSize; i++) {
    double send_delta = rand.Rand<double>() * 2 * kAvgTimeBetweenPackets;
    double recv_delta = 2 * send_delta;
    now_ms += recv_delta;
    estimator.Update(recv_delta, send_delta, now_ms);
    if (i < kWindowSize)
      EXPECT_NEAR(estimator.trendline_slope(), 0, 0.001);
    else
      EXPECT_NEAR(estimator.trendline_slope(), 0.5, 0.001);
  }
}

TEST(TrendlineEstimator, PerfectLineSlopeMinusOne) {
  TrendlineEstimator estimator(kWindowSize, kSmoothing, kGain);
  Random rand(0x1234567);
  double now_ms = rand.Rand<double>() * 10000;
  for (size_t i = 1; i < 2 * kWindowSize; i++) {
    double send_delta = rand.Rand<double>() * 2 * kAvgTimeBetweenPackets;
    double recv_delta = 0.5 * send_delta;
    now_ms += recv_delta;
    estimator.Update(recv_delta, send_delta, now_ms);
    if (i < kWindowSize)
      EXPECT_NEAR(estimator.trendline_slope(), 0, 0.001);
    else
      EXPECT_NEAR(estimator.trendline_slope(), -1, 0.001);
  }
}

TEST(TrendlineEstimator, PerfectLineSlopeZero) {
  TrendlineEstimator estimator(kWindowSize, kSmoothing, kGain);
  Random rand(0x1234567);
  double now_ms = rand.Rand<double>() * 10000;
  for (size_t i = 1; i < 2 * kWindowSize; i++) {
    double send_delta = rand.Rand<double>() * 2 * kAvgTimeBetweenPackets;
    double recv_delta = send_delta;
    now_ms += recv_delta;
    estimator.Update(recv_delta, send_delta, now_ms);
    EXPECT_NEAR(estimator.trendline_slope(), 0, 0.001);
  }
}

TEST(TrendlineEstimator, JitteryLineSlopeOneHalf) {
  TrendlineEstimator estimator(kWindowSize, kSmoothing, kGain);
  Random rand(0x1234567);
  double now_ms = rand.Rand<double>() * 10000;
  for (size_t i = 1; i < 2 * kWindowSize; i++) {
    double send_delta = rand.Rand<double>() * 2 * kAvgTimeBetweenPackets;
    double recv_delta = 2 * send_delta + rand.Gaussian(0, send_delta / 3);
    now_ms += recv_delta;
    estimator.Update(recv_delta, send_delta, now_ms);
    if (i < kWindowSize)
      EXPECT_NEAR(estimator.trendline_slope(), 0, 0.001);
    else
      EXPECT_NEAR(estimator.trendline_slope(), 0.5, 0.1);
  }
}

TEST(TrendlineEstimator, JitteryLineSlopeMinusOne) {
  TrendlineEstimator estimator(kWindowSize, kSmoothing, kGain);
  Random rand(0x1234567);
  double now_ms = rand.Rand<double>() * 10000;
  for (size_t i = 1; i < 2 * kWindowSize; i++) {
    double send_delta = rand.Rand<double>() * 2 * kAvgTimeBetweenPackets;
    double recv_delta = 0.5 * send_delta + rand.Gaussian(0, send_delta / 25);
    now_ms += recv_delta;
    estimator.Update(recv_delta, send_delta, now_ms);
    if (i < kWindowSize)
      EXPECT_NEAR(estimator.trendline_slope(), 0, 0.001);
    else
      EXPECT_NEAR(estimator.trendline_slope(), -1, 0.1);
  }
}

TEST(TrendlineEstimator, JitteryLineSlopeZero) {
  TrendlineEstimator estimator(kWindowSize, kSmoothing, kGain);
  Random rand(0x1234567);
  double now_ms = rand.Rand<double>() * 10000;
  for (size_t i = 1; i < 2 * kWindowSize; i++) {
    double send_delta = rand.Rand<double>() * 2 * kAvgTimeBetweenPackets;
    double recv_delta = send_delta + rand.Gaussian(0, send_delta / 8);
    now_ms += recv_delta;
    estimator.Update(recv_delta, send_delta, now_ms);
    EXPECT_NEAR(estimator.trendline_slope(), 0, 0.1);
  }
}

}  // namespace webrtc
