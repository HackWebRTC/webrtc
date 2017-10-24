/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_transceiver_impl.h"

#include <vector>

#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "rtc_base/ptr_util.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/rtcp_packet_parser.h"

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SizeIs;
using ::webrtc::MockTransport;
using ::webrtc::RtcpTransceiverConfig;
using ::webrtc::RtcpTransceiverImpl;
using ::webrtc::rtcp::ReportBlock;
using ::webrtc::test::RtcpPacketParser;

class MockReceiveStatisticsProvider : public webrtc::ReceiveStatisticsProvider {
 public:
  MOCK_METHOD1(RtcpReportBlocks, std::vector<ReportBlock>(size_t));
};

TEST(RtcpTransceiverImplTest, ForceSendReportEmitsRtcpPacket) {
  MockTransport outgoing_transport;
  RtcpPacketParser rtcp_parser;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillOnce(Invoke(&rtcp_parser, &RtcpPacketParser::Parse));

  RtcpTransceiverConfig config;
  config.outgoing_transport = &outgoing_transport;
  RtcpTransceiverImpl rtcp_transceiver(config);

  ASSERT_EQ(rtcp_parser.receiver_report()->num_packets(), 0);
  rtcp_transceiver.SendCompoundPacket();
  EXPECT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
}

TEST(RtcpTransceiverImplTest, ReceiverReportUsesReceiveStatistics) {
  const uint32_t kSenderSsrc = 12345;
  const uint32_t kMediaSsrc = 54321;
  MockTransport outgoing_transport;
  RtcpPacketParser rtcp_parser;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillOnce(Invoke(&rtcp_parser, &RtcpPacketParser::Parse));

  MockReceiveStatisticsProvider receive_statistics;
  std::vector<ReportBlock> report_blocks(1);
  report_blocks[0].SetMediaSsrc(kMediaSsrc);
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillRepeatedly(Return(report_blocks));

  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  config.outgoing_transport = &outgoing_transport;
  config.receive_statistics = &receive_statistics;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();

  ASSERT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  EXPECT_EQ(rtcp_parser.receiver_report()->sender_ssrc(), kSenderSsrc);
  ASSERT_THAT(rtcp_parser.receiver_report()->report_blocks(),
              SizeIs(report_blocks.size()));
  EXPECT_EQ(rtcp_parser.receiver_report()->report_blocks()[0].source_ssrc(),
            kMediaSsrc);
}

}  // namespace
