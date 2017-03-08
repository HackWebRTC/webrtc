/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>
#include <memory>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/modules/bitrate_controller/include/mock/mock_bitrate_controller.h"
#include "webrtc/modules/congestion_controller/transport_feedback_adapter.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace webrtc {

namespace {
const PacedPacketInfo kPacingInfo0(0, 5, 2000);
const PacedPacketInfo kPacingInfo1(1, 8, 4000);
const PacedPacketInfo kPacingInfo2(2, 14, 7000);
const PacedPacketInfo kPacingInfo3(3, 20, 10000);
const PacedPacketInfo kPacingInfo4(4, 22, 10000);
}

namespace test {

class TransportFeedbackAdapterTest : public ::testing::Test {
 public:
  TransportFeedbackAdapterTest()
      : clock_(0), bitrate_controller_(this), target_bitrate_bps_(0) {}

  virtual ~TransportFeedbackAdapterTest() {}

  virtual void SetUp() {
    adapter_.reset(
        new TransportFeedbackAdapter(nullptr, &clock_, &bitrate_controller_));
    adapter_->InitBwe();
    adapter_->SetStartBitrate(300000);
  }

  virtual void TearDown() { adapter_.reset(); }

 protected:
  // Proxy class used since TransportFeedbackAdapter will own the instance
  // passed at construction.
  class MockBitrateControllerAdapter : public MockBitrateController {
   public:
    explicit MockBitrateControllerAdapter(TransportFeedbackAdapterTest* owner)
        : MockBitrateController(), owner_(owner) {}

    ~MockBitrateControllerAdapter() override {}

    void OnDelayBasedBweResult(const DelayBasedBwe::Result& result) override {
      owner_->target_bitrate_bps_ = result.target_bitrate_bps;
    }

    TransportFeedbackAdapterTest* const owner_;
  };

  void OnReceivedEstimatedBitrate(uint32_t bitrate) {}

  void OnReceivedRtcpReceiverReport(const ReportBlockList& report_blocks,
                                    int64_t rtt,
                                    int64_t now_ms) {}

  void ComparePacketVectors(const std::vector<PacketFeedback>& truth,
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

  void OnSentPacket(const PacketFeedback& packet_feedback) {
    adapter_->AddPacket(packet_feedback.sequence_number,
                        packet_feedback.payload_size,
                        packet_feedback.pacing_info);
    adapter_->OnSentPacket(packet_feedback.sequence_number,
                           packet_feedback.send_time_ms);
  }

  SimulatedClock clock_;
  MockBitrateControllerAdapter bitrate_controller_;
  std::unique_ptr<TransportFeedbackAdapter> adapter_;

  uint32_t target_bitrate_bps_;
};

TEST_F(TransportFeedbackAdapterTest, AdaptsFeedbackAndPopulatesSendTimes) {
  std::vector<PacketFeedback> packets;
  packets.push_back(PacketFeedback(100, 200, 0, 1500, kPacingInfo0));
  packets.push_back(PacketFeedback(110, 210, 1, 1500, kPacingInfo0));
  packets.push_back(PacketFeedback(120, 220, 2, 1500, kPacingInfo0));
  packets.push_back(PacketFeedback(130, 230, 3, 1500, kPacingInfo1));
  packets.push_back(PacketFeedback(140, 240, 4, 1500, kPacingInfo1));

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

  adapter_->OnTransportFeedback(feedback);
  ComparePacketVectors(packets, adapter_->GetTransportFeedbackVector());
}

TEST_F(TransportFeedbackAdapterTest, FeedbackVectorReportsUnreceived) {
  std::vector<PacketFeedback> sent_packets = {
      PacketFeedback(100, 220, 0, 1500, kPacingInfo0),
      PacketFeedback(110, 210, 1, 1500, kPacingInfo0),
      PacketFeedback(120, 220, 2, 1500, kPacingInfo0),
      PacketFeedback(130, 230, 3, 1500, kPacingInfo0),
      PacketFeedback(140, 240, 4, 1500, kPacingInfo0),
      PacketFeedback(150, 250, 5, 1500, kPacingInfo0),
      PacketFeedback(160, 260, 6, 1500, kPacingInfo0)
  };

  for (const PacketFeedback& packet : sent_packets)
    OnSentPacket(packet);

  // Note: Important to include the last packet, as only unreceived packets in
  // between received packets can be inferred.
  std::vector<PacketFeedback> received_packets = {
    sent_packets[0], sent_packets[2], sent_packets[6]
  };

  rtcp::TransportFeedback feedback;
  feedback.SetBase(received_packets[0].sequence_number,
                   received_packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : received_packets) {
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                           packet.arrival_time_ms * 1000));
  }

