/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/rtp/transport_feedback_adapter.h"

#include <limits>
#include <memory>
#include <vector>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace webrtc {
namespace webrtc_cc {

namespace {
const PacedPacketInfo kPacingInfo0(0, 5, 2000);
const PacedPacketInfo kPacingInfo1(1, 8, 4000);
const PacedPacketInfo kPacingInfo2(2, 14, 7000);
const PacedPacketInfo kPacingInfo3(3, 20, 10000);
const PacedPacketInfo kPacingInfo4(4, 22, 10000);

void ComparePacketFeedbackVectors(const std::vector<PacketFeedback>& truth,
                                  const std::vector<PacketFeedback>& input) {
  ASSERT_EQ(truth.size(), input.size());
  size_t len = truth.size();
  // truth contains the input data for the test, and input is what will be
  // sent to the bandwidth estimator. truth.arrival_tims_ms is used to
  // populate the transport feedback messages. As these times may be changed
  // (because of resolution limits in the packets, and because of the time
  // base adjustment performed by the TransportFeedbackAdapter at the first
  // packet, the truth[x].arrival_time and input[x].arrival_time may not be
  // equal. However, the difference must be the same for all x.
  int64_t arrival_time_delta =
      truth[0].arrival_time_ms - input[0].arrival_time_ms;
  for (size_t i = 0; i < len; ++i) {
    RTC_CHECK(truth[i].arrival_time_ms != PacketFeedback::kNotReceived);
    if (input[i].arrival_time_ms != PacketFeedback::kNotReceived) {
      EXPECT_EQ(truth[i].arrival_time_ms,
                input[i].arrival_time_ms + arrival_time_delta);
    }
    EXPECT_EQ(truth[i].send_time_ms, input[i].send_time_ms);
    EXPECT_EQ(truth[i].sequence_number, input[i].sequence_number);
    EXPECT_EQ(truth[i].payload_size, input[i].payload_size);
    EXPECT_EQ(truth[i].pacing_info, input[i].pacing_info);
  }
}

PacketFeedback CreatePacketFeedback(int64_t arrival_time_ms,
                                    int64_t send_time_ms,
                                    int64_t sequence_number,
                                    size_t payload_size,
                                    const PacedPacketInfo& pacing_info) {
  PacketFeedback res;
  res.arrival_time_ms = arrival_time_ms;
  res.send_time_ms = send_time_ms;
  res.sequence_number = sequence_number;
  res.payload_size = payload_size;
  res.pacing_info = pacing_info;
  return res;
}

}  // namespace

namespace test {

class MockStreamFeedbackObserver : public webrtc::StreamFeedbackObserver {
 public:
  MOCK_METHOD1(OnPacketFeedbackVector,
               void(std::vector<StreamPacketInfo> packet_feedback_vector));
};

class TransportFeedbackAdapterTest : public ::testing::Test {
 public:
  TransportFeedbackAdapterTest() : clock_(0) {}

  virtual ~TransportFeedbackAdapterTest() {}

  virtual void SetUp() { adapter_.reset(new TransportFeedbackAdapter()); }

  virtual void TearDown() { adapter_.reset(); }

 protected:
  void OnReceivedEstimatedBitrate(uint32_t bitrate) {}

  void OnReceivedRtcpReceiverReport(const ReportBlockList& report_blocks,
                                    int64_t rtt,
                                    int64_t now_ms) {}

  void OnSentPacket(const PacketFeedback& packet_feedback) {
    RtpPacketSendInfo packet_info;
    packet_info.ssrc = kSsrc;
    packet_info.transport_sequence_number = packet_feedback.sequence_number;
    packet_info.rtp_sequence_number = 0;
    packet_info.has_rtp_sequence_number = true;
    packet_info.length = packet_feedback.payload_size;
    packet_info.pacing_info = packet_feedback.pacing_info;
    adapter_->AddPacket(RtpPacketSendInfo(packet_info), 0u,
                        Timestamp::ms(clock_.TimeInMilliseconds()));
    adapter_->ProcessSentPacket(rtc::SentPacket(packet_feedback.sequence_number,
                                                packet_feedback.send_time_ms,
                                                rtc::PacketInfo()));
  }

