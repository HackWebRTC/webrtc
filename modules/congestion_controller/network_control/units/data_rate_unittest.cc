/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/network_control/units/data_rate.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
TEST(DataRateTest, GetBackSameValues) {
  const int64_t kValue = 123 * 8;
  EXPECT_EQ(DataRate::bytes_per_second(kValue).bytes_per_second(), kValue);
  EXPECT_EQ(DataRate::bits_per_second(kValue).bits_per_second(), kValue);
  EXPECT_EQ(DataRate::bps(kValue).bps(), kValue);
  EXPECT_EQ(DataRate::kbps(kValue).kbps(), kValue);
}

TEST(DataRateTest, GetDifferentPrefix) {
  const int64_t kValue = 123 * 8000;
  EXPECT_EQ(DataRate::bytes_per_second(kValue).bps(), kValue * 8);
  EXPECT_EQ(DataRate::bits_per_second(kValue).bytes_per_second(), kValue / 8);
  EXPECT_EQ(DataRate::bps(kValue).kbps(), kValue / 1000);
}

TEST(DataRateTest, IdentityChecks) {
  const int64_t kValue = 3000;
  EXPECT_TRUE(DataRate::Zero().IsZero());
  EXPECT_FALSE(DataRate::bytes_per_second(kValue).IsZero());

  EXPECT_TRUE(DataRate::Infinity().IsInfinite());
  EXPECT_FALSE(DataRate::Zero().IsInfinite());
  EXPECT_FALSE(DataRate::bytes_per_second(kValue).IsInfinite());

  EXPECT_FALSE(DataRate::Infinity().IsFinite());
  EXPECT_TRUE(DataRate::bytes_per_second(kValue).IsFinite());
  EXPECT_TRUE(DataRate::Zero().IsFinite());
}

TEST(DataRateTest, ComparisonOperators) {
  const int64_t kSmall = 450;
  const int64_t kLarge = 451;
  const DataRate small = DataRate::bytes_per_second(kSmall);
  const DataRate large = DataRate::bytes_per_second(kLarge);

  EXPECT_EQ(DataRate::Zero(), DataRate::bps(0));
  EXPECT_EQ(DataRate::Infinity(), DataRate::Infinity());
  EXPECT_EQ(small, small);
  EXPECT_LE(small, small);
  EXPECT_GE(small, small);
  EXPECT_NE(small, large);
  EXPECT_LE(small, large);
  EXPECT_LT(small, large);
  EXPECT_GE(large, small);
  EXPECT_GT(large, small);
  EXPECT_LT(DataRate::Zero(), small);
  EXPECT_GT(DataRate::Infinity(), large);
}

TEST(DataRateTest, MathOperations) {
  const int64_t kValueA = 450;
  const int64_t kValueB = 267;
  const DataRate size_a = DataRate::bytes_per_second(kValueA);
  const int32_t kInt32Value = 123;
  const double kFloatValue = 123.0;
  EXPECT_EQ((size_a * kValueB).bytes_per_second(), kValueA * kValueB);
  EXPECT_EQ((size_a * kInt32Value).bytes_per_second(), kValueA * kInt32Value);
  EXPECT_EQ((size_a * kFloatValue).bytes_per_second(), kValueA * kFloatValue);
}
}  // namespace test
}  // namespace webrtc
