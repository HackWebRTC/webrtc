/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/fir.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/rtcp_packet_parser.h"

using webrtc::rtcp::Fir;
using webrtc::rtcp::RawPacket;
using webrtc::test::RtcpPacketParser;

namespace webrtc {

const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;

TEST(RtcpPacketFirTest, Fir) {
  Fir fir;
  fir.From(kSenderSsrc);
  fir.To(kRemoteSsrc);
  fir.WithCommandSeqNum(123);

  rtc::scoped_ptr<RawPacket> packet(fir.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.fir()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.fir()->Ssrc());
  EXPECT_EQ(1, parser.fir_item()->num_packets());
  EXPECT_EQ(kRemoteSsrc, parser.fir_item()->Ssrc());
  EXPECT_EQ(123U, parser.fir_item()->SeqNum());
}

}  // namespace webrtc