  static constexpr uint32_t kSsrc = 8492;

  SimulatedClock clock_;
  std::unique_ptr<TransportFeedbackAdapter> adapter_;
};

TEST_F(TransportFeedbackAdapterTest, ObserverSanity) {
  MockStreamFeedbackObserver mock;
  adapter_->RegisterStreamFeedbackObserver({kSsrc}, &mock);

  const std::vector<PacketFeedback> packets = {
      CreatePacketFeedback(100, 200, 0, 1000, kPacingInfo0),
      CreatePacketFeedback(110, 210, 1, 2000, kPacingInfo0),
      CreatePacketFeedback(120, 220, 2, 3000, kPacingInfo0)};

  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sequence_number,
                   packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : packets) {
    OnSentPacket(packet);
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                           packet.arrival_time_ms * 1000));
  }

  EXPECT_CALL(mock, OnPacketFeedbackVector(_)).Times(1);
  adapter_->ProcessTransportFeedback(
      feedback, Timestamp::ms(clock_.TimeInMilliseconds()));

  adapter_->DeRegisterStreamFeedbackObserver(&mock);

  const PacketFeedback new_packet =
      CreatePacketFeedback(130, 230, 3, 4000, kPacingInfo0);
  OnSentPacket(new_packet);

  rtcp::TransportFeedback second_feedback;
  second_feedback.SetBase(new_packet.sequence_number,
                          new_packet.arrival_time_ms * 1000);
  EXPECT_TRUE(second_feedback.AddReceivedPacket(
      new_packet.sequence_number, new_packet.arrival_time_ms * 1000));
  EXPECT_CALL(mock, OnPacketFeedbackVector(_)).Times(0);
  adapter_->ProcessTransportFeedback(
      second_feedback, Timestamp::ms(clock_.TimeInMilliseconds()));
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST_F(TransportFeedbackAdapterTest, ObserverDoubleRegistrationDeathTest) {
  MockStreamFeedbackObserver mock;
  adapter_->RegisterStreamFeedbackObserver({0}, &mock);
  EXPECT_DEATH(adapter_->RegisterStreamFeedbackObserver({0}, &mock), "");
  adapter_->DeRegisterStreamFeedbackObserver(&mock);
}

TEST_F(TransportFeedbackAdapterTest, ObserverMissingDeRegistrationDeathTest) {
  MockStreamFeedbackObserver mock;
  adapter_->RegisterStreamFeedbackObserver({0}, &mock);
  EXPECT_DEATH(adapter_.reset(), "");
  adapter_->DeRegisterStreamFeedbackObserver(&mock);
}
#endif

TEST_F(TransportFeedbackAdapterTest, AdaptsFeedbackAndPopulatesSendTimes) {
  std::vector<PacketFeedback> packets;
  packets.push_back(CreatePacketFeedback(100, 200, 0, 1500, kPacingInfo0));
  packets.push_back(CreatePacketFeedback(110, 210, 1, 1500, kPacingInfo0));
  packets.push_back(CreatePacketFeedback(120, 220, 2, 1500, kPacingInfo0));
  packets.push_back(CreatePacketFeedback(130, 230, 3, 1500, kPacingInfo1));
  packets.push_back(CreatePacketFeedback(140, 240, 4, 1500, kPacingInfo1));

  for (const PacketFeedback& packet : packets)
    OnSentPacket(packet);

  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sequence_number,
                   packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : packets) {
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                           packet.arrival_time_ms * 1000));
  }

  feedback.Build();

  adapter_->ProcessTransportFeedback(
      feedback, Timestamp::ms(clock_.TimeInMilliseconds()));
  ComparePacketFeedbackVectors(packets, adapter_->GetTransportFeedbackVector());
}

