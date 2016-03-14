/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/mod_ops.h"

namespace webrtc {
class TestModOps : public ::testing::Test {
 protected:
  // Can't use std::numeric_limits<unsigned long>::max() since
  // MSVC doesn't support constexpr.
  static const unsigned long ulmax = ~0ul;  // NOLINT
};

TEST_F(TestModOps, Add) {
  const int D = 100;
  ASSERT_EQ(1u, Add<D>(0, 1));
  ASSERT_EQ(0u, Add<D>(0, D));
  for (int i = 0; i < D; ++i)
    ASSERT_EQ(0u, Add<D>(i, D - i));

  int t = 37;
  uint8_t a = t;
  for (int i = 0; i < 256; ++i) {
    ASSERT_EQ(a, static_cast<uint8_t>(t));
    t = Add<256>(t, 1);
    ++a;
  }
}

TEST_F(TestModOps, AddLarge) {
  // NOLINTNEXTLINE
  const unsigned long D = ulmax - 10ul;  // NOLINT
  unsigned long l = D - 1ul;             // NOLINT
  ASSERT_EQ(D - 2ul, Add<D>(l, l));
  ASSERT_EQ(9ul, Add<D>(l, ulmax));
  ASSERT_EQ(10ul, Add<D>(0ul, ulmax));
}

TEST_F(TestModOps, Subtract) {
  const int D = 100;
  ASSERT_EQ(99u, Subtract<D>(0, 1));
  ASSERT_EQ(0u, Subtract<D>(0, D));
  for (int i = 0; i < D; ++i)
    ASSERT_EQ(0u, Subtract<D>(i, D + i));

  int t = 37;
  uint8_t a = t;
  for (int i = 0; i < 256; ++i) {
    ASSERT_EQ(a, static_cast<uint8_t>(t));
    t = Subtract<256>(t, 1);
    --a;
  }
}

TEST_F(TestModOps, SubtractLarge) {
  // NOLINTNEXTLINE
  const unsigned long D = ulmax - 10ul;  // NOLINT
  unsigned long l = D - 1ul;             // NOLINT
  ASSERT_EQ(0ul, Subtract<D>(l, l));
  ASSERT_EQ(D - 11ul, Subtract<D>(l, ulmax));
  ASSERT_EQ(D - 10ul, Subtract<D>(0ul, ulmax));
}

TEST_F(TestModOps, ForwardDiff) {
  ASSERT_EQ(0u, ForwardDiff(4711u, 4711u));

  uint8_t x = 0;
  uint8_t y = 255;
  for (int i = 0; i < 256; ++i) {
    ASSERT_EQ(255u, ForwardDiff(x, y));
    ++x;
    ++y;
  }

  int yi = 255;
  for (int i = 0; i < 256; ++i) {
    ASSERT_EQ(255u, ForwardDiff<uint8_t>(x, yi));
    ++x;
    ++yi;
  }
}

TEST_F(TestModOps, ReverseDiff) {
  ASSERT_EQ(0u, ReverseDiff(4711u, 4711u));

  uint8_t x = 0;
  uint8_t y = 255;
  for (int i = 0; i < 256; ++i) {
    ASSERT_EQ(1u, ReverseDiff(x, y));
    ++x;
    ++y;
  }

  int yi = 255;
  for (int i = 0; i < 256; ++i) {
    ASSERT_EQ(1u, ReverseDiff<uint8_t>(x, yi));
    ++x;
    ++yi;
  }
}

TEST_F(TestModOps, AheadOrAt) {
  uint8_t x = 0;
  uint8_t y = 0;
  ASSERT_TRUE(AheadOrAt(x, y));
  ++x;
  ASSERT_TRUE(AheadOrAt(x, y));
  ASSERT_FALSE(AheadOrAt(y, x));
  for (int i = 0; i < 256; ++i) {
    ASSERT_TRUE(AheadOrAt(x, y));
    ++x;
    ++y;
  }

  x = 128;
  y = 0;
  ASSERT_TRUE(AheadOrAt(x, y));
  ASSERT_FALSE(AheadOrAt(y, x));

  x = 129;
  ASSERT_FALSE(AheadOrAt(x, y));
  ASSERT_TRUE(AheadOrAt(y, x));
  ASSERT_TRUE(AheadOrAt<uint16_t>(x, y));
  ASSERT_FALSE(AheadOrAt<uint16_t>(y, x));
}

TEST_F(TestModOps, AheadOf) {
  uint8_t x = 0;
  uint8_t y = 0;
  ASSERT_FALSE(AheadOf(x, y));
  ++x;
  ASSERT_TRUE(AheadOf(x, y));
  ASSERT_FALSE(AheadOf(y, x));
  for (int i = 0; i < 256; ++i) {
    ASSERT_TRUE(AheadOf(x, y));
    ++x;
    ++y;
  }

  x = 128;
  y = 0;
  for (int i = 0; i < 128; ++i) {
    ASSERT_TRUE(AheadOf(x, y));
    ASSERT_FALSE(AheadOf(y, x));
    x++;
    y++;
  }

  for (int i = 0; i < 128; ++i) {
    ASSERT_FALSE(AheadOf(x, y));
    ASSERT_TRUE(AheadOf(y, x));
    x++;
    y++;
  }

  x = 129;
  y = 0;
  ASSERT_FALSE(AheadOf(x, y));
  ASSERT_TRUE(AheadOf(y, x));
  ASSERT_TRUE(AheadOf<uint16_t>(x, y));
  ASSERT_FALSE(AheadOf<uint16_t>(y, x));
}

TEST_F(TestModOps, ForwardDiffWithDivisor) {
  const uint8_t kDivisor = 211;

  for (uint8_t i = 0; i < kDivisor - 1; ++i) {
    ASSERT_EQ(0, (ForwardDiff<uint8_t, kDivisor>(i, i)));
    ASSERT_EQ(1, (ForwardDiff<uint8_t, kDivisor>(i, i + 1)));
    ASSERT_EQ(kDivisor - 1, (ForwardDiff<uint8_t, kDivisor>(i + 1, i)));
  }

  for (uint8_t i = 1; i < kDivisor; ++i) {
    ASSERT_EQ(i, (ForwardDiff<uint8_t, kDivisor>(0, i)));
    ASSERT_EQ(kDivisor - i, (ForwardDiff<uint8_t, kDivisor>(i, 0)));
  }
}

TEST_F(TestModOps, ReverseDiffWithDivisor) {
  const uint8_t kDivisor = 241;

  for (uint8_t i = 0; i < kDivisor - 1; ++i) {
    ASSERT_EQ(0, (ReverseDiff<uint8_t, kDivisor>(i, i)));
    ASSERT_EQ(kDivisor - 1, (ReverseDiff<uint8_t, kDivisor>(i, i + 1)));
    ASSERT_EQ(1, (ReverseDiff<uint8_t, kDivisor>(i + 1, i)));
  }

  for (uint8_t i = 1; i < kDivisor; ++i) {
    ASSERT_EQ(kDivisor - i, (ReverseDiff<uint8_t, kDivisor>(0, i)));
    ASSERT_EQ(i, (ReverseDiff<uint8_t, kDivisor>(i, 0)));
  }
}

}  // namespace webrtc
