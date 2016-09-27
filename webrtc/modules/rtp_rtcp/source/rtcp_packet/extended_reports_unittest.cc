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
#include "webrtc/base/random.h"
#include "webrtc/test/rtcp_packet_parser.h"

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::IsEmpty;
using testing::make_tuple;
using webrtc::rtcp::Dlrr;
using webrtc::rtcp::ExtendedReports;
using webrtc::rtcp::ReceiveTimeInfo;
using webrtc::rtcp::Rrtr;
using webrtc::rtcp::VoipMetric;

namespace webrtc {
// Define comparision operators that shouldn't be needed in production,
// but make testing matches more clear.
bool operator==(const RTCPVoIPMetric& metric1, const RTCPVoIPMetric& metric2) {
  return metric1.lossRate == metric2.lossRate &&
         metric1.discardRate == metric2.discardRate &&
         metric1.burstDensity == metric2.burstDensity &&
         metric1.gapDensity == metric2.gapDensity &&
         metric1.burstDuration == metric2.burstDuration &&
         metric1.gapDuration == metric2.gapDuration &&
         metric1.roundTripDelay == metric2.roundTripDelay &&
         metric1.endSystemDelay == metric2.endSystemDelay &&
         metric1.signalLevel == metric2.signalLevel &&
         metric1.noiseLevel == metric2.noiseLevel &&
         metric1.RERL == metric2.RERL &&
         metric1.Gmin == metric2.Gmin &&
         metric1.Rfactor == metric2.Rfactor &&
         metric1.extRfactor == metric2.extRfactor &&
         metric1.MOSLQ == metric2.MOSLQ &&
         metric1.MOSCQ == metric2.MOSCQ &&
         metric1.RXconfig == metric2.RXconfig &&
         metric1.JBnominal == metric2.JBnominal &&
         metric1.JBmax == metric2.JBmax &&
         metric1.JBabsMax == metric2.JBabsMax;
}

namespace rtcp {
bool operator==(const Rrtr& rrtr1, const Rrtr& rrtr2) {
  return rrtr1.ntp() == rrtr2.ntp();
}

bool operator==(const ReceiveTimeInfo& time1, const ReceiveTimeInfo& time2) {
  return time1.ssrc == time2.ssrc &&
         time1.last_rr == time2.last_rr &&
         time1.delay_since_last_rr == time2.delay_since_last_rr;
}

bool operator==(const Dlrr& dlrr1, const Dlrr& dlrr2) {
  return dlrr1.sub_blocks() == dlrr2.sub_blocks();
}

bool operator==(const VoipMetric& metric1, const VoipMetric& metric2) {
  return metric1.ssrc() == metric2.ssrc() &&
         metric1.voip_metric() == metric2.voip_metric();
}
}  // namespace rtcp

namespace {
constexpr uint32_t kSenderSsrc = 0x12345678;
constexpr uint8_t kEmptyPacket[] = {0x80, 207,  0x00, 0x01,
                                    0x12, 0x34, 0x56, 0x78};
}  // namespace

class RtcpPacketExtendedReportsTest : public ::testing::Test {
 public:
  RtcpPacketExtendedReportsTest() : random_(0x123456789) {}

 protected:
  template <typename T>
  T Rand() {
    return random_.Rand<T>();
  }

 private:
  Random random_;
};

template <>
ReceiveTimeInfo RtcpPacketExtendedReportsTest::Rand<ReceiveTimeInfo>() {
  uint32_t ssrc = Rand<uint32_t>();
  uint32_t last_rr = Rand<uint32_t>();
  uint32_t delay_since_last_rr = Rand<uint32_t>();
  return ReceiveTimeInfo(ssrc, last_rr, delay_since_last_rr);
}

template <>
NtpTime RtcpPacketExtendedReportsTest::Rand<NtpTime>() {
  uint32_t secs = Rand<uint32_t>();
  uint32_t frac = Rand<uint32_t>();
  return NtpTime(secs, frac);
}

template <>
Rrtr RtcpPacketExtendedReportsTest::Rand<Rrtr>() {
  Rrtr rrtr;
  rrtr.WithNtp(Rand<NtpTime>());
  return rrtr;
}

template <>
RTCPVoIPMetric RtcpPacketExtendedReportsTest::Rand<RTCPVoIPMetric>() {
  RTCPVoIPMetric metric;
  metric.lossRate       = Rand<uint8_t>();
  metric.discardRate    = Rand<uint8_t>();
  metric.burstDensity   = Rand<uint8_t>();
  metric.gapDensity     = Rand<uint8_t>();
  metric.burstDuration  = Rand<uint16_t>();
  metric.gapDuration    = Rand<uint16_t>();
  metric.roundTripDelay = Rand<uint16_t>();
  metric.endSystemDelay = Rand<uint16_t>();
  metric.signalLevel    = Rand<uint8_t>();
  metric.noiseLevel     = Rand<uint8_t>();
  metric.RERL           = Rand<uint8_t>();
  metric.Gmin           = Rand<uint8_t>();
  metric.Rfactor        = Rand<uint8_t>();
  metric.extRfactor     = Rand<uint8_t>();
  metric.MOSLQ          = Rand<uint8_t>();
  metric.MOSCQ          = Rand<uint8_t>();
  metric.RXconfig       = Rand<uint8_t>();
  metric.JBnominal      = Rand<uint16_t>();
  metric.JBmax          = Rand<uint16_t>();
  metric.JBabsMax       = Rand<uint16_t>();
  return metric;
}

template <>
VoipMetric RtcpPacketExtendedReportsTest::Rand<VoipMetric>() {
  VoipMetric voip_metric;
  voip_metric.To(Rand<uint32_t>());
  voip_metric.WithVoipMetric(Rand<RTCPVoIPMetric>());
  return voip_metric;
}

TEST_F(RtcpPacketExtendedReportsTest, CreateWithoutReportBlocks) {
  ExtendedReports xr;
  xr.From(kSenderSsrc);

  rtc::Buffer packet = xr.Build();

  EXPECT_THAT(make_tuple(packet.data(), packet.size()),
              ElementsAreArray(kEmptyPacket));
}

TEST_F(RtcpPacketExtendedReportsTest, ParseWithoutReportBlocks) {
  ExtendedReports parsed;
  EXPECT_TRUE(test::ParseSinglePacket(kEmptyPacket, &parsed));
  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.rrtrs(), IsEmpty());
  EXPECT_THAT(parsed.dlrrs(), IsEmpty());
  EXPECT_THAT(parsed.voip_metrics(), IsEmpty());
}

TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithOneRrtrBlock) {
  Rrtr rrtr = Rand<Rrtr>();
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithRrtr(rrtr));
  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.rrtrs(), ElementsAre(rrtr));
}

TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithTwoRrtrBlocks) {
  Rrtr rrtr1 = Rand<Rrtr>();
  Rrtr rrtr2 = Rand<Rrtr>();
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithRrtr(rrtr1));
  EXPECT_TRUE(xr.WithRrtr(rrtr2));

  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.rrtrs(), ElementsAre(rrtr1, rrtr2));
}

TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithDlrrWithOneSubBlock) {
  Dlrr dlrr;
  EXPECT_TRUE(dlrr.WithDlrrItem(Rand<ReceiveTimeInfo>()));
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithDlrr(dlrr));

  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.dlrrs(), ElementsAre(dlrr));
}

TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithDlrrWithTwoSubBlocks) {
  Dlrr dlrr;
  EXPECT_TRUE(dlrr.WithDlrrItem(Rand<ReceiveTimeInfo>()));
  EXPECT_TRUE(dlrr.WithDlrrItem(Rand<ReceiveTimeInfo>()));
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithDlrr(dlrr));

  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.dlrrs(), ElementsAre(dlrr));
}

TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithTwoDlrrBlocks) {
  Dlrr dlrr1;
  EXPECT_TRUE(dlrr1.WithDlrrItem(Rand<ReceiveTimeInfo>()));
  Dlrr dlrr2;
  EXPECT_TRUE(dlrr2.WithDlrrItem(Rand<ReceiveTimeInfo>()));
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithDlrr(dlrr1));
  EXPECT_TRUE(xr.WithDlrr(dlrr2));

  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.dlrrs(), ElementsAre(dlrr1, dlrr2));
}

TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithVoipMetric) {
  VoipMetric voip_metric = Rand<VoipMetric>();

  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithVoipMetric(voip_metric));

  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.voip_metrics(), ElementsAre(voip_metric));
}

TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithMultipleReportBlocks) {
  Rrtr rrtr = Rand<Rrtr>();
  Dlrr dlrr;
  EXPECT_TRUE(dlrr.WithDlrrItem(Rand<ReceiveTimeInfo>()));
  VoipMetric metric = Rand<VoipMetric>();
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithRrtr(rrtr));
  EXPECT_TRUE(xr.WithDlrr(dlrr));
  EXPECT_TRUE(xr.WithVoipMetric(metric));

  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.rrtrs(), ElementsAre(rrtr));
  EXPECT_THAT(parsed.dlrrs(), ElementsAre(dlrr));
  EXPECT_THAT(parsed.voip_metrics(), ElementsAre(metric));
}

TEST_F(RtcpPacketExtendedReportsTest, DlrrWithoutItemNotIncludedInPacket) {
  Rrtr rrtr = Rand<Rrtr>();
  Dlrr dlrr;
  VoipMetric metric = Rand<VoipMetric>();
  ExtendedReports xr;
  xr.From(kSenderSsrc);
  EXPECT_TRUE(xr.WithRrtr(rrtr));
  EXPECT_TRUE(xr.WithDlrr(dlrr));
  EXPECT_TRUE(xr.WithVoipMetric(metric));

  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_THAT(parsed.rrtrs(), ElementsAre(rrtr));
  EXPECT_THAT(parsed.dlrrs(), IsEmpty());
  EXPECT_THAT(parsed.voip_metrics(), ElementsAre(metric));
}

TEST_F(RtcpPacketExtendedReportsTest, WithTooManyBlocks) {
  const size_t kMaxBlocks = 50;
  ExtendedReports xr;

  Rrtr rrtr = Rand<Rrtr>();
  for (size_t i = 0; i < kMaxBlocks; ++i)
    EXPECT_TRUE(xr.WithRrtr(rrtr));
  EXPECT_FALSE(xr.WithRrtr(rrtr));

  Dlrr dlrr;
  for (size_t i = 0; i < kMaxBlocks; ++i)
    EXPECT_TRUE(xr.WithDlrr(dlrr));
  EXPECT_FALSE(xr.WithDlrr(dlrr));

  VoipMetric voip_metric = Rand<VoipMetric>();
  for (size_t i = 0; i < kMaxBlocks; ++i)
    EXPECT_TRUE(xr.WithVoipMetric(voip_metric));
  EXPECT_FALSE(xr.WithVoipMetric(voip_metric));
}
}  // namespace webrtc
