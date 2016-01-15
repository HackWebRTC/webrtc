/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 * This file includes unit tests for the RtcpPacket.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/app.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "webrtc/test/rtcp_packet_parser.h"

using ::testing::ElementsAre;

using webrtc::rtcp::App;
using webrtc::rtcp::Bye;
using webrtc::rtcp::RawPacket;
using webrtc::rtcp::ReceiverReport;
using webrtc::rtcp::ReportBlock;
using webrtc::rtcp::Rpsi;
using webrtc::rtcp::Sdes;
using webrtc::rtcp::SenderReport;
using webrtc::test::RtcpPacketParser;

namespace webrtc {

const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;

TEST(RtcpPacketTest, Sr) {
  SenderReport sr;
  sr.From(kSenderSsrc);
  sr.WithNtpSec(0x11111111);
  sr.WithNtpFrac(0x22222222);
  sr.WithRtpTimestamp(0x33333333);
  sr.WithPacketCount(0x44444444);
  sr.WithOctetCount(0x55555555);

  rtc::scoped_ptr<RawPacket> packet(sr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());

  EXPECT_EQ(1, parser.sender_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sender_report()->Ssrc());
  EXPECT_EQ(0x11111111U, parser.sender_report()->NtpSec());
  EXPECT_EQ(0x22222222U, parser.sender_report()->NtpFrac());
  EXPECT_EQ(0x33333333U, parser.sender_report()->RtpTimestamp());
  EXPECT_EQ(0x44444444U, parser.sender_report()->PacketCount());
  EXPECT_EQ(0x55555555U, parser.sender_report()->OctetCount());
  EXPECT_EQ(0, parser.report_block()->num_packets());
}

TEST(RtcpPacketTest, SrWithOneReportBlock) {
  ReportBlock rb;
  rb.To(kRemoteSsrc);

  SenderReport sr;
  sr.From(kSenderSsrc);
  EXPECT_TRUE(sr.WithReportBlock(rb));

  rtc::scoped_ptr<RawPacket> packet(sr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.sender_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sender_report()->Ssrc());
  EXPECT_EQ(1, parser.report_block()->num_packets());
  EXPECT_EQ(kRemoteSsrc, parser.report_block()->Ssrc());
}

TEST(RtcpPacketTest, SrWithTwoReportBlocks) {
  ReportBlock rb1;
  rb1.To(kRemoteSsrc);
  ReportBlock rb2;
  rb2.To(kRemoteSsrc + 1);

  SenderReport sr;
  sr.From(kSenderSsrc);
  EXPECT_TRUE(sr.WithReportBlock(rb1));
  EXPECT_TRUE(sr.WithReportBlock(rb2));

  rtc::scoped_ptr<RawPacket> packet(sr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.sender_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sender_report()->Ssrc());
  EXPECT_EQ(2, parser.report_block()->num_packets());
  EXPECT_EQ(1, parser.report_blocks_per_ssrc(kRemoteSsrc));
  EXPECT_EQ(1, parser.report_blocks_per_ssrc(kRemoteSsrc + 1));
}

TEST(RtcpPacketTest, SrWithTooManyReportBlocks) {
  SenderReport sr;
  sr.From(kSenderSsrc);
  const int kMaxReportBlocks = (1 << 5) - 1;
  ReportBlock rb;
  for (int i = 0; i < kMaxReportBlocks; ++i) {
    rb.To(kRemoteSsrc + i);
    EXPECT_TRUE(sr.WithReportBlock(rb));
  }
  rb.To(kRemoteSsrc + kMaxReportBlocks);
  EXPECT_FALSE(sr.WithReportBlock(rb));
}

TEST(RtcpPacketTest, AppWithNoData) {
  App app;
  app.WithSubType(30);
  uint32_t name = 'n' << 24;
  name += 'a' << 16;
  name += 'm' << 8;
  name += 'e';
  app.WithName(name);

  rtc::scoped_ptr<RawPacket> packet(app.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.app()->num_packets());
  EXPECT_EQ(30U, parser.app()->SubType());
  EXPECT_EQ(name, parser.app()->Name());
  EXPECT_EQ(0, parser.app_item()->num_packets());
}

TEST(RtcpPacketTest, App) {
  App app;
  app.From(kSenderSsrc);
  app.WithSubType(30);
  uint32_t name = 'n' << 24;
  name += 'a' << 16;
  name += 'm' << 8;
  name += 'e';
  app.WithName(name);
  const char kData[] = {'t', 'e', 's', 't', 'd', 'a', 't', 'a'};
  const size_t kDataLength = sizeof(kData) / sizeof(kData[0]);
  app.WithData((const uint8_t*)kData, kDataLength);

  rtc::scoped_ptr<RawPacket> packet(app.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.app()->num_packets());
  EXPECT_EQ(30U, parser.app()->SubType());
  EXPECT_EQ(name, parser.app()->Name());
  EXPECT_EQ(1, parser.app_item()->num_packets());
  EXPECT_EQ(kDataLength, parser.app_item()->DataLength());
  EXPECT_EQ(0, strncmp(kData, (const char*)parser.app_item()->Data(),
      parser.app_item()->DataLength()));
}

TEST(RtcpPacketTest, SdesWithOneChunk) {
  Sdes sdes;
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc, "alice@host"));

  rtc::scoped_ptr<RawPacket> packet(sdes.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.sdes()->num_packets());
  EXPECT_EQ(1, parser.sdes_chunk()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sdes_chunk()->Ssrc());
  EXPECT_EQ("alice@host", parser.sdes_chunk()->Cname());
}

