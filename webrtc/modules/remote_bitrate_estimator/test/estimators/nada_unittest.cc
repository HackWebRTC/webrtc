/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <numeric>

#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet.h"
#include "webrtc/modules/remote_bitrate_estimator/test/estimators/nada.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_sender.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace testing {
namespace bwe {

class MedianFilterTest : public ::testing::Test {
 public:
  void FilterFromConstantArray() {
    for (int i = 0; i < kSize; v[i++] = 200) {
    }
    for (int i = 0; i < kSize; ++i) {
      int size = std::min(5, i + 1);
      m_filtered[i] = NadaBweReceiver::MedianFilter(v + i + 1 - size, size);
    }
  }

  void FilterFromIntermittentNoiseArray() {
    const int kValue = 500;
    const int kNoise = 100;

    for (int i = 0; i < kSize; i++) {
      v[i] = kValue + kNoise * (i % 10 == 9 ? 1 : 0);
    }
    for (int i = 0; i < kSize; ++i) {
      int size = std::min(5, i + 1);
      m_filtered[i] = NadaBweReceiver::MedianFilter(v + i + 1 - size, size);
      EXPECT_EQ(m_filtered[i], kValue);
    }
  }

 protected:
  static const int kSize = 1000;
  int64_t v[kSize];
  int64_t m_filtered[kSize];
};

class ExponentialSmoothingFilterTest : public ::testing::Test {
 public:
  void FilterFromConstantArray() {
    for (int i = 0; i < kSize; v[i++] = 200) {
    }
    exp_smoothed[0] =
        NadaBweReceiver::ExponentialSmoothingFilter(v[0], -1, kAlpha);

    for (int i = 1; i < kSize; ++i) {
      exp_smoothed[i] = NadaBweReceiver::ExponentialSmoothingFilter(
          v[i], exp_smoothed[i - 1], kAlpha);
    }
  }

 protected:
  static const int kSize = 1000;
  const float kAlpha = 0.8f;
  int64_t v[kSize];
  int64_t exp_smoothed[kSize];
};

TEST_F(MedianFilterTest, ConstantArray) {
  FilterFromConstantArray();
  for (int i = 0; i < kSize; ++i) {
    EXPECT_TRUE(m_filtered[i] == v[i]);
  }
}

TEST_F(MedianFilterTest, IntermittentNoise) {
  FilterFromIntermittentNoiseArray();
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