TEST_F(TransportFeedbackAdapterTest, FeedbackVectorReportsUnreceived) {
  std::vector<PacketFeedback> sent_packets = {
      CreatePacketFeedback(100, 220, 0, 1500, kPacingInfo0),
      CreatePacketFeedback(110, 210, 1, 1500, kPacingInfo0),
      CreatePacketFeedback(120, 220, 2, 1500, kPacingInfo0),
      CreatePacketFeedback(130, 230, 3, 1500, kPacingInfo0),
      CreatePacketFeedback(140, 240, 4, 1500, kPacingInfo0),
      CreatePacketFeedback(150, 250, 5, 1500, kPacingInfo0),
      CreatePacketFeedback(160, 260, 6, 1500, kPacingInfo0)};

  for (const PacketFeedback& packet : sent_packets)
    OnSentPacket(packet);

  // Note: Important to include the last packet, as only unreceived packets in
  // between received packets can be inferred.
  std::vector<PacketFeedback> received_packets = {
      sent_packets[0], sent_packets[2], sent_packets[6]};

  rtcp::TransportFeedback feedback;
  feedback.SetBase(received_packets[0].sequence_number,
                   received_packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : received_packets) {
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                           packet.arrival_time_ms * 1000));
  }

  feedback.Build();

  adapter_->ProcessTransportFeedback(
      feedback, Timestamp::ms(clock_.TimeInMilliseconds()));
  ComparePacketFeedbackVectors(sent_packets,
                               adapter_->GetTransportFeedbackVector());
}

TEST_F(TransportFeedbackAdapterTest, HandlesDroppedPackets) {
  std::vector<PacketFeedback> packets;
  packets.push_back(CreatePacketFeedback(100, 200, 0, 1500, kPacingInfo0));
  packets.push_back(CreatePacketFeedback(110, 210, 1, 1500, kPacingInfo1));
  packets.push_back(CreatePacketFeedback(120, 220, 2, 1500, kPacingInfo2));
  packets.push_back(CreatePacketFeedback(130, 230, 3, 1500, kPacingInfo3));
  packets.push_back(CreatePacketFeedback(140, 240, 4, 1500, kPacingInfo4));

  const uint16_t kSendSideDropBefore = 1;
  const uint16_t kReceiveSideDropAfter = 3;

  for (const PacketFeedback& packet : packets) {
    if (packet.sequence_number >= kSendSideDropBefore)
      OnSentPacket(packet);
  }

  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sequence_number,
                   packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : packets) {
    if (packet.sequence_number <= kReceiveSideDropAfter) {
      EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                             packet.arrival_time_ms * 1000));
    }
  }

  feedback.Build();

  std::vector<PacketFeedback> expected_packets(
      packets.begin() + kSendSideDropBefore,
      packets.begin() + kReceiveSideDropAfter + 1);
  // Packets that have timed out on the send-side have lost the
  // information stored on the send-side. And they will not be reported to
  // observers since we won't know that they come from the same networks.

  adapter_->ProcessTransportFeedback(
      feedback, Timestamp::ms(clock_.TimeInMilliseconds()));
  ComparePacketFeedbackVectors(expected_packets,
                               adapter_->GetTransportFeedbackVector());
}

