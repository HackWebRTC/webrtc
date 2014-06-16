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

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/test/rtcp_packet_parser.h"

using webrtc::rtcp::App;
using webrtc::rtcp::Bye;
using webrtc::rtcp::Empty;
using webrtc::rtcp::Fir;
using webrtc::rtcp::Ij;
using webrtc::rtcp::Nack;
using webrtc::rtcp::Pli;
using webrtc::rtcp::Sdes;
using webrtc::rtcp::SenderReport;
using webrtc::rtcp::Sli;
using webrtc::rtcp::RawPacket;
using webrtc::rtcp::ReceiverReport;
using webrtc::rtcp::ReportBlock;
using webrtc::rtcp::Rpsi;
using webrtc::test::RtcpPacketParser;

namespace webrtc {

const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;

TEST(RtcpPacketTest, Rr) {
  ReceiverReport rr;
  rr.From(kSenderSsrc);

  RawPacket packet = rr.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.receiver_report()->Ssrc());
  EXPECT_EQ(0, parser.report_block()->num_packets());
}

TEST(RtcpPacketTest, RrWithOneReportBlock) {
  ReportBlock rb;
  rb.To(kRemoteSsrc);
  rb.WithFractionLost(55);
  rb.WithCumulativeLost(0x111111);
  rb.WithExtHighestSeqNum(0x22222222);
  rb.WithJitter(0x33333333);
  rb.WithLastSr(0x44444444);
  rb.WithDelayLastSr(0x55555555);

  ReceiverReport rr;
  rr.From(kSenderSsrc);
  rr.WithReportBlock(&rb);

  RawPacket packet = rr.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.receiver_report()->Ssrc());
  EXPECT_EQ(1, parser.report_block()->num_packets());
  EXPECT_EQ(kRemoteSsrc, parser.report_block()->Ssrc());
  EXPECT_EQ(55U, parser.report_block()->FractionLost());
  EXPECT_EQ(0x111111U, parser.report_block()->CumPacketLost());
  EXPECT_EQ(0x22222222U, parser.report_block()->ExtHighestSeqNum());
  EXPECT_EQ(0x33333333U, parser.report_block()->Jitter());
  EXPECT_EQ(0x44444444U, parser.report_block()->LastSr());
  EXPECT_EQ(0x55555555U, parser.report_block()->DelayLastSr());
}

TEST(RtcpPacketTest, RrWithTwoReportBlocks) {
  ReportBlock rb1;
  rb1.To(kRemoteSsrc);
  ReportBlock rb2;
  rb2.To(kRemoteSsrc + 1);

  ReceiverReport rr;
  rr.From(kSenderSsrc);
  rr.WithReportBlock(&rb1);
  rr.WithReportBlock(&rb2);

  RawPacket packet = rr.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.receiver_report()->Ssrc());
  EXPECT_EQ(2, parser.report_block()->num_packets());
  EXPECT_EQ(1, parser.report_blocks_per_ssrc(kRemoteSsrc));
  EXPECT_EQ(1, parser.report_blocks_per_ssrc(kRemoteSsrc + 1));
}

TEST(RtcpPacketTest, Sr) {
  SenderReport sr;
  sr.From(kSenderSsrc);
  sr.WithNtpSec(0x11111111);
  sr.WithNtpFrac(0x22222222);
  sr.WithRtpTimestamp(0x33333333);
  sr.WithPacketCount(0x44444444);
  sr.WithOctetCount(0x55555555);

  RawPacket packet = sr.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());

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
  sr.WithReportBlock(&rb);

  RawPacket packet = sr.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
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
  sr.WithReportBlock(&rb1);
  sr.WithReportBlock(&rb2);

  RawPacket packet = sr.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.sender_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sender_report()->Ssrc());
  EXPECT_EQ(2, parser.report_block()->num_packets());
  EXPECT_EQ(1, parser.report_blocks_per_ssrc(kRemoteSsrc));
  EXPECT_EQ(1, parser.report_blocks_per_ssrc(kRemoteSsrc + 1));
}

TEST(RtcpPacketTest, IjNoItem) {
  Ij ij;

  RawPacket packet = ij.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.ij()->num_packets());
  EXPECT_EQ(0, parser.ij_item()->num_packets());
}