  feedback.Build();

  adapter_->OnTransportFeedback(feedback);
  ComparePacketVectors(sent_packets, adapter_->GetTransportFeedbackVector());
}

TEST_F(TransportFeedbackAdapterTest, LongFeedbackDelays) {
  const int64_t kFeedbackTimeoutMs = 60001;
  const int kMaxConsecutiveFailedLookups = 5;
  for (int i = 0; i < kMaxConsecutiveFailedLookups; ++i) {
    std::vector<PacketFeedback> packets;
    packets.push_back(
        PacketFeedback(i * 100, 2 * i * 100, 0, 1500, kPacingInfo0));
    packets.push_back(
        PacketFeedback(i * 100 + 10, 2 * i * 100 + 10, 1, 1500, kPacingInfo0));
    packets.push_back(
        PacketFeedback(i * 100 + 20, 2 * i * 100 + 20, 2, 1500, kPacingInfo0));
    packets.push_back(
        PacketFeedback(i * 100 + 30, 2 * i * 100 + 30, 3, 1500, kPacingInfo1));
    packets.push_back(
        PacketFeedback(i * 100 + 40, 2 * i * 100 + 40, 4, 1500, kPacingInfo1));

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

    clock_.AdvanceTimeMilliseconds(kFeedbackTimeoutMs);
    PacketFeedback later_packet(kFeedbackTimeoutMs + i * 100 + 40,
                                kFeedbackTimeoutMs + i * 200 + 40, 5, 1500,
                                kPacingInfo1);
    OnSentPacket(later_packet);

    adapter_->OnTransportFeedback(feedback);

    // Check that packets have timed out.
    for (PacketFeedback& packet : packets) {
      packet.send_time_ms = -1;
      packet.payload_size = 0;
      packet.pacing_info = PacedPacketInfo();
    }
    ComparePacketVectors(packets, adapter_->GetTransportFeedbackVector());
  }

  // Target bitrate should have halved due to feedback delays.
  EXPECT_EQ(150000u, target_bitrate_bps_);

  // Test with feedback that isn't late enough to time out.
  {
    std::vector<PacketFeedback> packets;
    packets.push_back(PacketFeedback(100, 200, 0, 1500, kPacingInfo0));
    packets.push_back(PacketFeedback(110, 210, 1, 1500, kPacingInfo0));
    packets.push_back(PacketFeedback(120, 220, 2, 1500, kPacingInfo0));
    packets.push_back(PacketFeedback(130, 230, 3, 1500, kPacingInfo1));
    packets.push_back(PacketFeedback(140, 240, 4, 1500, kPacingInfo1));

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

    clock_.AdvanceTimeMilliseconds(kFeedbackTimeoutMs - 1);
    PacketFeedback later_packet(kFeedbackTimeoutMs + 140,
                                kFeedbackTimeoutMs + 240, 5, 1500,
                                kPacingInfo1);
    OnSentPacket(later_packet);

    adapter_->OnTransportFeedback(feedback);
    ComparePacketVectors(packets, adapter_->GetTransportFeedbackVector());
  }
}

TEST_F(TransportFeedbackAdapterTest, HandlesDroppedPackets) {
  std::vector<PacketFeedback> packets;
  packets.push_back(PacketFeedback(100, 200, 0, 1500, kPacingInfo0));
  packets.push_back(PacketFeedback(110, 210, 1, 1500, kPacingInfo1));
  packets.push_back(PacketFeedback(120, 220, 2, 1500, kPacingInfo2));
  packets.push_back(PacketFeedback(130, 230, 3, 1500, kPacingInfo3));
  packets.push_back(PacketFeedback(140, 240, 4, 1500, kPacingInfo4));

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
      packets.begin(), packets.begin() + kReceiveSideDropAfter + 1);
  // Packets that have timed out on the send-side have lost the
  // information stored on the send-side.
  for (size_t i = 0; i < kSendSideDropBefore; ++i) {
    expected_packets[i].send_time_ms = -1;
    expected_packets[i].payload_size = 0;
    expected_packets[i].pacing_info = PacedPacketInfo();
  }

  adapter_->OnTransportFeedback(feedback);
  ComparePacketVectors(expected_packets,
                       adapter_->GetTransportFeedbackVector());
}

