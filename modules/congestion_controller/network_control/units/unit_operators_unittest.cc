/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/network_control/units/unit_operators.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

TEST(UnitConversionTest, DataRateAndDataSizeAndTimeDelta) {
  const int64_t kValueA = 5;
  const int64_t kValueB = 450;
  const int64_t kValueC = 45000;
  const TimeDelta delta_a = TimeDelta::seconds(kValueA);
  const DataRate rate_b = DataRate::bytes_per_second(kValueB);
  const DataSize size_c = DataSize::bytes(kValueC);
  EXPECT_EQ((delta_a * rate_b).bytes(), kValueA * kValueB);
  EXPECT_EQ((rate_b * delta_a).bytes(), kValueA * kValueB);
  EXPECT_EQ((size_c / delta_a).bytes_per_second(), kValueC / kValueA);
  EXPECT_EQ((size_c / rate_b).s(), kValueC / kValueB);
}

}  // namespace test
}  // namespace webrtc
