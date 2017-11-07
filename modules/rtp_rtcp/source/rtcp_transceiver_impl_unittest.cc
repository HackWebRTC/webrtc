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
#include "rtc_base/event.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/task_queue.h"
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

// Since some tests will need to wait for this period, make it small to avoid
// slowing tests too much. As long as there are test bots with high scheduler
// granularity, small period should be ok.
constexpr int kReportPeriodMs = 10;
// On some systems task queue might be slow, instead of guessing right
// grace period, use very large timeout, 100x larger expected wait time.
// Use finite timeout to fail tests rather than hang them.
constexpr int kAlmostForeverMs = 1000;

// Helper to wait for an rtcp packet produced on a different thread/task queue.
class FakeRtcpTransport : public webrtc::Transport {
 public:
  FakeRtcpTransport() : sent_rtcp_(false, false) {}
  bool SendRtcp(const uint8_t* data, size_t size) override {
    sent_rtcp_.Set();
    return true;
  }
  bool SendRtp(const uint8_t*, size_t, const webrtc::PacketOptions&) override {
    ADD_FAILURE() << "RtcpTransciver shouldn't send rtp packets.";
    return true;
  }

  // Returns true when packet was received by the transport.
  bool WaitPacket() {
    // Normally packet should be sent fast, long before the timeout.
    bool packet_sent = sent_rtcp_.Wait(kAlmostForeverMs);
    // Disallow tests to wait almost forever for no packets.
    EXPECT_TRUE(packet_sent);
    // Return wait result even though it is expected to be true, so that
    // individual tests can EXPECT on it for better error message.
    return packet_sent;
  }

 private:
  rtc::Event sent_rtcp_;
};

TEST(RtcpTransceiverImplTest, DelaysSendingFirstCompondPacket) {
  rtc::TaskQueue queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config;
  config.outgoing_transport = &transport;
  config.initial_report_delay_ms = 10;
  config.task_queue = &queue;
  rtc::Optional<RtcpTransceiverImpl> rtcp_transceiver;

  int64_t started_ms = rtc::TimeMillis();
  queue.PostTask([&] { rtcp_transceiver.emplace(config); });
  EXPECT_TRUE(transport.WaitPacket());

  EXPECT_GE(rtc::TimeMillis() - started_ms, config.initial_report_delay_ms);

  // Cleanup.
  rtc::Event done(false, false);
  queue.PostTask([&] {
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(kAlmostForeverMs));
}

TEST(RtcpTransceiverImplTest, PeriodicallySendsPackets) {
  rtc::TaskQueue queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config;
  config.outgoing_transport = &transport;
  config.initial_report_delay_ms = 0;
  config.report_period_ms = kReportPeriodMs;
  config.task_queue = &queue;
  rtc::Optional<RtcpTransceiverImpl> rtcp_transceiver;
  int64_t time_just_before_1st_packet_ms = 0;
  queue.PostTask([&] {
    // Because initial_report_delay_ms is set to 0, time_just_before_the_packet
    // should be very close to the time_of_the_packet.
    time_just_before_1st_packet_ms = rtc::TimeMillis();
    rtcp_transceiver.emplace(config);
  });

  EXPECT_TRUE(transport.WaitPacket());
  EXPECT_TRUE(transport.WaitPacket());
  int64_t time_just_after_2nd_packet_ms = rtc::TimeMillis();

  EXPECT_GE(time_just_after_2nd_packet_ms - time_just_before_1st_packet_ms,
            config.report_period_ms);

  // Cleanup.
  rtc::Event done(false, false);
  queue.PostTask([&] {
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(kAlmostForeverMs));
}

TEST(RtcpTransceiverImplTest, SendCompoundPacketDelaysPeriodicSendPackets) {
  rtc::TaskQueue queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config;
  config.outgoing_transport = &transport;
  config.initial_report_delay_ms = 0;
  config.report_period_ms = kReportPeriodMs;
  config.task_queue = &queue;
  rtc::Optional<RtcpTransceiverImpl> rtcp_transceiver;
  queue.PostTask([&] { rtcp_transceiver.emplace(config); });

  // Wait for first packet.
  EXPECT_TRUE(transport.WaitPacket());
  // Send non periodic one after half period.
  rtc::Event non_periodic(false, false);
  int64_t time_of_non_periodic_packet_ms = 0;
  queue.PostDelayedTask(
      [&] {
        time_of_non_periodic_packet_ms = rtc::TimeMillis();
        rtcp_transceiver->SendCompoundPacket();
        non_periodic.Set();
      },
      config.report_period_ms / 2);
  // Though non-periodic packet is scheduled just in between periodic, due to
  // small period and task queue flakiness it migth end-up 1ms after next
  // periodic packet. To be sure duration after non-periodic packet is tested
  // wait for transport after ensuring non-periodic packet was sent.
  EXPECT_TRUE(non_periodic.Wait(kAlmostForeverMs));
  EXPECT_TRUE(transport.WaitPacket());
  // Wait for next periodic packet.
  EXPECT_TRUE(transport.WaitPacket());
  int64_t time_of_last_periodic_packet_ms = rtc::TimeMillis();

  EXPECT_GE(time_of_last_periodic_packet_ms - time_of_non_periodic_packet_ms,
            config.report_period_ms);

  // Cleanup.
  rtc::Event done(false, false);
  queue.PostTask([&] {
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(kAlmostForeverMs));
}

TEST(RtcpTransceiverImplTest, SendsMinimalCompoundPacket) {
  const uint32_t kSenderSsrc = 12345;
  MockTransport outgoing_transport;
  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  config.cname = "cname";
  config.outgoing_transport = &outgoing_transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  RtcpPacketParser rtcp_parser;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillOnce(Invoke(&rtcp_parser, &RtcpPacketParser::Parse));
  rtcp_transceiver.SendCompoundPacket();

  // Minimal compound RTCP packet contains sender or receiver report and sdes
  // with cname.
  ASSERT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  EXPECT_EQ(rtcp_parser.receiver_report()->sender_ssrc(), kSenderSsrc);
  ASSERT_GT(rtcp_parser.sdes()->num_packets(), 0);
  ASSERT_EQ(rtcp_parser.sdes()->chunks().size(), 1u);
  EXPECT_EQ(rtcp_parser.sdes()->chunks()[0].ssrc, kSenderSsrc);
  EXPECT_EQ(rtcp_parser.sdes()->chunks()[0].cname, config.cname);
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
  config.schedule_periodic_compound_packets = false;
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
