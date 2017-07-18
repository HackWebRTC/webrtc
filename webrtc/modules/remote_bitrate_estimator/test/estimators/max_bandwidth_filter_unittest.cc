/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/max_bandwidth_filter.h"

#include "webrtc/test/gtest.h"

namespace webrtc {
namespace testing {
namespace bwe {
TEST(MaxBandwidthFilterTest, InitializationCheck) {
  MaxBandwidthFilter max_bandwidth_filter;
  EXPECT_EQ(max_bandwidth_filter.max_bandwidth_estimate_bytes_per_ms(), 0);
}

TEST(MaxBandwidthFilterTest, AddOneBandwidthSample) {
  MaxBandwidthFilter max_bandwidth_filter;
  max_bandwidth_filter.AddBandwidthSample(13, 4, 10);
  EXPECT_EQ(max_bandwidth_filter.max_bandwidth_estimate_bytes_per_ms(), 13);
}

TEST(MaxBandwidthFilterTest, AddSeveralBandwidthSamples) {
  MaxBandwidthFilter max_bandwidth_filter;
  max_bandwidth_filter.AddBandwidthSample(10, 5, 10);
  max_bandwidth_filter.AddBandwidthSample(13, 6, 10);
  EXPECT_EQ(max_bandwidth_filter.max_bandwidth_estimate_bytes_per_ms(), 13);
}

TEST(MaxBandwidthFilterTest, SampleTimeOut) {
  MaxBandwidthFilter max_bandwidth_filter;
  max_bandwidth_filter.AddBandwidthSample(13, 5, 10);
  max_bandwidth_filter.AddBandwidthSample(10, 15, 10);
  EXPECT_EQ(max_bandwidth_filter.max_bandwidth_estimate_bytes_per_ms(), 10);
}

TEST(MaxBandwidthFilterTest, FullBandwidthReached) {
  MaxBandwidthFilter max_bandwidth_filter;
  max_bandwidth_filter.AddBandwidthSample(100, 1, 10);
  EXPECT_EQ(max_bandwidth_filter.FullBandwidthReached(1.25f, 3), false);
  max_bandwidth_filter.AddBandwidthSample(110, 2, 10);
  EXPECT_EQ(max_bandwidth_filter.FullBandwidthReached(1.25f, 3), false);
  max_bandwidth_filter.AddBandwidthSample(120, 3, 10);
  EXPECT_EQ(max_bandwidth_filter.FullBandwidthReached(1.25f, 3), false);
  max_bandwidth_filter.AddBandwidthSample(124, 4, 10);
  EXPECT_EQ(max_bandwidth_filter.FullBandwidthReached(1.25f, 3), true);
}

TEST(MaxBandwidthFilterTest, FullBandwidthNotReached) {
  MaxBandwidthFilter max_bandwidth_filter;
  max_bandwidth_filter.AddBandwidthSample(100, 1, 10);
  EXPECT_EQ(max_bandwidth_filter.FullBandwidthReached(1.25f, 3), false);
  max_bandwidth_filter.AddBandwidthSample(110, 2, 10);
  EXPECT_EQ(max_bandwidth_filter.FullBandwidthReached(1.25f, 3), false);
  max_bandwidth_filter.AddBandwidthSample(120, 3, 10);
  EXPECT_EQ(max_bandwidth_filter.FullBandwidthReached(1.25f, 3), false);
  max_bandwidth_filter.AddBandwidthSample(125, 4, 10);
  EXPECT_EQ(max_bandwidth_filter.FullBandwidthReached(1.25f, 3), false);
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