TEST(RtcpPacketTest, IjOneItem) {
  Ij ij;
  ij.WithJitterItem(0x11111111);

  RawPacket packet = ij.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.ij()->num_packets());
  EXPECT_EQ(1, parser.ij_item()->num_packets());
  EXPECT_EQ(0x11111111U, parser.ij_item()->Jitter());
}

TEST(RtcpPacketTest, IjTwoItems) {
  Ij ij;
  ij.WithJitterItem(0x11111111);
  ij.WithJitterItem(0x22222222);

  RawPacket packet = ij.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.ij()->num_packets());
  EXPECT_EQ(2, parser.ij_item()->num_packets());
  EXPECT_EQ(0x22222222U, parser.ij_item()->Jitter());
}

TEST(RtcpPacketTest, AppWithNoData) {
  App app;
  app.WithSubType(30);
  uint32_t name = 'n' << 24;
  name += 'a' << 16;
  name += 'm' << 8;
  name += 'e';
  app.WithName(name);

  RawPacket packet = app.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
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

  RawPacket packet = app.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
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
  sdes.WithCName(kSenderSsrc, "alice@host");

  RawPacket packet = sdes.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.sdes()->num_packets());
  EXPECT_EQ(1, parser.sdes_chunk()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sdes_chunk()->Ssrc());
  EXPECT_EQ("alice@host", parser.sdes_chunk()->Cname());
}

TEST(RtcpPacketTest, SdesWithMultipleChunks) {
  Sdes sdes;
  sdes.WithCName(kSenderSsrc, "a");
  sdes.WithCName(kSenderSsrc + 1, "ab");
  sdes.WithCName(kSenderSsrc + 2, "abc");
  sdes.WithCName(kSenderSsrc + 3, "abcd");
  sdes.WithCName(kSenderSsrc + 4, "abcde");
  sdes.WithCName(kSenderSsrc + 5, "abcdef");

  RawPacket packet = sdes.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.sdes()->num_packets());
  EXPECT_EQ(6, parser.sdes_chunk()->num_packets());
  EXPECT_EQ(kSenderSsrc + 5, parser.sdes_chunk()->Ssrc());
  EXPECT_EQ("abcdef", parser.sdes_chunk()->Cname());
}

TEST(RtcpPacketTest, CnameItemWithEmptyString) {
  Sdes sdes;
  sdes.WithCName(kSenderSsrc, "");

  RawPacket packet = sdes.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.sdes()->num_packets());
  EXPECT_EQ(1, parser.sdes_chunk()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sdes_chunk()->Ssrc());
  EXPECT_EQ("", parser.sdes_chunk()->Cname());
}

TEST(RtcpPacketTest, Pli) {
  Pli pli;
  pli.From(kSenderSsrc);
  pli.To(kRemoteSsrc);

  RawPacket packet = pli.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.pli()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.pli()->Ssrc());
  EXPECT_EQ(kRemoteSsrc, parser.pli()->MediaSsrc());
}

TEST(RtcpPacketTest, Sli) {
  const uint16_t kFirstMb = 7777;
  const uint16_t kNumberOfMb = 6666;
  const uint8_t kPictureId = 60;
  Sli sli;
  sli.From(kSenderSsrc);
  sli.To(kRemoteSsrc);
  sli.WithFirstMb(kFirstMb);
  sli.WithNumberOfMb(kNumberOfMb);
  sli.WithPictureId(kPictureId);

  RawPacket packet = sli.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.sli()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.sli()->Ssrc());
  EXPECT_EQ(kRemoteSsrc, parser.sli()->MediaSsrc());
  EXPECT_EQ(1, parser.sli_item()->num_packets());
  EXPECT_EQ(kFirstMb, parser.sli_item()->FirstMb());
  EXPECT_EQ(kNumberOfMb, parser.sli_item()->NumberOfMb());
  EXPECT_EQ(kPictureId, parser.sli_item()->PictureId());
}