TEST(RtcpPacketTest, SdesWithMultipleChunks) {
  Sdes sdes;
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc, "a"));
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc + 1, "ab"));
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc + 2, "abc"));
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc + 3, "abcd"));
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc + 4, "abcde"));
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc + 5, "abcdef"));

  rtc::scoped_ptr<RawPacket> packet(sdes.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.sdes()->num_packets());
  EXPECT_EQ(6, parser.sdes_chunk()->num_packets());
  EXPECT_EQ(kSenderSsrc + 5, parser.sdes_chunk()->Ssrc());
  EXPECT_EQ("abcdef", parser.sdes_chunk()->Cname());
}

TEST(RtcpPacketTest, SdesWithTooManyChunks) {
  Sdes sdes;
  const int kMaxChunks = (1 << 5) - 1;
  for (int i = 0; i < kMaxChunks; ++i) {
    uint32_t ssrc = kSenderSsrc + i;
    std::ostringstream oss;
    oss << "cname" << i;
    EXPECT_TRUE(sdes.WithCName(ssrc, oss.str()));
  }
  EXPECT_FALSE(sdes.WithCName(kSenderSsrc + kMaxChunks, "foo"));
}

TEST(RtcpPacketTest, CnameItemWithEmptyString) {
  Sdes sdes;
  EXPECT_TRUE(sdes.WithCName(kSenderSsrc, ""));

  rtc::scoped_ptr<RawPacket> packet(sdes.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.sdes()->num_packets());
  EXPECT_EQ(1, parser.sdes_chunk()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sdes_chunk()->Ssrc());
  EXPECT_EQ("", parser.sdes_chunk()->Cname());
}

TEST(RtcpPacketTest, Rpsi) {
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

TEST(RtcpPacketTest, RpsiWithTwoByteNativeString) {
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

TEST(RtcpPacketTest, RpsiWithThreeByteNativeString) {
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

TEST(RtcpPacketTest, RpsiWithFourByteNativeString) {
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

TEST(RtcpPacketTest, RpsiWithMaxPictureId) {
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

TEST(RtcpPacketTest, BuildWithTooSmallBuffer) {
  ReportBlock rb;
  ReceiverReport rr;
  rr.From(kSenderSsrc);
  EXPECT_TRUE(rr.WithReportBlock(rb));

  const size_t kRrLength = 8;
  const size_t kReportBlockLength = 24;

  // No packet.
  class Verifier : public rtcp::RtcpPacket::PacketReadyCallback {
    void OnPacketReady(uint8_t* data, size_t length) override {
      ADD_FAILURE() << "Packet should not fit within max size.";
    }
  } verifier;
  const size_t kBufferSize = kRrLength + kReportBlockLength - 1;
  uint8_t buffer[kBufferSize];
  EXPECT_FALSE(rr.BuildExternalBuffer(buffer, kBufferSize, &verifier));
}
}  // namespace webrtc
