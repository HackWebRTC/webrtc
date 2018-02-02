/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/network_control/include/network_units.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

TEST(TimeDeltaTest, GetBackSameValues) {
  const int64_t kValue = 499;
  for (int sign = -1; sign <= 1; ++sign) {
    int64_t value = kValue * sign;
    EXPECT_EQ(TimeDelta::ms(value).ms(), value);
    EXPECT_EQ(TimeDelta::us(value).us(), value);
    EXPECT_EQ(TimeDelta::s(value).s(), value);
    EXPECT_EQ(TimeDelta::seconds(value).s(), value);
  }
  EXPECT_EQ(TimeDelta::Zero().us(), 0);
}

TEST(TimeDeltaTest, GetDifferentPrefix) {
  const int64_t kValue = 3000000;
  EXPECT_EQ(TimeDelta::us(kValue).s(), kValue / 1000000);
  EXPECT_EQ(TimeDelta::ms(kValue).s(), kValue / 1000);
  EXPECT_EQ(TimeDelta::us(kValue).ms(), kValue / 1000);

  EXPECT_EQ(TimeDelta::ms(kValue).us(), kValue * 1000);
  EXPECT_EQ(TimeDelta::s(kValue).ms(), kValue * 1000);
  EXPECT_EQ(TimeDelta::s(kValue).us(), kValue * 1000000);
}

TEST(TimeDeltaTest, IdentityChecks) {
  const int64_t kValue = 3000;
  EXPECT_TRUE(TimeDelta::Zero().IsZero());
  EXPECT_FALSE(TimeDelta::ms(kValue).IsZero());

  EXPECT_TRUE(TimeDelta::Infinity().IsInfinite());
  EXPECT_TRUE(TimeDelta::kPlusInfinity.IsInfinite());
  EXPECT_TRUE(TimeDelta::kMinusInfinity.IsInfinite());
  EXPECT_FALSE(TimeDelta::Zero().IsInfinite());
  EXPECT_FALSE(TimeDelta::ms(-kValue).IsInfinite());
  EXPECT_FALSE(TimeDelta::ms(kValue).IsInfinite());

  EXPECT_FALSE(TimeDelta::Infinity().IsFinite());
  EXPECT_FALSE(TimeDelta::kPlusInfinity.IsFinite());
  EXPECT_FALSE(TimeDelta::kMinusInfinity.IsFinite());
  EXPECT_TRUE(TimeDelta::ms(-kValue).IsFinite());
  EXPECT_TRUE(TimeDelta::ms(kValue).IsFinite());
  EXPECT_TRUE(TimeDelta::Zero().IsFinite());
}

TEST(TimeDeltaTest, ComparisonOperators) {
  const int64_t kSmall = 450;
  const int64_t kLarge = 451;
  const TimeDelta small = TimeDelta::ms(kSmall);
  const TimeDelta large = TimeDelta::ms(kLarge);

  EXPECT_EQ(TimeDelta::Zero(), TimeDelta::Zero());
  EXPECT_EQ(TimeDelta::Infinity(), TimeDelta::Infinity());
  EXPECT_EQ(small, TimeDelta::ms(kSmall));
  EXPECT_LE(small, TimeDelta::ms(kSmall));
  EXPECT_GE(small, TimeDelta::ms(kSmall));
  EXPECT_NE(small, TimeDelta::ms(kLarge));
  EXPECT_LE(small, TimeDelta::ms(kLarge));
  EXPECT_LT(small, TimeDelta::ms(kLarge));
  EXPECT_GE(large, TimeDelta::ms(kSmall));
  EXPECT_GT(large, TimeDelta::ms(kSmall));
  EXPECT_LT(TimeDelta::kZero, small);
  EXPECT_GT(TimeDelta::kZero, TimeDelta::ms(-kSmall));
  EXPECT_GT(TimeDelta::kZero, TimeDelta::ms(-kSmall));

  EXPECT_GT(TimeDelta::kPlusInfinity, large);
  EXPECT_LT(TimeDelta::kMinusInfinity, TimeDelta::kZero);
}

TEST(TimeDeltaTest, MathOperations) {
  const int64_t kValueA = 267;
  const int64_t kValueB = 450;
  const TimeDelta delta_a = TimeDelta::ms(kValueA);
  const TimeDelta delta_b = TimeDelta::ms(kValueB);
  EXPECT_EQ((delta_a + delta_b).ms(), kValueA + kValueB);
  EXPECT_EQ((delta_a - delta_b).ms(), kValueA - kValueB);

  const int32_t kInt32Value = 123;
  const double kFloatValue = 123.0;
  EXPECT_EQ((TimeDelta::us(kValueA) * kValueB).us(), kValueA * kValueB);
  EXPECT_EQ((TimeDelta::us(kValueA) * kInt32Value).us(), kValueA * kInt32Value);
  EXPECT_EQ((TimeDelta::us(kValueA) * kFloatValue).us(), kValueA * kFloatValue);

  EXPECT_EQ(TimeDelta::us(-kValueA).Abs().us(), kValueA);
  EXPECT_EQ(TimeDelta::us(kValueA).Abs().us(), kValueA);
}