TEST(RtcpPacketTest, Nack) {
  Nack nack;
  const uint16_t kList[] = {0, 1, 3, 8, 16};
  const uint16_t kListLength = sizeof(kList) / sizeof(kList[0]);
  nack.From(kSenderSsrc);
  nack.To(kRemoteSsrc);
  nack.WithList(kList, kListLength);
  RawPacket packet = nack.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.nack()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.nack()->Ssrc());
  EXPECT_EQ(kRemoteSsrc, parser.nack()->MediaSsrc());
  EXPECT_EQ(1, parser.nack_item()->num_packets());
  std::vector<uint16_t> seqs = parser.nack_item()->last_nack_list();
  EXPECT_EQ(kListLength, seqs.size());
  for (size_t i = 0; i < kListLength; ++i) {
    EXPECT_EQ(kList[i], seqs[i]);
  }
}

TEST(RtcpPacketTest, NackWithWrap) {
  Nack nack;
  const uint16_t kList[] = {65500, 65516, 65534, 65535, 0, 1, 3, 20, 100};
  const uint16_t kListLength = sizeof(kList) / sizeof(kList[0]);
  nack.From(kSenderSsrc);
  nack.To(kRemoteSsrc);
  nack.WithList(kList, kListLength);
  RawPacket packet = nack.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.nack()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.nack()->Ssrc());
  EXPECT_EQ(kRemoteSsrc, parser.nack()->MediaSsrc());
  EXPECT_EQ(4, parser.nack_item()->num_packets());
  std::vector<uint16_t> seqs = parser.nack_item()->last_nack_list();
  EXPECT_EQ(kListLength, seqs.size());
  for (size_t i = 0; i < kListLength; ++i) {
    EXPECT_EQ(kList[i], seqs[i]);
  }
}

TEST(RtcpPacketTest, Rpsi) {
  Rpsi rpsi;
  // 1000001 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x41;
  const uint16_t kNumberOfValidBytes = 1;
  rpsi.WithPayloadType(100);
  rpsi.WithPictureId(kPictureId);

  RawPacket packet = rpsi.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
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

  RawPacket packet = rpsi.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(kNumberOfValidBytes * 8, parser.rpsi()->NumberOfValidBits());
  EXPECT_EQ(kPictureId, parser.rpsi()->PictureId());
}

TEST(RtcpPacketTest, RpsiWithThreeByteNativeString) {
  Rpsi rpsi;
  // 10000|00 100000|0 1000000 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x102040;
  const uint16_t kNumberOfValidBytes = 3;
  rpsi.WithPictureId(kPictureId);

  RawPacket packet = rpsi.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(kNumberOfValidBytes * 8, parser.rpsi()->NumberOfValidBits());
  EXPECT_EQ(kPictureId, parser.rpsi()->PictureId());
}

TEST(RtcpPacketTest, RpsiWithFourByteNativeString) {
  Rpsi rpsi;
  // 1000|001 00001|01 100001|1 1000010 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x84161C2;
  const uint16_t kNumberOfValidBytes = 4;
  rpsi.WithPictureId(kPictureId);

  RawPacket packet = rpsi.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
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

  RawPacket packet = rpsi.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(kNumberOfValidBytes * 8, parser.rpsi()->NumberOfValidBits());
  EXPECT_EQ(kPictureId, parser.rpsi()->PictureId());
}

TEST(RtcpPacketTest, Fir) {
  Fir fir;
  fir.From(kSenderSsrc);
  fir.To(kRemoteSsrc);
  fir.WithCommandSeqNum(123);

  RawPacket packet = fir.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.fir()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.fir()->Ssrc());
  EXPECT_EQ(1, parser.fir_item()->num_packets());
  EXPECT_EQ(kRemoteSsrc, parser.fir_item()->Ssrc());
  EXPECT_EQ(123U, parser.fir_item()->SeqNum());
}

TEST(RtcpPacketTest, AppendPacket) {
  Fir fir;
  ReportBlock rb;
  ReceiverReport rr;
  rr.From(kSenderSsrc);
  rr.WithReportBlock(&rb);
  rr.Append(&fir);

  RawPacket packet = rr.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.receiver_report()->Ssrc());
  EXPECT_EQ(1, parser.report_block()->num_packets());
  EXPECT_EQ(1, parser.fir()->num_packets());
}

