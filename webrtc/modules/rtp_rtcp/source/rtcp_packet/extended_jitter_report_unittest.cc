/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_jitter_report.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/rtcp_packet_parser.h"

using testing::ElementsAre;
using testing::IsEmpty;
using webrtc::rtcp::ExtendedJitterReport;

namespace webrtc {
namespace {
constexpr uint32_t kJitter1 = 0x11121314;
constexpr uint32_t kJitter2 = 0x22242628;
}  // namespace

TEST(RtcpPacketExtendedJitterReportTest, CreateAndParseWithoutItems) {
  ExtendedJitterReport ij;
  rtc::Buffer raw = ij.Build();

  ExtendedJitterReport parsed;
  EXPECT_TRUE(test::ParseSinglePacket(raw, &parsed));

  EXPECT_THAT(parsed.jitters(), IsEmpty());
}

TEST(RtcpPacketExtendedJitterReportTest, CreateAndParseWithOneItem) {
  ExtendedJitterReport ij;
  EXPECT_TRUE(ij.WithJitter(kJitter1));
  rtc::Buffer raw = ij.Build();

  ExtendedJitterReport parsed;
  EXPECT_TRUE(test::ParseSinglePacket(raw, &parsed));

  EXPECT_THAT(parsed.jitters(), ElementsAre(kJitter1));
}

TEST(RtcpPacketExtendedJitterReportTest, CreateAndParseWithTwoItems) {
  ExtendedJitterReport ij;
  EXPECT_TRUE(ij.WithJitter(kJitter1));
  EXPECT_TRUE(ij.WithJitter(kJitter2));
  rtc::Buffer raw = ij.Build();

  ExtendedJitterReport parsed;
  EXPECT_TRUE(test::ParseSinglePacket(raw, &parsed));

  EXPECT_THAT(parsed.jitters(), ElementsAre(kJitter1, kJitter2));
}

TEST(RtcpPacketExtendedJitterReportTest, CreateWithTooManyItems) {
  ExtendedJitterReport ij;
  const int kMaxIjItems = (1 << 5) - 1;
  for (int i = 0; i < kMaxIjItems; ++i) {
    EXPECT_TRUE(ij.WithJitter(i));
  }
  EXPECT_FALSE(ij.WithJitter(kMaxIjItems));
}

TEST(RtcpPacketExtendedJitterReportTest, ParseFailsWithTooManyItems) {
  ExtendedJitterReport ij;
  ij.WithJitter(kJitter1);
  rtc::Buffer raw = ij.Build();
  raw[0]++;  // Damage packet: increase jitter count by 1.
  ExtendedJitterReport parsed;
  EXPECT_FALSE(test::ParseSinglePacket(raw, &parsed));
}

}  // namespace webrtc
