/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sender_report.h"

#include "testing/gtest/include/gtest/gtest.h"

using webrtc::rtcp::ReportBlock;
using webrtc::rtcp::SenderReport;
using webrtc::RTCPUtility::RtcpCommonHeader;
using webrtc::RTCPUtility::RtcpParseCommonHeader;

namespace webrtc {

class RtcpPacketSenderReportTest : public ::testing::Test {
 protected:
  const uint32_t kSenderSsrc = 0x12345678;
  const uint32_t kRemoteSsrc = 0x23456789;

  void ParsePacket(const rtc::Buffer& packet) {
    RtcpCommonHeader header;
    EXPECT_TRUE(RtcpParseCommonHeader(packet.data(), packet.size(), &header));
    EXPECT_EQ(packet.size(), header.BlockSize());
    EXPECT_TRUE(parsed_.Parse(
        header, packet.data() + RtcpCommonHeader::kHeaderSizeBytes));
  }

  // Only ParsePacket can change parsed, tests should use it in readonly mode.
  const SenderReport& parsed() { return parsed_; }

 private:
  SenderReport parsed_;
};

TEST_F(RtcpPacketSenderReportTest, WithoutReportBlocks) {
  const NtpTime kNtp(0x11121418, 0x22242628);
  const uint32_t kRtpTimestamp = 0x33343536;
  const uint32_t kPacketCount = 0x44454647;
  const uint32_t kOctetCount = 0x55565758;

  SenderReport sr;
  sr.From(kSenderSsrc);
  sr.WithNtp(kNtp);
  sr.WithRtpTimestamp(kRtpTimestamp);
  sr.WithPacketCount(kPacketCount);
  sr.WithOctetCount(kOctetCount);

  rtc::Buffer packet = sr.Build();
  ParsePacket(packet);

  EXPECT_EQ(kSenderSsrc, parsed().sender_ssrc());
  EXPECT_EQ(kNtp, parsed().ntp());
  EXPECT_EQ(kRtpTimestamp, parsed().rtp_timestamp());
  EXPECT_EQ(kPacketCount, parsed().sender_packet_count());
  EXPECT_EQ(kOctetCount, parsed().sender_octet_count());
  EXPECT_TRUE(parsed().report_blocks().empty());
}

TEST_F(RtcpPacketSenderReportTest, WithOneReportBlock) {
  ReportBlock rb;
  rb.To(kRemoteSsrc);

  SenderReport sr;
  sr.From(kSenderSsrc);
  EXPECT_TRUE(sr.WithReportBlock(rb));

  rtc::Buffer packet = sr.Build();
  ParsePacket(packet);

  EXPECT_EQ(kSenderSsrc, parsed().sender_ssrc());
  EXPECT_EQ(1u, parsed().report_blocks().size());
  EXPECT_EQ(kRemoteSsrc, parsed().report_blocks()[0].source_ssrc());
}

TEST_F(RtcpPacketSenderReportTest, WithTwoReportBlocks) {
  ReportBlock rb1;
  rb1.To(kRemoteSsrc);
  ReportBlock rb2;
  rb2.To(kRemoteSsrc + 1);

  SenderReport sr;
  sr.From(kSenderSsrc);
  EXPECT_TRUE(sr.WithReportBlock(rb1));
  EXPECT_TRUE(sr.WithReportBlock(rb2));

  rtc::Buffer packet = sr.Build();
  ParsePacket(packet);

  EXPECT_EQ(kSenderSsrc, parsed().sender_ssrc());
  EXPECT_EQ(2u, parsed().report_blocks().size());
  EXPECT_EQ(kRemoteSsrc, parsed().report_blocks()[0].source_ssrc());
  EXPECT_EQ(kRemoteSsrc + 1, parsed().report_blocks()[1].source_ssrc());
}

TEST_F(RtcpPacketSenderReportTest, WithTooManyReportBlocks) {
  SenderReport sr;
  sr.From(kSenderSsrc);
  const size_t kMaxReportBlocks = (1 << 5) - 1;
  ReportBlock rb;
  for (size_t i = 0; i < kMaxReportBlocks; ++i) {
    rb.To(kRemoteSsrc + i);
    EXPECT_TRUE(sr.WithReportBlock(rb));
  }
  rb.To(kRemoteSsrc + kMaxReportBlocks);
  EXPECT_FALSE(sr.WithReportBlock(rb));
}

}  // namespace webrtc
