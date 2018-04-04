/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/fec_private_tables_bursty.h"
#include "modules/rtp_rtcp/source/fec_private_tables_random.h"
#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"

#include "test/gtest.h"

namespace webrtc {
namespace fec_private_tables {

using internal::LookUpInFecTable;

TEST(FecTable, TestBurstyLookup) {
  rtc::ArrayView<const uint8_t> result;
  result = LookUpInFecTable(&kPacketMaskBurstyTbl[0], 0, 0);
  // Should match kMaskBursty1_1.
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(0x80u, result[0]);

  result = LookUpInFecTable(&kPacketMaskBurstyTbl[0], 3, 0);
  // Should match kMaskBursty4_1.
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(0xf0u, result[0]);
  EXPECT_EQ(0x00u, result[1]);

  result = LookUpInFecTable(&kPacketMaskBurstyTbl[0], 1, 1);
  // Should match kMaskBursty2_2.
  EXPECT_EQ(4u, result.size());
  EXPECT_EQ(0x80u, result[0]);
  EXPECT_EQ(0xc0u, result[2]);

  result = LookUpInFecTable(&kPacketMaskBurstyTbl[0], 11, 11);
  // Should match kMaskBursty12_12.
  EXPECT_EQ(24u, result.size());
  EXPECT_EQ(0x80u, result[0]);
  EXPECT_EQ(0x30u, result[23]);
}

TEST(FecTable, TestRandomLookup) {
  rtc::ArrayView<const uint8_t> result;
  result = LookUpInFecTable(&kPacketMaskRandomTbl[0], 0, 0);
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(0x80u, result[0]);
  EXPECT_EQ(0x00u, result[1]);

  result = LookUpInFecTable(&kPacketMaskRandomTbl[0], 4, 1);
  // kMaskRandom5_2.
  EXPECT_EQ(4u, result.size());
  EXPECT_EQ(0xa8u, result[0]);
  EXPECT_EQ(0xd0u, result[2]);

  result = LookUpInFecTable(&kPacketMaskRandomTbl[0], 16, 0);
  // kMaskRandom17_1.
  EXPECT_EQ(6u, result.size());
  EXPECT_EQ(0xffu, result[0]);
  EXPECT_EQ(0x00u, result[5]);

  result = LookUpInFecTable(&kPacketMaskRandomTbl[0], 47, 47);
  // kMaskRandom48_48.
  EXPECT_EQ(6u * 48, result.size());
  EXPECT_EQ(0x10u, result[0]);
  EXPECT_EQ(0x02u, result[6]);
}

}  // namespace fec_private_tables
}  // namespace webrtc