TEST(RtcpPacketTest, AppendPacketOnEmpty) {
  Empty empty;
  ReceiverReport rr;
  rr.From(kSenderSsrc);
  empty.Append(&rr);

  RawPacket packet = empty.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.receiver_report()->num_packets());
  EXPECT_EQ(0, parser.report_block()->num_packets());
}

TEST(RtcpPacketTest, AppendPacketWithOwnAppendedPacket) {
  Fir fir;
  Bye bye;
  ReportBlock rb;

  ReceiverReport rr;
  rr.WithReportBlock(&rb);
  rr.Append(&fir);

  SenderReport sr;
  sr.Append(&bye);
  sr.Append(&rr);

  RawPacket packet = sr.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.sender_report()->num_packets());
  EXPECT_EQ(1, parser.receiver_report()->num_packets());
  EXPECT_EQ(1, parser.report_block()->num_packets());
  EXPECT_EQ(1, parser.bye()->num_packets());
  EXPECT_EQ(1, parser.fir()->num_packets());
}

TEST(RtcpPacketTest, Bye) {
  Bye bye;
  bye.From(kSenderSsrc);

  RawPacket packet = bye.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.bye()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.bye()->Ssrc());
}

TEST(RtcpPacketTest, ByeWithCsrcs) {
  Fir fir;
  Bye bye;
  bye.From(kSenderSsrc);
  bye.WithCsrc(0x22222222);
  bye.WithCsrc(0x33333333);
  bye.Append(&fir);

  RawPacket packet = bye.Build();
  RtcpPacketParser parser;
  parser.Parse(packet.buffer(), packet.buffer_length());
  EXPECT_EQ(1, parser.bye()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.bye()->Ssrc());
  EXPECT_EQ(1, parser.fir()->num_packets());
}

TEST(RtcpPacketTest, BuildWithInputBuffer) {
  Fir fir;
  ReportBlock rb;
  ReceiverReport rr;
  rr.From(kSenderSsrc);
  rr.WithReportBlock(&rb);
  rr.Append(&fir);

  const size_t kRrLength = 8;
  const size_t kReportBlockLength = 24;
  const size_t kFirLength = 20;

  size_t len = 0;
  uint8_t packet[kRrLength + kReportBlockLength + kFirLength];
  rr.Build(packet, &len, kRrLength + kReportBlockLength + kFirLength);

  RtcpPacketParser parser;
  parser.Parse(packet, len);
  EXPECT_EQ(1, parser.receiver_report()->num_packets());
  EXPECT_EQ(1, parser.report_block()->num_packets());
  EXPECT_EQ(1, parser.fir()->num_packets());
}

TEST(RtcpPacketTest, BuildWithTooSmallBuffer) {
  ReportBlock rb;
  ReceiverReport rr;
  rr.From(kSenderSsrc);
  rr.WithReportBlock(&rb);

  const size_t kRrLength = 8;
  const size_t kReportBlockLength = 24;

  // No packet.
  size_t len = 0;
  uint8_t packet[kRrLength + kReportBlockLength - 1];
  rr.Build(packet, &len, kRrLength + kReportBlockLength - 1);
  RtcpPacketParser parser;
  parser.Parse(packet, len);
  EXPECT_EQ(0U, len);
}

TEST(RtcpPacketTest, BuildWithTooSmallBuffer_LastBlockFits) {
  Fir fir;
  ReportBlock rb;
  ReceiverReport rr;
  rr.From(kSenderSsrc);
  rr.WithReportBlock(&rb);
  rr.Append(&fir);

  const size_t kRrLength = 8;
  const size_t kReportBlockLength = 24;

  size_t len = 0;
  uint8_t packet[kRrLength + kReportBlockLength - 1];
  rr.Build(packet, &len, kRrLength + kReportBlockLength - 1);
  RtcpPacketParser parser;
  parser.Parse(packet, len);
  EXPECT_EQ(0, parser.receiver_report()->num_packets());
  EXPECT_EQ(0, parser.report_block()->num_packets());
  EXPECT_EQ(1, parser.fir()->num_packets());
}
}  // namespace webrtc