TEST_F(TransportFeedbackAdapterTest, SendTimeWrapsBothWays) {
  int64_t kHighArrivalTimeMs = rtcp::TransportFeedback::kDeltaScaleFactor *
                               static_cast<int64_t>(1 << 8) *
                               static_cast<int64_t>((1 << 23) - 1) / 1000;
  std::vector<PacketFeedback> packets;
  packets.push_back(CreatePacketFeedback(kHighArrivalTimeMs - 64, 200, 0, 1500,
                                         PacedPacketInfo()));
  packets.push_back(CreatePacketFeedback(kHighArrivalTimeMs + 64, 210, 1, 1500,
                                         PacedPacketInfo()));
  packets.push_back(CreatePacketFeedback(kHighArrivalTimeMs, 220, 2, 1500,
                                         PacedPacketInfo()));

  for (const PacketFeedback& packet : packets)
    OnSentPacket(packet);

  for (size_t i = 0; i < packets.size(); ++i) {
    std::unique_ptr<rtcp::TransportFeedback> feedback(
        new rtcp::TransportFeedback());
    feedback->SetBase(packets[i].sequence_number,
                      packets[i].arrival_time_ms * 1000);

    EXPECT_TRUE(feedback->AddReceivedPacket(packets[i].sequence_number,
                                            packets[i].arrival_time_ms * 1000));

    rtc::Buffer raw_packet = feedback->Build();
    feedback = rtcp::TransportFeedback::ParseFrom(raw_packet.data(),
                                                  raw_packet.size());

    std::vector<PacketFeedback> expected_packets;
    expected_packets.push_back(packets[i]);

    adapter_->ProcessTransportFeedback(
        *feedback.get(), Timestamp::ms(clock_.TimeInMilliseconds()));
    ComparePacketFeedbackVectors(expected_packets,
                                 adapter_->GetTransportFeedbackVector());
  }
}

TEST_F(TransportFeedbackAdapterTest, HandlesArrivalReordering) {
  std::vector<PacketFeedback> packets;
  packets.push_back(CreatePacketFeedback(120, 200, 0, 1500, kPacingInfo0));
  packets.push_back(CreatePacketFeedback(110, 210, 1, 1500, kPacingInfo0));
  packets.push_back(CreatePacketFeedback(100, 220, 2, 1500, kPacingInfo0));

  for (const PacketFeedback& packet : packets)
    OnSentPacket(packet);

  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sequence_number,
                   packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : packets) {
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                           packet.arrival_time_ms * 1000));
  }

  feedback.Build();

  // Adapter keeps the packets ordered by sequence number (which is itself
  // assigned by the order of transmission). Reordering by some other criteria,
  // eg. arrival time, is up to the observers.
  adapter_->ProcessTransportFeedback(
      feedback, Timestamp::ms(clock_.TimeInMilliseconds()));
  ComparePacketFeedbackVectors(packets, adapter_->GetTransportFeedbackVector());
}

