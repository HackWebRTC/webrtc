/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/rtcp_packet_parser.h"

using webrtc::rtcp::Dlrr;
using webrtc::rtcp::ExtendedReports;
using webrtc::rtcp::RawPacket;
using webrtc::rtcp::Rrtr;
using webrtc::rtcp::VoipMetric;
using webrtc::test::RtcpPacketParser;

namespace webrtc {
namespace rtcp {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;

TEST(RtcpPacketExtendedReportsTest, WithNoReportBlocks) {
  ExtendedReports xr;
  xr.From(kSenderSsrc);

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
}

TEST(RtcpPacketExtendedReportsTest, WithRrtr) {
  Rrtr rrtr;
  rrtr.WithNtp(NtpTime(0x11111111, 0x22222222));
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithRrtr(&rrtr));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(1, parser.rrtr()->num_packets());
  EXPECT_EQ(0x11111111U, parser.rrtr()->NtpSec());
  EXPECT_EQ(0x22222222U, parser.rrtr()->NtpFrac());
}

TEST(RtcpPacketExtendedReportsTest, WithTwoRrtrBlocks) {
  Rrtr rrtr1;
  rrtr1.WithNtp(NtpTime(0x11111111, 0x22222222));
  Rrtr rrtr2;
  rrtr2.WithNtp(NtpTime(0x33333333, 0x44444444));
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithRrtr(&rrtr1));
  EXPECT_TRUE(xr.WithRrtr(&rrtr2));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(2, parser.rrtr()->num_packets());
  EXPECT_EQ(0x33333333U, parser.rrtr()->NtpSec());
  EXPECT_EQ(0x44444444U, parser.rrtr()->NtpFrac());
}

TEST(RtcpPacketExtendedReportsTest, WithDlrrWithOneSubBlock) {
  Dlrr dlrr;
  EXPECT_TRUE(dlrr.WithDlrrItem(0x11111111, 0x22222222, 0x33333333));
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithDlrr(&dlrr));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(1, parser.dlrr()->num_packets());
  EXPECT_EQ(1, parser.dlrr_items()->num_packets());
  EXPECT_EQ(0x11111111U, parser.dlrr_items()->Ssrc(0));
  EXPECT_EQ(0x22222222U, parser.dlrr_items()->LastRr(0));
  EXPECT_EQ(0x33333333U, parser.dlrr_items()->DelayLastRr(0));
}

TEST(RtcpPacketExtendedReportsTest, WithDlrrWithTwoSubBlocks) {
  Dlrr dlrr;
  EXPECT_TRUE(dlrr.WithDlrrItem(0x11111111, 0x22222222, 0x33333333));
  EXPECT_TRUE(dlrr.WithDlrrItem(0x44444444, 0x55555555, 0x66666666));
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithDlrr(&dlrr));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(1, parser.dlrr()->num_packets());
  EXPECT_EQ(2, parser.dlrr_items()->num_packets());
  EXPECT_EQ(0x11111111U, parser.dlrr_items()->Ssrc(0));
  EXPECT_EQ(0x22222222U, parser.dlrr_items()->LastRr(0));
  EXPECT_EQ(0x33333333U, parser.dlrr_items()->DelayLastRr(0));
  EXPECT_EQ(0x44444444U, parser.dlrr_items()->Ssrc(1));
  EXPECT_EQ(0x55555555U, parser.dlrr_items()->LastRr(1));
  EXPECT_EQ(0x66666666U, parser.dlrr_items()->DelayLastRr(1));
}

TEST(RtcpPacketExtendedReportsTest, WithTwoDlrrBlocks) {
  Dlrr dlrr1;
  EXPECT_TRUE(dlrr1.WithDlrrItem(0x11111111, 0x22222222, 0x33333333));
  Dlrr dlrr2;
  EXPECT_TRUE(dlrr2.WithDlrrItem(0x44444444, 0x55555555, 0x66666666));
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithDlrr(&dlrr1));
  EXPECT_TRUE(xr.WithDlrr(&dlrr2));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(2, parser.dlrr()->num_packets());
  EXPECT_EQ(2, parser.dlrr_items()->num_packets());
  EXPECT_EQ(0x11111111U, parser.dlrr_items()->Ssrc(0));
  EXPECT_EQ(0x22222222U, parser.dlrr_items()->LastRr(0));
  EXPECT_EQ(0x33333333U, parser.dlrr_items()->DelayLastRr(0));
  EXPECT_EQ(0x44444444U, parser.dlrr_items()->Ssrc(1));
  EXPECT_EQ(0x55555555U, parser.dlrr_items()->LastRr(1));
  EXPECT_EQ(0x66666666U, parser.dlrr_items()->DelayLastRr(1));
}

