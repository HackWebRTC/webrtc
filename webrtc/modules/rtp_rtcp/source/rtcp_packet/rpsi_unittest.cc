/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rpsi.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/test/rtcp_packet_parser.h"

using webrtc::rtcp::RawPacket;
using webrtc::rtcp::Rpsi;
using webrtc::test::RtcpPacketParser;

namespace webrtc {

TEST(RtcpPacketRpsiTest, WithOneByteNativeString) {
  Rpsi rpsi;
  // 1000001 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x41;
  const uint16_t kNumberOfValidBytes = 1;
  rpsi.WithPayloadType(100);
  rpsi.WithPictureId(kPictureId);

  rtc::scoped_ptr<RawPacket> packet(rpsi.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(100, parser.rpsi()->PayloadType());
  EXPECT_EQ(kNumberOfValidBytes * 8, parser.rpsi()->NumberOfValidBits());
  EXPECT_EQ(kPictureId, parser.rpsi()->PictureId());
}

TEST(RtcpPacketRpsiTest, WithTwoByteNativeString) {
  Rpsi rpsi;
  // |1 0000001 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x81;
  const uint16_t kNumberOfValidBytes = 2;
  rpsi.WithPictureId(kPictureId);

  rtc::scoped_ptr<RawPacket> packet(rpsi.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(kNumberOfValidBytes * 8, parser.rpsi()->NumberOfValidBits());
  EXPECT_EQ(kPictureId, parser.rpsi()->PictureId());
}

TEST(RtcpPacketRpsiTest, WithThreeByteNativeString) {
  Rpsi rpsi;
  // 10000|00 100000|0 1000000 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x102040;
  const uint16_t kNumberOfValidBytes = 3;
  rpsi.WithPictureId(kPictureId);

  rtc::scoped_ptr<RawPacket> packet(rpsi.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(kNumberOfValidBytes * 8, parser.rpsi()->NumberOfValidBits());
  EXPECT_EQ(kPictureId, parser.rpsi()->PictureId());
}

TEST(RtcpPacketRpsiTest, WithFourByteNativeString) {
  Rpsi rpsi;
  // 1000|001 00001|01 100001|1 1000010 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x84161C2;
  const uint16_t kNumberOfValidBytes = 4;
  rpsi.WithPictureId(kPictureId);

  rtc::scoped_ptr<RawPacket> packet(rpsi.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(kNumberOfValidBytes * 8, parser.rpsi()->NumberOfValidBits());
  EXPECT_EQ(kPictureId, parser.rpsi()->PictureId());
}

TEST(RtcpPacketRpsiTest, WithMaxPictureId) {
  Rpsi rpsi;
  // 1 1111111| 1111111 1|111111 11|11111 111|1111 1111|111 11111|
  // 11 111111|1 1111111 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0xffffffffffffffff;
  const uint16_t kNumberOfValidBytes = 10;
  rpsi.WithPictureId(kPictureId);

  rtc::scoped_ptr<RawPacket> packet(rpsi.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(kNumberOfValidBytes * 8, parser.rpsi()->NumberOfValidBits());
  EXPECT_EQ(kPictureId, parser.rpsi()->PictureId());
}

}  // namespace webrtc