TEST_F(TransportFeedbackAdapterTest, TimestampDeltas) {
  std::vector<PacketFeedback> sent_packets;
  const int64_t kSmallDeltaUs =
      rtcp::TransportFeedback::kDeltaScaleFactor * ((1 << 8) - 1);
  const int64_t kLargePositiveDeltaUs =
      rtcp::TransportFeedback::kDeltaScaleFactor *
      std::numeric_limits<int16_t>::max();
  const int64_t kLargeNegativeDeltaUs =
      rtcp::TransportFeedback::kDeltaScaleFactor *
      std::numeric_limits<int16_t>::min();

  PacketFeedback packet_feedback;
  packet_feedback.sequence_number = 1;
  packet_feedback.send_time_ms = 100;
  packet_feedback.arrival_time_ms = 200;
  packet_feedback.payload_size = 1500;
  sent_packets.push_back(packet_feedback);

  packet_feedback.send_time_ms += kSmallDeltaUs / 1000;
  packet_feedback.arrival_time_ms += kSmallDeltaUs / 1000;
  ++packet_feedback.sequence_number;
  sent_packets.push_back(packet_feedback);

  packet_feedback.send_time_ms += kLargePositiveDeltaUs / 1000;
  packet_feedback.arrival_time_ms += kLargePositiveDeltaUs / 1000;
  ++packet_feedback.sequence_number;
  sent_packets.push_back(packet_feedback);

  packet_feedback.send_time_ms += kLargeNegativeDeltaUs / 1000;
  packet_feedback.arrival_time_ms += kLargeNegativeDeltaUs / 1000;
  ++packet_feedback.sequence_number;
  sent_packets.push_back(packet_feedback);

  // Too large, delta - will need two feedback messages.
  packet_feedback.send_time_ms += (kLargePositiveDeltaUs + 1000) / 1000;
  packet_feedback.arrival_time_ms += (kLargePositiveDeltaUs + 1000) / 1000;
  ++packet_feedback.sequence_number;

  // Packets will be added to send history.
  for (const PacketFeedback& packet : sent_packets)
    OnSentPacket(packet);
  OnSentPacket(packet_feedback);

  // Create expected feedback and send into adapter.
  std::unique_ptr<rtcp::TransportFeedback> feedback(
      new rtcp::TransportFeedback());
  feedback->SetBase(sent_packets[0].sequence_number,
                    sent_packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : sent_packets) {
    EXPECT_TRUE(feedback->AddReceivedPacket(packet.sequence_number,
                                            packet.arrival_time_ms * 1000));
  }
  EXPECT_FALSE(feedback->AddReceivedPacket(
      packet_feedback.sequence_number, packet_feedback.arrival_time_ms * 1000));

  rtc::Buffer raw_packet = feedback->Build();
  feedback =
      rtcp::TransportFeedback::ParseFrom(raw_packet.data(), raw_packet.size());

  std::vector<PacketFeedback> received_feedback;

  EXPECT_TRUE(feedback.get() != nullptr);
  adapter_->ProcessTransportFeedback(
      *feedback.get(), Timestamp::ms(clock_.TimeInMilliseconds()));
  ComparePacketFeedbackVectors(sent_packets,
                               adapter_->GetTransportFeedbackVector());

  // Create a new feedback message and add the trailing item.
  feedback.reset(new rtcp::TransportFeedback());
  feedback->SetBase(packet_feedback.sequence_number,
                    packet_feedback.arrival_time_ms * 1000);
  EXPECT_TRUE(feedback->AddReceivedPacket(
      packet_feedback.sequence_number, packet_feedback.arrival_time_ms * 1000));
  raw_packet = feedback->Build();
  feedback =
      rtcp::TransportFeedback::ParseFrom(raw_packet.data(), raw_packet.size());

  EXPECT_TRUE(feedback.get() != nullptr);
  adapter_->ProcessTransportFeedback(
      *feedback.get(), Timestamp::ms(clock_.TimeInMilliseconds()));
  {
    std::vector<PacketFeedback> expected_packets;
    expected_packets.push_back(packet_feedback);
    ComparePacketFeedbackVectors(expected_packets,
                                 adapter_->GetTransportFeedbackVector());
  }
}

TEST_F(TransportFeedbackAdapterTest, IgnoreDuplicatePacketSentCalls) {
  const PacketFeedback packet =
      CreatePacketFeedback(100, 200, 0, 1500, kPacingInfo0);

  // Add a packet and then mark it as sent.
  RtpPacketSendInfo packet_info;
  packet_info.ssrc = kSsrc;
  packet_info.transport_sequence_number = packet.sequence_number;
  packet_info.length = packet.payload_size;
  packet_info.pacing_info = packet.pacing_info;
  adapter_->AddPacket(packet_info, 0u,
                      Timestamp::ms(clock_.TimeInMilliseconds()));
  absl::optional<SentPacket> sent_packet =
      adapter_->ProcessSentPacket(rtc::SentPacket(
          packet.sequence_number, packet.send_time_ms, rtc::PacketInfo()));
  EXPECT_TRUE(sent_packet.has_value());

  // Call ProcessSentPacket() again with the same sequence number. This packet
  // has already been marked as sent and the call should be ignored.
  absl::optional<SentPacket> duplicate_packet =
      adapter_->ProcessSentPacket(rtc::SentPacket(
          packet.sequence_number, packet.send_time_ms, rtc::PacketInfo()));
  EXPECT_FALSE(duplicate_packet.has_value());
}

}  // namespace test
}  // namespace webrtc_cc
}  // namespace webrtc
