/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <typedefs.h>

#include "gtest/gtest.h"
#include "modules/audio_coding/codecs/iSAC/fix/source/lpc_masking_model.h"

class IsacUnitTest : public testing::Test {
};

TEST_F(IsacUnitTest, CalculateResidualEnergyTest) {
  const int kIntOrder = 10;
  const int32_t kInt32QDomain = 5;
  const int kIntShift = 11;
  int16_t a[kIntOrder + 1] = {32760, 122, 7, 0, -32760, -3958,
      -48, 18745, 498, 9, 23456};
  int32_t corr[kIntOrder + 1] = {11443647, -27495, 0,
      98745, -11443600, 1, 1, 498, 9, 888, 23456};
  int q_shift_residual = 0;
  int32_t residual_energy = 0;

  residual_energy = WebRtcIsacfix_CalculateResidualEnergy(kIntOrder,
      kInt32QDomain, kIntShift, a, corr, &q_shift_residual);
  EXPECT_EQ(1789023310, residual_energy);
  EXPECT_EQ(2, q_shift_residual);
}