TEST(RtcpPacketExtendedReportsTest, WithVoipMetric) {
  RTCPVoIPMetric metric;
  metric.lossRate = 1;
  metric.discardRate = 2;
  metric.burstDensity = 3;
  metric.gapDensity = 4;
  metric.burstDuration = 0x1111;
  metric.gapDuration = 0x2222;
  metric.roundTripDelay = 0x3333;
  metric.endSystemDelay = 0x4444;
  metric.signalLevel = 5;
  metric.noiseLevel = 6;
  metric.RERL = 7;
  metric.Gmin = 8;
  metric.Rfactor = 9;
  metric.extRfactor = 10;
  metric.MOSLQ = 11;
  metric.MOSCQ = 12;
  metric.RXconfig = 13;
  metric.JBnominal = 0x5555;
  metric.JBmax = 0x6666;
  metric.JBabsMax = 0x7777;
  VoipMetric metric_block;
  metric_block.To(kRemoteSsrc);
  metric_block.WithVoipMetric(metric);
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithVoipMetric(&metric_block));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(1, parser.voip_metric()->num_packets());
  EXPECT_EQ(kRemoteSsrc, parser.voip_metric()->Ssrc());
  EXPECT_EQ(1, parser.voip_metric()->LossRate());
  EXPECT_EQ(2, parser.voip_metric()->DiscardRate());
  EXPECT_EQ(3, parser.voip_metric()->BurstDensity());
  EXPECT_EQ(4, parser.voip_metric()->GapDensity());
  EXPECT_EQ(0x1111, parser.voip_metric()->BurstDuration());
  EXPECT_EQ(0x2222, parser.voip_metric()->GapDuration());
  EXPECT_EQ(0x3333, parser.voip_metric()->RoundTripDelay());
  EXPECT_EQ(0x4444, parser.voip_metric()->EndSystemDelay());
  EXPECT_EQ(5, parser.voip_metric()->SignalLevel());
  EXPECT_EQ(6, parser.voip_metric()->NoiseLevel());
  EXPECT_EQ(7, parser.voip_metric()->Rerl());
  EXPECT_EQ(8, parser.voip_metric()->Gmin());
  EXPECT_EQ(9, parser.voip_metric()->Rfactor());
  EXPECT_EQ(10, parser.voip_metric()->ExtRfactor());
  EXPECT_EQ(11, parser.voip_metric()->MosLq());
  EXPECT_EQ(12, parser.voip_metric()->MosCq());
  EXPECT_EQ(13, parser.voip_metric()->RxConfig());
  EXPECT_EQ(0x5555, parser.voip_metric()->JbNominal());
  EXPECT_EQ(0x6666, parser.voip_metric()->JbMax());
  EXPECT_EQ(0x7777, parser.voip_metric()->JbAbsMax());
}

TEST(RtcpPacketExtendedReportsTest, WithMultipleReportBlocks) {
  Rrtr rrtr;
  Dlrr dlrr;
  EXPECT_TRUE(dlrr.WithDlrrItem(1, 2, 3));
  VoipMetric metric;
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithRrtr(&rrtr));
  EXPECT_TRUE(xr.WithDlrr(&dlrr));
  EXPECT_TRUE(xr.WithVoipMetric(&metric));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(1, parser.rrtr()->num_packets());
  EXPECT_EQ(1, parser.dlrr()->num_packets());
  EXPECT_EQ(1, parser.dlrr_items()->num_packets());
  EXPECT_EQ(1, parser.voip_metric()->num_packets());
}

TEST(RtcpPacketExtendedReportsTest, DlrrWithoutItemNotIncludedInPacket) {
  Rrtr rrtr;
  Dlrr dlrr;
  VoipMetric metric;
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithRrtr(&rrtr));
  EXPECT_TRUE(xr.WithDlrr(&dlrr));
  EXPECT_TRUE(xr.WithVoipMetric(&metric));

  rtc::scoped_ptr<RawPacket> packet(xr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.xr_header()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.xr_header()->Ssrc());
  EXPECT_EQ(1, parser.rrtr()->num_packets());
  EXPECT_EQ(0, parser.dlrr()->num_packets());
  EXPECT_EQ(1, parser.voip_metric()->num_packets());
}

TEST(RtcpPacketExtendedReportsTest, WithTooManyBlocks) {
  const int kMaxBlocks = 50;
  ExtendedReports xr;

  Rrtr rrtr;
  for (int i = 0; i < kMaxBlocks; ++i)
    EXPECT_TRUE(xr.WithRrtr(&rrtr));
  EXPECT_FALSE(xr.WithRrtr(&rrtr));

  Dlrr dlrr;
  for (int i = 0; i < kMaxBlocks; ++i)
    EXPECT_TRUE(xr.WithDlrr(&dlrr));
  EXPECT_FALSE(xr.WithDlrr(&dlrr));

  VoipMetric voip_metric;
  for (int i = 0; i < kMaxBlocks; ++i)
    EXPECT_TRUE(xr.WithVoipMetric(&voip_metric));
  EXPECT_FALSE(xr.WithVoipMetric(&voip_metric));
}
}  // namespace rtcp
}  // namespace webrtc
