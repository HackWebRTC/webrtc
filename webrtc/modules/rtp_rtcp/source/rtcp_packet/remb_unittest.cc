/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/remb.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/rtcp_packet_parser.h"

using webrtc::rtcp::RawPacket;
using webrtc::rtcp::Remb;
using webrtc::test::RtcpPacketParser;

namespace webrtc {

const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;

TEST(RtcpPacketRembTest, Remb) {
  Remb remb;
  remb.From(kSenderSsrc);
  remb.AppliesTo(kRemoteSsrc);
  remb.AppliesTo(kRemoteSsrc + 1);
  remb.AppliesTo(kRemoteSsrc + 2);
  remb.WithBitrateBps(261011);

  rtc::scoped_ptr<RawPacket> packet(remb.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.psfb_app()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.psfb_app()->Ssrc());
  EXPECT_EQ(1, parser.remb_item()->num_packets());
  EXPECT_EQ(261011, parser.remb_item()->last_bitrate_bps());
  std::vector<uint32_t> ssrcs = parser.remb_item()->last_ssrc_list();
  EXPECT_EQ(kRemoteSsrc, ssrcs[0]);
  EXPECT_EQ(kRemoteSsrc + 1, ssrcs[1]);
  EXPECT_EQ(kRemoteSsrc + 2, ssrcs[2]);
}
}  // namespace webrtc
