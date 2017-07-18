/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/min_rtt_filter.h"

#include "webrtc/test/gtest.h"

namespace webrtc {
namespace testing {
namespace bwe {
TEST(MinRttFilterTest, InitializationCheck) {
  MinRttFilter min_rtt_filter;
  EXPECT_FALSE(min_rtt_filter.min_rtt_ms());
  EXPECT_EQ(min_rtt_filter.discovery_time(), 0);
}

TEST(MinRttFilterTest, AddRttSample) {
  MinRttFilter min_rtt_filter;
  min_rtt_filter.add_rtt_sample(120, 5);
  EXPECT_EQ(min_rtt_filter.min_rtt_ms(), 120);
  EXPECT_EQ(min_rtt_filter.discovery_time(), 5);
  min_rtt_filter.add_rtt_sample(121, 6);
  EXPECT_EQ(min_rtt_filter.discovery_time(), 5);
  min_rtt_filter.add_rtt_sample(119, 7);
  EXPECT_EQ(min_rtt_filter.discovery_time(), 7);
}

TEST(MinRttFilterTest, MinRttExpired) {
  MinRttFilter min_rtt_filter;
  min_rtt_filter.add_rtt_sample(120, 5);
  EXPECT_EQ(min_rtt_filter.min_rtt_expired(10, 5), true);
  EXPECT_EQ(min_rtt_filter.min_rtt_expired(9, 5), false);
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