TEST_F(TransportFeedbackAdapterTest, SendTimeWrapsBothWays) {
  int64_t kHighArrivalTimeMs = rtcp::TransportFeedback::kDeltaScaleFactor *
                               static_cast<int64_t>(1 << 8) *
                               static_cast<int64_t>((1 << 23) - 1) / 1000;
  std::vector<PacketFeedback> packets;
  packets.push_back(
      PacketFeedback(kHighArrivalTimeMs - 64, 200, 0, 1500, PacedPacketInfo()));
  packets.push_back(
      PacketFeedback(kHighArrivalTimeMs + 64, 210, 1, 1500, PacedPacketInfo()));
  packets.push_back(
      PacketFeedback(kHighArrivalTimeMs, 220, 2, 1500, PacedPacketInfo()));

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

    adapter_->OnTransportFeedback(*feedback.get());
    ComparePacketVectors(expected_packets,
                         adapter_->GetTransportFeedbackVector());
  }
}

TEST_F(TransportFeedbackAdapterTest, HandlesArrivalReordering) {
  std::vector<PacketFeedback> packets;
  packets.push_back(PacketFeedback(120, 200, 0, 1500, kPacingInfo0));
  packets.push_back(PacketFeedback(110, 210, 1, 1500, kPacingInfo0));
  packets.push_back(PacketFeedback(100, 220, 2, 1500, kPacingInfo0));

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
  adapter_->OnTransportFeedback(feedback);
  ComparePacketVectors(packets, adapter_->GetTransportFeedbackVector());
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

  PacketFeedback packet_feedback(100, 200, 0, 1500, true, PacedPacketInfo());
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
  adapter_->OnTransportFeedback(*feedback.get());
  ComparePacketVectors(sent_packets, adapter_->GetTransportFeedbackVector());

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
  adapter_->OnTransportFeedback(*feedback.get());
  {
    std::vector<PacketFeedback> expected_packets;
    expected_packets.push_back(packet_feedback);
    ComparePacketVectors(expected_packets,
                         adapter_->GetTransportFeedbackVector());
  }
}

TEST_F(TransportFeedbackAdapterTest, UpdatesDelayBasedEstimate) {
  uint16_t seq_num = 0;
  size_t kPayloadSize = 1000;
  // The test must run and insert packets/feedback long enough that the
  // BWE computes a valid estimate.
  const int64_t kRunTimeMs = 6000;
  int64_t start_time_ms = clock_.TimeInMilliseconds();
  while (clock_.TimeInMilliseconds() - start_time_ms < kRunTimeMs) {
    PacketFeedback packet(clock_.TimeInMilliseconds(),
                          clock_.TimeInMilliseconds(), seq_num, kPayloadSize,
                          PacedPacketInfo());
    OnSentPacket(packet);
    // Create expected feedback and send into adapter.
    std::unique_ptr<rtcp::TransportFeedback> feedback(
        new rtcp::TransportFeedback());
    feedback->SetBase(packet.sequence_number, packet.arrival_time_ms * 1000);
    EXPECT_TRUE(feedback->AddReceivedPacket(packet.sequence_number,
                                            packet.arrival_time_ms * 1000));
    rtc::Buffer raw_packet = feedback->Build();
    feedback = rtcp::TransportFeedback::ParseFrom(raw_packet.data(),
                                                  raw_packet.size());
    EXPECT_TRUE(feedback.get() != nullptr);
    adapter_->OnTransportFeedback(*feedback.get());
    clock_.AdvanceTimeMilliseconds(50);
    ++seq_num;
  }
  EXPECT_GT(target_bitrate_bps_, 0u);
}

}  // namespace test
}  // namespace webrtc