TEST(TimestampTest, GetBackSameValues) {
  const int64_t kValue = 499;
  EXPECT_EQ(Timestamp::ms(kValue).ms(), kValue);
  EXPECT_EQ(Timestamp::us(kValue).us(), kValue);
  EXPECT_EQ(Timestamp::s(kValue).s(), kValue);
}

TEST(TimestampTest, GetDifferentPrefix) {
  const int64_t kValue = 3000000;
  EXPECT_EQ(Timestamp::us(kValue).s(), kValue / 1000000);
  EXPECT_EQ(Timestamp::ms(kValue).s(), kValue / 1000);
  EXPECT_EQ(Timestamp::us(kValue).ms(), kValue / 1000);

  EXPECT_EQ(Timestamp::ms(kValue).us(), kValue * 1000);
  EXPECT_EQ(Timestamp::s(kValue).ms(), kValue * 1000);
  EXPECT_EQ(Timestamp::s(kValue).us(), kValue * 1000000);
}

TEST(TimestampTest, IdentityChecks) {
  const int64_t kValue = 3000;

  EXPECT_TRUE(Timestamp::Infinity().IsInfinite());
  EXPECT_FALSE(Timestamp::ms(kValue).IsInfinite());

  EXPECT_FALSE(Timestamp::kNotInitialized.IsFinite());
  EXPECT_FALSE(Timestamp::Infinity().IsFinite());
  EXPECT_TRUE(Timestamp::ms(kValue).IsFinite());
}

TEST(TimestampTest, ComparisonOperators) {
  const int64_t kSmall = 450;
  const int64_t kLarge = 451;

  EXPECT_EQ(Timestamp::Infinity(), Timestamp::Infinity());
  EXPECT_EQ(Timestamp::ms(kSmall), Timestamp::ms(kSmall));
  EXPECT_LE(Timestamp::ms(kSmall), Timestamp::ms(kSmall));
  EXPECT_GE(Timestamp::ms(kSmall), Timestamp::ms(kSmall));
  EXPECT_NE(Timestamp::ms(kSmall), Timestamp::ms(kLarge));
  EXPECT_LE(Timestamp::ms(kSmall), Timestamp::ms(kLarge));
  EXPECT_LT(Timestamp::ms(kSmall), Timestamp::ms(kLarge));
  EXPECT_GE(Timestamp::ms(kLarge), Timestamp::ms(kSmall));
  EXPECT_GT(Timestamp::ms(kLarge), Timestamp::ms(kSmall));
}

TEST(UnitConversionTest, TimestampAndTimeDeltaMath) {
  const int64_t kValueA = 267;
  const int64_t kValueB = 450;
  const Timestamp time_a = Timestamp::ms(kValueA);
  const Timestamp time_b = Timestamp::ms(kValueB);
  const TimeDelta delta_a = TimeDelta::ms(kValueA);

  EXPECT_EQ((time_a - time_b), TimeDelta::ms(kValueA - kValueB));
  EXPECT_EQ((time_b - delta_a), Timestamp::ms(kValueB - kValueA));
  EXPECT_EQ((time_b + delta_a), Timestamp::ms(kValueB + kValueA));
}

TEST(DataSizeTest, GetBackSameValues) {
  const int64_t kValue = 123 * 8;
  EXPECT_EQ(DataSize::bytes(kValue).bytes(), kValue);
  EXPECT_EQ(DataSize::bits(kValue).bits(), kValue);
}

TEST(DataSizeTest, GetDifferentPrefix) {
  const int64_t kValue = 123 * 8000;
  EXPECT_EQ(DataSize::bytes(kValue).bits(), kValue * 8);
  EXPECT_EQ(DataSize::bits(kValue).bytes(), kValue / 8);
  EXPECT_EQ(DataSize::bits(kValue).kilobits(), kValue / 1000);
  EXPECT_EQ(DataSize::bytes(kValue).kilobytes(), kValue / 1000);
}

TEST(DataSizeTest, IdentityChecks) {
  const int64_t kValue = 3000;
  EXPECT_TRUE(DataSize::Zero().IsZero());
  EXPECT_FALSE(DataSize::bytes(kValue).IsZero());

  EXPECT_TRUE(DataSize::Infinity().IsInfinite());
  EXPECT_TRUE(DataSize::kPlusInfinity.IsInfinite());
  EXPECT_FALSE(DataSize::Zero().IsInfinite());
  EXPECT_FALSE(DataSize::bytes(kValue).IsInfinite());

  EXPECT_FALSE(DataSize::Infinity().IsFinite());
  EXPECT_FALSE(DataSize::kPlusInfinity.IsFinite());
  EXPECT_TRUE(DataSize::bytes(kValue).IsFinite());
  EXPECT_TRUE(DataSize::Zero().IsFinite());
}

TEST(DataSizeTest, ComparisonOperators) {
  const int64_t kSmall = 450;
  const int64_t kLarge = 451;
  const DataSize small = DataSize::bytes(kSmall);
  const DataSize large = DataSize::bytes(kLarge);

  EXPECT_EQ(DataSize::Zero(), DataSize::Zero());
  EXPECT_EQ(DataSize::Infinity(), DataSize::Infinity());
  EXPECT_EQ(small, small);
  EXPECT_LE(small, small);
  EXPECT_GE(small, small);
  EXPECT_NE(small, large);
  EXPECT_LE(small, large);
  EXPECT_LT(small, large);
  EXPECT_GE(large, small);
  EXPECT_GT(large, small);
  EXPECT_LT(DataSize::kZero, small);

  EXPECT_GT(DataSize::kPlusInfinity, large);
}

TEST(DataSizeTest, MathOperations) {
  const int64_t kValueA = 450;
  const int64_t kValueB = 267;
  const DataSize size_a = DataSize::bytes(kValueA);
  const DataSize size_b = DataSize::bytes(kValueB);
  EXPECT_EQ((size_a + size_b).bytes(), kValueA + kValueB);
  EXPECT_EQ((size_a - size_b).bytes(), kValueA - kValueB);

  const int32_t kInt32Value = 123;
  const double kFloatValue = 123.0;
  EXPECT_EQ((size_a * kValueB).bytes(), kValueA * kValueB);
  EXPECT_EQ((size_a * kInt32Value).bytes(), kValueA * kInt32Value);
  EXPECT_EQ((size_a * kFloatValue).bytes(), kValueA * kFloatValue);

  EXPECT_EQ((size_a / 10).bytes(), kValueA / 10);

  DataSize mutable_size = DataSize::bytes(kValueA);
  mutable_size += size_b;
  EXPECT_EQ(mutable_size.bytes(), kValueA + kValueB);
  mutable_size -= size_a;
  EXPECT_EQ(mutable_size.bytes(), kValueB);
}

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
  EXPECT_TRUE(DataRate::kPlusInfinity.IsInfinite());
  EXPECT_FALSE(DataRate::Zero().IsInfinite());
  EXPECT_FALSE(DataRate::bytes_per_second(kValue).IsInfinite());

  EXPECT_FALSE(DataRate::Infinity().IsFinite());
  EXPECT_FALSE(DataRate::kPlusInfinity.IsFinite());
  EXPECT_TRUE(DataRate::bytes_per_second(kValue).IsFinite());
  EXPECT_TRUE(DataRate::Zero().IsFinite());
}

TEST(DataRateTest, ComparisonOperators) {
  const int64_t kSmall = 450;
  const int64_t kLarge = 451;
  const DataRate small = DataRate::bytes_per_second(kSmall);
  const DataRate large = DataRate::bytes_per_second(kLarge);

  EXPECT_EQ(DataRate::Zero(), DataRate::Zero());
  EXPECT_EQ(DataRate::Infinity(), DataRate::Infinity());
  EXPECT_EQ(small, small);
  EXPECT_LE(small, small);
  EXPECT_GE(small, small);
  EXPECT_NE(small, large);
  EXPECT_LE(small, large);
  EXPECT_LT(small, large);
  EXPECT_GE(large, small);
  EXPECT_GT(large, small);
  EXPECT_LT(DataRate::kZero, small);
  EXPECT_GT(DataRate::kPlusInfinity, large);
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

TEST(UnitConversionTest, DataRateAndDataSizeAndTimeDelta) {
  const int64_t kValueA = 5;
  const int64_t kValueB = 450;
  const int64_t kValueC = 45000;
  const TimeDelta delta_a = TimeDelta::seconds(kValueA);
  const DataRate rate_b = DataRate::bytes_per_second(kValueB);
  const DataSize size_c = DataSize::bytes(kValueC);
  EXPECT_EQ((delta_a * rate_b).bytes(), kValueA * kValueB);
  EXPECT_EQ((size_c / delta_a).bytes_per_second(), kValueC / kValueA);
  EXPECT_EQ((size_c / rate_b).s(), kValueC / kValueB);
}

}  // namespace test
}  // namespace webrtc
