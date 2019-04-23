/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "absl/memory/memory.h"
#include "api/transport/field_trial_based_config.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_timing.h"
#include "logging/rtc_event_log/events/rtc_event.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_sender.h"
#include "modules/rtp_rtcp/source/rtp_sender_video.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/rate_limiter.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"

namespace webrtc {

namespace {
enum : int {  // The first valid value is 1.
  kAbsoluteSendTimeExtensionId = 1,
  kAudioLevelExtensionId,
  kGenericDescriptorId00,
  kGenericDescriptorId01,
  kMidExtensionId,
  kRepairedRidExtensionId,
  kRidExtensionId,
  kTransmissionTimeOffsetExtensionId,
  kTransportSequenceNumberExtensionId,
  kVideoRotationExtensionId,
  kVideoTimingExtensionId,
};

const int kPayload = 100;
const int kRtxPayload = 98;
const uint32_t kTimestamp = 10;
const uint16_t kSeqNum = 33;
const uint32_t kSsrc = 725242;
const uint16_t kTransportSequenceNumber = 0xaabbu;
const uint64_t kStartTime = 123456789;
const size_t kMaxPaddingSize = 224u;
const uint8_t kPayloadData[] = {47, 11, 32, 93, 89};
const int64_t kDefaultExpectedRetransmissionTimeMs = 125;
const char kNoRid[] = "";
const char kNoMid[] = "";

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::SizeIs;

uint64_t ConvertMsToAbsSendTime(int64_t time_ms) {
  return (((time_ms << 18) + 500) / 1000) & 0x00ffffff;
}

class LoopbackTransportTest : public webrtc::Transport {
 public:
  LoopbackTransportTest() : total_bytes_sent_(0) {
    receivers_extensions_.Register(kRtpExtensionTransmissionTimeOffset,
                                   kTransmissionTimeOffsetExtensionId);
    receivers_extensions_.Register(kRtpExtensionAbsoluteSendTime,
                                   kAbsoluteSendTimeExtensionId);
    receivers_extensions_.Register(kRtpExtensionTransportSequenceNumber,
                                   kTransportSequenceNumberExtensionId);
    receivers_extensions_.Register(kRtpExtensionVideoRotation,
                                   kVideoRotationExtensionId);
    receivers_extensions_.Register(kRtpExtensionAudioLevel,
                                   kAudioLevelExtensionId);
    receivers_extensions_.Register(kRtpExtensionVideoTiming,
                                   kVideoTimingExtensionId);
    receivers_extensions_.Register(kRtpExtensionMid, kMidExtensionId);
    receivers_extensions_.Register(kRtpExtensionGenericFrameDescriptor00,
                                   kGenericDescriptorId00);
    receivers_extensions_.Register(kRtpExtensionGenericFrameDescriptor01,
                                   kGenericDescriptorId01);
    receivers_extensions_.Register(kRtpExtensionRtpStreamId, kRidExtensionId);
    receivers_extensions_.Register(kRtpExtensionRepairedRtpStreamId,
                                   kRepairedRidExtensionId);
  }

  bool SendRtp(const uint8_t* data,
               size_t len,
               const PacketOptions& options) override {
    last_options_ = options;
    total_bytes_sent_ += len;
    sent_packets_.push_back(RtpPacketReceived(&receivers_extensions_));
    EXPECT_TRUE(sent_packets_.back().Parse(data, len));
    return true;
  }
  bool SendRtcp(const uint8_t* data, size_t len) override { return false; }
  const RtpPacketReceived& last_sent_packet() { return sent_packets_.back(); }
  int packets_sent() { return sent_packets_.size(); }

  size_t total_bytes_sent_;
  PacketOptions last_options_;
  std::vector<RtpPacketReceived> sent_packets_;

 private:
  RtpHeaderExtensionMap receivers_extensions_;
};

MATCHER_P(SameRtcEventTypeAs, value, "") {
  return value == arg->GetType();
}

}  // namespace

class MockRtpPacketSender : public RtpPacketSender {
 public:
  MockRtpPacketSender() {}
  virtual ~MockRtpPacketSender() {}

  MOCK_METHOD6(InsertPacket,
               void(Priority priority,
                    uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_time_ms,
                    size_t bytes,
                    bool retransmission));
};

class MockTransportSequenceNumberAllocator
    : public TransportSequenceNumberAllocator {
 public:
  MOCK_METHOD0(AllocateSequenceNumber, uint16_t());
};

class MockSendSideDelayObserver : public SendSideDelayObserver {
 public:
  MOCK_METHOD3(SendSideDelayUpdated, void(int, int, uint32_t));
};

class MockSendPacketObserver : public SendPacketObserver {
 public:
  MOCK_METHOD3(OnSendPacket, void(uint16_t, int64_t, uint32_t));
};

class MockTransportFeedbackObserver : public TransportFeedbackObserver {
 public:
  MOCK_METHOD1(OnAddPacket, void(const RtpPacketSendInfo&));
  MOCK_METHOD1(OnTransportFeedback, void(const rtcp::TransportFeedback&));
  MOCK_CONST_METHOD0(GetTransportFeedbackVector, std::vector<PacketFeedback>());
};

class MockOverheadObserver : public OverheadObserver {
 public:
  MOCK_METHOD1(OnOverheadChanged, void(size_t overhead_bytes_per_packet));
};

class RtpSenderTest : public ::testing::TestWithParam<bool> {
 protected:
  RtpSenderTest()
      : fake_clock_(kStartTime),
        mock_rtc_event_log_(),
        mock_paced_sender_(),
        retransmission_rate_limiter_(&fake_clock_, 1000),
        rtp_sender_(),
        transport_(),
        kMarkerBit(true),
        field_trials_(GetParam() ? "WebRTC-SendSideBwe-WithOverhead/Enabled/"
                                 : "") {}

  void SetUp() override { SetUpRtpSender(true, false); }

  void SetUpRtpSender(bool pacer, bool populate_network2) {
    rtp_sender_.reset(new RTPSender(
        false, &fake_clock_, &transport_, pacer ? &mock_paced_sender_ : nullptr,
        absl::nullopt, &seq_num_allocator_, nullptr, nullptr, nullptr,
        &mock_rtc_event_log_, &send_packet_observer_,
        &retransmission_rate_limiter_, nullptr, populate_network2, nullptr,
        false, false, FieldTrialBasedConfig()));
    rtp_sender_->SetSequenceNumber(kSeqNum);
    rtp_sender_->SetTimestampOffset(0);
    rtp_sender_->SetSSRC(kSsrc);
  }

  SimulatedClock fake_clock_;
  ::testing::NiceMock<MockRtcEventLog> mock_rtc_event_log_;
  MockRtpPacketSender mock_paced_sender_;
  ::testing::StrictMock<MockTransportSequenceNumberAllocator>
      seq_num_allocator_;
  ::testing::StrictMock<MockSendPacketObserver> send_packet_observer_;
  ::testing::StrictMock<MockTransportFeedbackObserver> feedback_observer_;
  RateLimiter retransmission_rate_limiter_;
  std::unique_ptr<RTPSender> rtp_sender_;
  LoopbackTransportTest transport_;
  const bool kMarkerBit;
  test::ScopedFieldTrials field_trials_;

  std::unique_ptr<RtpPacketToSend> BuildRtpPacket(int payload_type,
                                                  bool marker_bit,
                                                  uint32_t timestamp,
                                                  int64_t capture_time_ms) {
    auto packet = rtp_sender_->AllocatePacket();
    packet->SetPayloadType(payload_type);
    packet->SetMarker(marker_bit);
    packet->SetTimestamp(timestamp);
    packet->set_capture_time_ms(capture_time_ms);
    EXPECT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
    return packet;
  }

  void SendPacket(int64_t capture_time_ms, int payload_length) {
    uint32_t timestamp = capture_time_ms * 90;
    auto packet =
        BuildRtpPacket(kPayload, kMarkerBit, timestamp, capture_time_ms);
    packet->AllocatePayload(payload_length);

    // Packet should be stored in a send bucket.
    EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                           kAllowRetransmission,
                                           RtpPacketSender::kNormalPriority));
  }

  void SendGenericPacket() {
    const int64_t kCaptureTimeMs = fake_clock_.TimeInMilliseconds();
    SendPacket(kCaptureTimeMs, sizeof(kPayloadData));
  }
};

// TODO(pbos): Move tests over from WithoutPacer to RtpSenderTest as this is our
// default code path.
class RtpSenderTestWithoutPacer : public RtpSenderTest {
 public:
  void SetUp() override { SetUpRtpSender(false, false); }
};

TEST_P(RtpSenderTestWithoutPacer, AllocatePacketSetCsrc) {
  // Configure rtp_sender with csrc.
  std::vector<uint32_t> csrcs;
  csrcs.push_back(0x23456789);
  rtp_sender_->SetCsrcs(csrcs);

  auto packet = rtp_sender_->AllocatePacket();

  ASSERT_TRUE(packet);
  EXPECT_EQ(rtp_sender_->SSRC(), packet->Ssrc());
  EXPECT_EQ(csrcs, packet->Csrcs());
}

TEST_P(RtpSenderTestWithoutPacer, AllocatePacketReserveExtensions) {
  // Configure rtp_sender with extensions.
  ASSERT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  ASSERT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  ASSERT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));
  ASSERT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  ASSERT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));

  auto packet = rtp_sender_->AllocatePacket();

  ASSERT_TRUE(packet);
  // Preallocate BWE extensions RtpSender set itself.
  EXPECT_TRUE(packet->HasExtension<TransmissionOffset>());
  EXPECT_TRUE(packet->HasExtension<AbsoluteSendTime>());
  EXPECT_TRUE(packet->HasExtension<TransportSequenceNumber>());
  // Do not allocate media specific extensions.
  EXPECT_FALSE(packet->HasExtension<AudioLevel>());
  EXPECT_FALSE(packet->HasExtension<VideoOrientation>());
}

TEST_P(RtpSenderTestWithoutPacer, AssignSequenceNumberAdvanceSequenceNumber) {
  auto packet = rtp_sender_->AllocatePacket();
  ASSERT_TRUE(packet);
  const uint16_t sequence_number = rtp_sender_->SequenceNumber();

  EXPECT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));

  EXPECT_EQ(sequence_number, packet->SequenceNumber());
  EXPECT_EQ(sequence_number + 1, rtp_sender_->SequenceNumber());
}

TEST_P(RtpSenderTestWithoutPacer, AssignSequenceNumberFailsOnNotSending) {
  auto packet = rtp_sender_->AllocatePacket();
  ASSERT_TRUE(packet);

  rtp_sender_->SetSendingMediaStatus(false);
  EXPECT_FALSE(rtp_sender_->AssignSequenceNumber(packet.get()));
}

TEST_P(RtpSenderTestWithoutPacer, AssignSequenceNumberMayAllowPaddingOnVideo) {
  constexpr size_t kPaddingSize = 100;
  auto packet = rtp_sender_->AllocatePacket();
  ASSERT_TRUE(packet);

  ASSERT_FALSE(rtp_sender_->TimeToSendPadding(kPaddingSize, PacedPacketInfo()));
  packet->SetMarker(false);
  ASSERT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
  // Packet without marker bit doesn't allow padding on video stream.
  EXPECT_FALSE(rtp_sender_->TimeToSendPadding(kPaddingSize, PacedPacketInfo()));

  packet->SetMarker(true);
  ASSERT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
  // Packet with marker bit allows send padding.
  EXPECT_TRUE(rtp_sender_->TimeToSendPadding(kPaddingSize, PacedPacketInfo()));
}

TEST_P(RtpSenderTest, AssignSequenceNumberAllowsPaddingOnAudio) {
  MockTransport transport;
  const bool kEnableAudio = true;
  rtp_sender_.reset(new RTPSender(
      kEnableAudio, &fake_clock_, &transport, &mock_paced_sender_,
      absl::nullopt, nullptr, nullptr, nullptr, nullptr, &mock_rtc_event_log_,
      nullptr, &retransmission_rate_limiter_, nullptr, false, nullptr, false,
      false, FieldTrialBasedConfig()));
  rtp_sender_->SetTimestampOffset(0);
  rtp_sender_->SetSSRC(kSsrc);

  std::unique_ptr<RtpPacketToSend> audio_packet = rtp_sender_->AllocatePacket();
  // Padding on audio stream allowed regardless of marker in the last packet.
  audio_packet->SetMarker(false);
  audio_packet->SetPayloadType(kPayload);
  rtp_sender_->AssignSequenceNumber(audio_packet.get());

  const size_t kPaddingSize = 59;
  EXPECT_CALL(transport, SendRtp(_, kPaddingSize + kRtpHeaderSize, _))
      .WillOnce(::testing::Return(true));
  EXPECT_EQ(kPaddingSize,
            rtp_sender_->TimeToSendPadding(kPaddingSize, PacedPacketInfo()));

  // Requested padding size is too small, will send a larger one.
  const size_t kMinPaddingSize = 50;
  EXPECT_CALL(transport, SendRtp(_, kMinPaddingSize + kRtpHeaderSize, _))
      .WillOnce(::testing::Return(true));
  EXPECT_EQ(kMinPaddingSize, rtp_sender_->TimeToSendPadding(kMinPaddingSize - 5,
                                                            PacedPacketInfo()));
}

TEST_P(RtpSenderTestWithoutPacer, AssignSequenceNumberSetPaddingTimestamps) {
  constexpr size_t kPaddingSize = 100;
  auto packet = rtp_sender_->AllocatePacket();
  ASSERT_TRUE(packet);
  packet->SetMarker(true);
  packet->SetTimestamp(kTimestamp);

  ASSERT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
  ASSERT_TRUE(rtp_sender_->TimeToSendPadding(kPaddingSize, PacedPacketInfo()));

  ASSERT_EQ(1u, transport_.sent_packets_.size());
  // Verify padding packet timestamp.
  EXPECT_EQ(kTimestamp, transport_.last_sent_packet().Timestamp());
}

TEST_P(RtpSenderTestWithoutPacer,
       TransportFeedbackObserverGetsCorrectByteCount) {
  constexpr int kRtpOverheadBytesPerPacket = 12 + 8;
  ::testing::NiceMock<MockOverheadObserver> mock_overhead_observer;
  rtp_sender_.reset(
      new RTPSender(false, &fake_clock_, &transport_, nullptr, absl::nullopt,
                    &seq_num_allocator_, &feedback_observer_, nullptr, nullptr,
                    &mock_rtc_event_log_, nullptr,
                    &retransmission_rate_limiter_, &mock_overhead_observer,
                    false, nullptr, false, false, FieldTrialBasedConfig()));
  rtp_sender_->SetSSRC(kSsrc);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(::testing::Return(kTransportSequenceNumber));

  const size_t expected_bytes =
      GetParam() ? sizeof(kPayloadData) + kRtpOverheadBytesPerPacket
                 : sizeof(kPayloadData);

  EXPECT_CALL(feedback_observer_,
              OnAddPacket(AllOf(
                  Field(&RtpPacketSendInfo::ssrc, rtp_sender_->SSRC()),
                  Field(&RtpPacketSendInfo::transport_sequence_number,
                        kTransportSequenceNumber),
                  Field(&RtpPacketSendInfo::rtp_sequence_number,
                        rtp_sender_->SequenceNumber()),
                  Field(&RtpPacketSendInfo::length, expected_bytes),
                  Field(&RtpPacketSendInfo::pacing_info, PacedPacketInfo()))))
      .Times(1);
  EXPECT_CALL(mock_overhead_observer,
              OnOverheadChanged(kRtpOverheadBytesPerPacket))
      .Times(1);
  SendGenericPacket();
}

TEST_P(RtpSenderTestWithoutPacer, SendsPacketsWithTransportSequenceNumber) {
  rtp_sender_.reset(
      new RTPSender(false, &fake_clock_, &transport_, nullptr, absl::nullopt,
                    &seq_num_allocator_, &feedback_observer_, nullptr, nullptr,
                    &mock_rtc_event_log_, &send_packet_observer_,
                    &retransmission_rate_limiter_, nullptr, false, nullptr,
                    false, false, FieldTrialBasedConfig()));
  rtp_sender_->SetSSRC(kSsrc);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(::testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);

  EXPECT_CALL(feedback_observer_,
              OnAddPacket(AllOf(
                  Field(&RtpPacketSendInfo::ssrc, rtp_sender_->SSRC()),
                  Field(&RtpPacketSendInfo::transport_sequence_number,
                        kTransportSequenceNumber),
                  Field(&RtpPacketSendInfo::rtp_sequence_number,
                        rtp_sender_->SequenceNumber()),
                  Field(&RtpPacketSendInfo::pacing_info, PacedPacketInfo()))))
      .Times(1);

  SendGenericPacket();

  const auto& packet = transport_.last_sent_packet();
  uint16_t transport_seq_no;
  ASSERT_TRUE(packet.GetExtension<TransportSequenceNumber>(&transport_seq_no));
  EXPECT_EQ(kTransportSequenceNumber, transport_seq_no);
  EXPECT_EQ(transport_.last_options_.packet_id, transport_seq_no);
  EXPECT_TRUE(transport_.last_options_.included_in_allocation);
}

TEST_P(RtpSenderTestWithoutPacer, PacketOptionsNoRetransmission) {
  rtp_sender_.reset(
      new RTPSender(false, &fake_clock_, &transport_, nullptr, absl::nullopt,
                    &seq_num_allocator_, &feedback_observer_, nullptr, nullptr,
                    &mock_rtc_event_log_, &send_packet_observer_,
                    &retransmission_rate_limiter_, nullptr, false, nullptr,
                    false, false, FieldTrialBasedConfig()));
  rtp_sender_->SetSSRC(kSsrc);

  SendGenericPacket();

  EXPECT_FALSE(transport_.last_options_.is_retransmit);
}

TEST_P(RtpSenderTestWithoutPacer,
       SetsIncludedInFeedbackWhenTransportSequenceNumberExtensionIsRegistered) {
  SetUpRtpSender(false, false);
  rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionTransportSequenceNumber,
                                          kTransportSequenceNumberExtensionId);
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(::testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(send_packet_observer_, OnSendPacket).Times(1);
  SendGenericPacket();
  EXPECT_TRUE(transport_.last_options_.included_in_feedback);
}

TEST_P(
    RtpSenderTestWithoutPacer,
    SetsIncludedInAllocationWhenTransportSequenceNumberExtensionIsRegistered) {
  SetUpRtpSender(false, false);
  rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionTransportSequenceNumber,
                                          kTransportSequenceNumberExtensionId);
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(::testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(send_packet_observer_, OnSendPacket).Times(1);
  SendGenericPacket();
  EXPECT_TRUE(transport_.last_options_.included_in_allocation);
}

TEST_P(RtpSenderTestWithoutPacer,
       SetsIncludedInAllocationWhenForcedAsPartOfAllocation) {
  SetUpRtpSender(false, false);
  rtp_sender_->SetAsPartOfAllocation(true);
  SendGenericPacket();
  EXPECT_FALSE(transport_.last_options_.included_in_feedback);
  EXPECT_TRUE(transport_.last_options_.included_in_allocation);
}

TEST_P(RtpSenderTestWithoutPacer, DoesnSetIncludedInAllocationByDefault) {
  SetUpRtpSender(false, false);
  SendGenericPacket();
  EXPECT_FALSE(transport_.last_options_.included_in_feedback);
  EXPECT_FALSE(transport_.last_options_.included_in_allocation);
}

TEST_P(RtpSenderTestWithoutPacer, OnSendSideDelayUpdated) {
  ::testing::StrictMock<MockSendSideDelayObserver> send_side_delay_observer_;
  rtp_sender_.reset(
      new RTPSender(false, &fake_clock_, &transport_, nullptr, absl::nullopt,
                    nullptr, nullptr, nullptr, &send_side_delay_observer_,
                    &mock_rtc_event_log_, nullptr, nullptr, nullptr, false,
                    nullptr, false, false, FieldTrialBasedConfig()));
  rtp_sender_->SetSSRC(kSsrc);
  PlayoutDelayOracle playout_delay_oracle;
  RTPSenderVideo rtp_sender_video(&fake_clock_, rtp_sender_.get(), nullptr,
                                  &playout_delay_oracle, nullptr, false,
                                  FieldTrialBasedConfig());

  const uint8_t kPayloadType = 127;
  const char payload_name[] = "GENERIC";

  rtp_sender_video.RegisterPayloadType(kPayloadType, payload_name);

  const uint32_t kCaptureTimeMsToRtpTimestamp = 90;  // 90 kHz clock
  RTPVideoHeader video_header;

  // Send packet with 10 ms send-side delay. The average and max should be 10
  // ms.
  EXPECT_CALL(send_side_delay_observer_, SendSideDelayUpdated(10, 10, kSsrc))
      .Times(1);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  fake_clock_.AdvanceTimeMilliseconds(10);
  EXPECT_TRUE(rtp_sender_video.SendVideo(
      VideoFrameType::kVideoFrameKey, kPayloadType,
      capture_time_ms * kCaptureTimeMsToRtpTimestamp, capture_time_ms,
      kPayloadData, sizeof(kPayloadData), nullptr, &video_header,
      kDefaultExpectedRetransmissionTimeMs));

  // Send another packet with 20 ms delay. The average
  // and max should be 15 and 20 ms respectively.
  EXPECT_CALL(send_side_delay_observer_, SendSideDelayUpdated(15, 20, kSsrc))
      .Times(1);
  fake_clock_.AdvanceTimeMilliseconds(10);
  EXPECT_TRUE(rtp_sender_video.SendVideo(
      VideoFrameType::kVideoFrameKey, kPayloadType,
      capture_time_ms * kCaptureTimeMsToRtpTimestamp, capture_time_ms,
      kPayloadData, sizeof(kPayloadData), nullptr, &video_header,
      kDefaultExpectedRetransmissionTimeMs));

  // Send another packet at the same time, which replaces the last packet.
  // Since this packet has 0 ms delay, the average is now 5 ms and max is 10 ms.
  // TODO(terelius): Is is not clear that this is the right behavior.
  EXPECT_CALL(send_side_delay_observer_, SendSideDelayUpdated(5, 10, kSsrc))
      .Times(1);
  capture_time_ms = fake_clock_.TimeInMilliseconds();
  EXPECT_TRUE(rtp_sender_video.SendVideo(
      VideoFrameType::kVideoFrameKey, kPayloadType,
      capture_time_ms * kCaptureTimeMsToRtpTimestamp, capture_time_ms,
      kPayloadData, sizeof(kPayloadData), nullptr, &video_header,
      kDefaultExpectedRetransmissionTimeMs));

  // Send a packet 1 second later. The earlier packets should have timed
  // out, so both max and average should be the delay of this packet.
  fake_clock_.AdvanceTimeMilliseconds(1000);
  capture_time_ms = fake_clock_.TimeInMilliseconds();
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_CALL(send_side_delay_observer_, SendSideDelayUpdated(1, 1, kSsrc))
      .Times(1);
  EXPECT_TRUE(rtp_sender_video.SendVideo(
      VideoFrameType::kVideoFrameKey, kPayloadType,
      capture_time_ms * kCaptureTimeMsToRtpTimestamp, capture_time_ms,
      kPayloadData, sizeof(kPayloadData), nullptr, &video_header,
      kDefaultExpectedRetransmissionTimeMs));
}

TEST_P(RtpSenderTestWithoutPacer, OnSendPacketUpdated) {
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(::testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);

  SendGenericPacket();
}

TEST_P(RtpSenderTest, SendsPacketsWithTransportSequenceNumber) {
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, &mock_paced_sender_, absl::nullopt,
      &seq_num_allocator_, &feedback_observer_, nullptr, nullptr,
      &mock_rtc_event_log_, &send_packet_observer_,
      &retransmission_rate_limiter_, nullptr, false, nullptr, false, false,
      FieldTrialBasedConfig()));
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetSSRC(kSsrc);
  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, kSsrc, kSeqNum, _, _, _));
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(::testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);
  EXPECT_CALL(feedback_observer_,
              OnAddPacket(AllOf(
                  Field(&RtpPacketSendInfo::ssrc, rtp_sender_->SSRC()),
                  Field(&RtpPacketSendInfo::transport_sequence_number,
                        kTransportSequenceNumber),
                  Field(&RtpPacketSendInfo::rtp_sequence_number,
                        rtp_sender_->SequenceNumber()),
                  Field(&RtpPacketSendInfo::pacing_info, PacedPacketInfo()))))
      .Times(1);

  SendGenericPacket();
  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum,
                                fake_clock_.TimeInMilliseconds(), false,
                                PacedPacketInfo());

  const auto& packet = transport_.last_sent_packet();
  uint16_t transport_seq_no;
  EXPECT_TRUE(packet.GetExtension<TransportSequenceNumber>(&transport_seq_no));
  EXPECT_EQ(kTransportSequenceNumber, transport_seq_no);
  EXPECT_EQ(transport_.last_options_.packet_id, transport_seq_no);
}

TEST_P(RtpSenderTest, WritesPacerExitToTimingExtension) {
  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoTiming, kVideoTimingExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet = rtp_sender_->AllocatePacket();
  packet->SetPayloadType(kPayload);
  packet->SetMarker(true);
  packet->SetTimestamp(kTimestamp);
  packet->set_capture_time_ms(capture_time_ms);
  const VideoSendTiming kVideoTiming = {0u, 0u, 0u, 0u, 0u, 0u, true};
  packet->SetExtension<VideoTimingExtension>(kVideoTiming);
  EXPECT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
  size_t packet_size = packet->size();

  const int kStoredTimeInMs = 100;
  {
    EXPECT_CALL(
        mock_paced_sender_,
        InsertPacket(RtpPacketSender::kNormalPriority, kSsrc, _, _, _, _));
    EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                           kAllowRetransmission,
                                           RtpPacketSender::kNormalPriority));
  }
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);
  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum, capture_time_ms, false,
                                PacedPacketInfo());
  EXPECT_EQ(1, transport_.packets_sent());
  EXPECT_EQ(packet_size, transport_.last_sent_packet().size());

  VideoSendTiming video_timing;
  EXPECT_TRUE(transport_.last_sent_packet().GetExtension<VideoTimingExtension>(
      &video_timing));
  EXPECT_EQ(kStoredTimeInMs, video_timing.pacer_exit_delta_ms);
}

TEST_P(RtpSenderTest, WritesNetwork2ToTimingExtensionWithPacer) {
  SetUpRtpSender(/*pacer=*/true, /*populate_network2=*/true);
  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoTiming, kVideoTimingExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet = rtp_sender_->AllocatePacket();
  packet->SetPayloadType(kPayload);
  packet->SetMarker(true);
  packet->SetTimestamp(kTimestamp);
  packet->set_capture_time_ms(capture_time_ms);
  const uint16_t kPacerExitMs = 1234u;
  const VideoSendTiming kVideoTiming = {0u, 0u, 0u, kPacerExitMs, 0u, 0u, true};
  packet->SetExtension<VideoTimingExtension>(kVideoTiming);
  EXPECT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
  size_t packet_size = packet->size();

  const int kStoredTimeInMs = 100;
  {
    EXPECT_CALL(
        mock_paced_sender_,
        InsertPacket(RtpPacketSender::kNormalPriority, kSsrc, _, _, _, _));
    EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                           kAllowRetransmission,
                                           RtpPacketSender::kNormalPriority));
  }
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);
  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum, capture_time_ms, false,
                                PacedPacketInfo());
  EXPECT_EQ(1, transport_.packets_sent());
  EXPECT_EQ(packet_size, transport_.last_sent_packet().size());

  VideoSendTiming video_timing;
  EXPECT_TRUE(transport_.last_sent_packet().GetExtension<VideoTimingExtension>(
      &video_timing));
  EXPECT_EQ(kStoredTimeInMs, video_timing.network2_timestamp_delta_ms);
  EXPECT_EQ(kPacerExitMs, video_timing.pacer_exit_delta_ms);
}

TEST_P(RtpSenderTest, WritesNetwork2ToTimingExtensionWithoutPacer) {
  SetUpRtpSender(/*pacer=*/false, /*populate_network2=*/true);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoTiming, kVideoTimingExtensionId));
  auto packet = rtp_sender_->AllocatePacket();
  packet->SetMarker(true);
  packet->set_capture_time_ms(fake_clock_.TimeInMilliseconds());
  const VideoSendTiming kVideoTiming = {0u, 0u, 0u, 0u, 0u, 0u, true};
  packet->SetExtension<VideoTimingExtension>(kVideoTiming);
  EXPECT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));

  const int kPropagateTimeMs = 10;
  fake_clock_.AdvanceTimeMilliseconds(kPropagateTimeMs);

  EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                         kAllowRetransmission,
                                         RtpPacketSender::kNormalPriority));

  EXPECT_EQ(1, transport_.packets_sent());
  absl::optional<VideoSendTiming> video_timing =
      transport_.last_sent_packet().GetExtension<VideoTimingExtension>();
  ASSERT_TRUE(video_timing);
  EXPECT_EQ(kPropagateTimeMs, video_timing->network2_timestamp_delta_ms);
}

TEST_P(RtpSenderTest, TrafficSmoothingWithExtensions) {
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               kSsrc, kSeqNum, _, _, _));
  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)));

  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet =
      BuildRtpPacket(kPayload, kMarkerBit, kTimestamp, capture_time_ms);
  size_t packet_size = packet->size();

  // Packet should be stored in a send bucket.
  EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                         kAllowRetransmission,
                                         RtpPacketSender::kNormalPriority));

  EXPECT_EQ(0, transport_.packets_sent());

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);

  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum, capture_time_ms, false,
                                PacedPacketInfo());

  // Process send bucket. Packet should now be sent.
  EXPECT_EQ(1, transport_.packets_sent());
  EXPECT_EQ(packet_size, transport_.last_sent_packet().size());

  webrtc::RTPHeader rtp_header;
  transport_.last_sent_packet().GetHeader(&rtp_header);

  // Verify transmission time offset.
  EXPECT_EQ(kStoredTimeInMs * 90, rtp_header.extension.transmissionTimeOffset);
  uint64_t expected_send_time =
      ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
}

TEST_P(RtpSenderTest, TrafficSmoothingRetransmits) {
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               kSsrc, kSeqNum, _, _, _));
  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)));

  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet =
      BuildRtpPacket(kPayload, kMarkerBit, kTimestamp, capture_time_ms);
  size_t packet_size = packet->size();

  // Packet should be stored in a send bucket.
  EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                         kAllowRetransmission,
                                         RtpPacketSender::kNormalPriority));

  EXPECT_EQ(0, transport_.packets_sent());

  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               kSsrc, kSeqNum, _, _, _));

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);

  EXPECT_EQ(static_cast<int>(packet_size), rtp_sender_->ReSendPacket(kSeqNum));
  EXPECT_EQ(0, transport_.packets_sent());

  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum, capture_time_ms, false,
                                PacedPacketInfo());

  // Process send bucket. Packet should now be sent.
  EXPECT_EQ(1, transport_.packets_sent());
  EXPECT_EQ(packet_size, transport_.last_sent_packet().size());

  webrtc::RTPHeader rtp_header;
  transport_.last_sent_packet().GetHeader(&rtp_header);

  // Verify transmission time offset.
  EXPECT_EQ(kStoredTimeInMs * 90, rtp_header.extension.transmissionTimeOffset);
  uint64_t expected_send_time =
      ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
}

// This test sends 1 regular video packet, then 4 padding packets, and then
// 1 more regular packet.
TEST_P(RtpSenderTest, SendPadding) {
  // Make all (non-padding) packets go to send queue.
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               kSsrc, kSeqNum, _, _, _));
  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(1 + 4 + 1);

  uint16_t seq_num = kSeqNum;
  uint32_t timestamp = kTimestamp;
  rtp_sender_->SetStorePacketsStatus(true, 10);
  size_t rtp_header_len = kRtpHeaderSize;
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  rtp_header_len += 4;  // 4 extra bytes common to all extension headers.

  webrtc::RTPHeader rtp_header;

  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet =
      BuildRtpPacket(kPayload, kMarkerBit, timestamp, capture_time_ms);
  const uint32_t media_packet_timestamp = timestamp;
  size_t packet_size = packet->size();

  // Packet should be stored in a send bucket.
  EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                         kAllowRetransmission,
                                         RtpPacketSender::kNormalPriority));

  int total_packets_sent = 0;
  EXPECT_EQ(total_packets_sent, transport_.packets_sent());

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);
  rtp_sender_->TimeToSendPacket(kSsrc, seq_num++, capture_time_ms, false,
                                PacedPacketInfo());
  // Packet should now be sent. This test doesn't verify the regular video
  // packet, since it is tested in another test.
  EXPECT_EQ(++total_packets_sent, transport_.packets_sent());
  timestamp += 90 * kStoredTimeInMs;

  // Send padding 4 times, waiting 50 ms between each.
  for (int i = 0; i < 4; ++i) {
    const int kPaddingPeriodMs = 50;
    const size_t kPaddingBytes = 100;
    const size_t kMaxPaddingLength = 224;  // Value taken from rtp_sender.cc.
    // Padding will be forced to full packets.
    EXPECT_EQ(kMaxPaddingLength,
              rtp_sender_->TimeToSendPadding(kPaddingBytes, PacedPacketInfo()));

    // Process send bucket. Padding should now be sent.
    EXPECT_EQ(++total_packets_sent, transport_.packets_sent());
    EXPECT_EQ(kMaxPaddingLength + rtp_header_len,
              transport_.last_sent_packet().size());

    transport_.last_sent_packet().GetHeader(&rtp_header);
    EXPECT_EQ(kMaxPaddingLength, rtp_header.paddingLength);

    // Verify sequence number and timestamp. The timestamp should be the same
    // as the last media packet.
    EXPECT_EQ(seq_num++, rtp_header.sequenceNumber);
    EXPECT_EQ(media_packet_timestamp, rtp_header.timestamp);
    // Verify transmission time offset.
    int offset = timestamp - media_packet_timestamp;
    EXPECT_EQ(offset, rtp_header.extension.transmissionTimeOffset);
    uint64_t expected_send_time =
        ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
    EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
    fake_clock_.AdvanceTimeMilliseconds(kPaddingPeriodMs);
    timestamp += 90 * kPaddingPeriodMs;
  }

  // Send a regular video packet again.
  capture_time_ms = fake_clock_.TimeInMilliseconds();
  packet = BuildRtpPacket(kPayload, kMarkerBit, timestamp, capture_time_ms);
  packet_size = packet->size();

  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               kSsrc, seq_num, _, _, _));

  // Packet should be stored in a send bucket.
  EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                         kAllowRetransmission,
                                         RtpPacketSender::kNormalPriority));

  rtp_sender_->TimeToSendPacket(kSsrc, seq_num, capture_time_ms, false,
                                PacedPacketInfo());
  // Process send bucket.
  EXPECT_EQ(++total_packets_sent, transport_.packets_sent());
  EXPECT_EQ(packet_size, transport_.last_sent_packet().size());
  transport_.last_sent_packet().GetHeader(&rtp_header);

  // Verify sequence number and timestamp.
  EXPECT_EQ(seq_num, rtp_header.sequenceNumber);
  EXPECT_EQ(timestamp, rtp_header.timestamp);
  // Verify transmission time offset. This packet is sent without delay.
  EXPECT_EQ(0, rtp_header.extension.transmissionTimeOffset);
  uint64_t expected_send_time =
      ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
}

TEST_P(RtpSenderTest, OnSendPacketUpdated) {
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  rtp_sender_->SetStorePacketsStatus(true, 10);

  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(::testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, kSsrc, kSeqNum, _, _, _))
      .Times(1);

  SendGenericPacket();  // Packet passed to pacer.
  const bool kIsRetransmit = false;
  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum,
                                fake_clock_.TimeInMilliseconds(), kIsRetransmit,
                                PacedPacketInfo());
  EXPECT_EQ(1, transport_.packets_sent());
}

TEST_P(RtpSenderTest, OnSendPacketNotUpdatedForRetransmits) {
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  rtp_sender_->SetStorePacketsStatus(true, 10);

  EXPECT_CALL(send_packet_observer_, OnSendPacket(_, _, _)).Times(0);
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(::testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, kSsrc, kSeqNum, _, _, _))
      .Times(1);

  SendGenericPacket();  // Packet passed to pacer.
  const bool kIsRetransmit = true;
  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum,
                                fake_clock_.TimeInMilliseconds(), kIsRetransmit,
                                PacedPacketInfo());
  EXPECT_EQ(1, transport_.packets_sent());
  EXPECT_TRUE(transport_.last_options_.is_retransmit);
}

TEST_P(RtpSenderTest, OnSendPacketNotUpdatedWithoutSeqNumAllocator) {
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, &mock_paced_sender_, absl::nullopt,
      nullptr /* TransportSequenceNumberAllocator */, nullptr, nullptr, nullptr,
      nullptr, &send_packet_observer_, &retransmission_rate_limiter_, nullptr,
      false, nullptr, false, false, FieldTrialBasedConfig()));
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetSSRC(kSsrc);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetStorePacketsStatus(true, 10);

  EXPECT_CALL(send_packet_observer_, OnSendPacket(_, _, _)).Times(0);
  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, kSsrc, kSeqNum, _, _, _))
      .Times(1);

  SendGenericPacket();  // Packet passed to pacer.
  const bool kIsRetransmit = false;
  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum,
                                fake_clock_.TimeInMilliseconds(), kIsRetransmit,
                                PacedPacketInfo());
  EXPECT_EQ(1, transport_.packets_sent());
}

TEST_P(RtpSenderTest, SendRedundantPayloads) {
  MockTransport transport;
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport, &mock_paced_sender_, absl::nullopt,
      nullptr, nullptr, nullptr, nullptr, &mock_rtc_event_log_, nullptr,
      &retransmission_rate_limiter_, nullptr, false, nullptr, false, false,
      FieldTrialBasedConfig()));
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetSSRC(kSsrc);
  rtp_sender_->SetRtxPayloadType(kRtxPayload, kPayload);

  uint16_t seq_num = kSeqNum;
  rtp_sender_->SetStorePacketsStatus(true, 10);
  int32_t rtp_header_len = kRtpHeaderSize;
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  rtp_header_len += 4;  // 4 extra bytes common to all extension headers.

  rtp_sender_->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  rtp_sender_->SetRtxSsrc(1234);

  const size_t kNumPayloadSizes = 10;
  const size_t kPayloadSizes[kNumPayloadSizes] = {500, 550, 600, 650, 700,
                                                  750, 800, 850, 900, 950};
  // Expect all packets go through the pacer.
  EXPECT_CALL(mock_paced_sender_,
              InsertPacket(RtpPacketSender::kNormalPriority, kSsrc, _, _, _, _))
      .Times(kNumPayloadSizes);
  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(kNumPayloadSizes);

  // Send 10 packets of increasing size.
  for (size_t i = 0; i < kNumPayloadSizes; ++i) {
    int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
    EXPECT_CALL(transport, SendRtp(_, _, _)).WillOnce(::testing::Return(true));
    SendPacket(capture_time_ms, kPayloadSizes[i]);
    rtp_sender_->TimeToSendPacket(kSsrc, seq_num++, capture_time_ms, false,
                                  PacedPacketInfo());
    fake_clock_.AdvanceTimeMilliseconds(33);
  }

  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(::testing::AtLeast(4));

  // The amount of padding to send it too small to send a payload packet.
  EXPECT_CALL(transport, SendRtp(_, kMaxPaddingSize + rtp_header_len, _))
      .WillOnce(::testing::Return(true));
  EXPECT_EQ(kMaxPaddingSize,
            rtp_sender_->TimeToSendPadding(49, PacedPacketInfo()));

  PacketOptions options;
  EXPECT_CALL(transport,
              SendRtp(_, kPayloadSizes[0] + rtp_header_len + kRtxHeaderSize, _))
      .WillOnce(::testing::DoAll(::testing::SaveArg<2>(&options),
                                 ::testing::Return(true)));
  EXPECT_EQ(kPayloadSizes[0],
            rtp_sender_->TimeToSendPadding(500, PacedPacketInfo()));
  EXPECT_TRUE(options.is_retransmit);

  EXPECT_CALL(transport, SendRtp(_,
                                 kPayloadSizes[kNumPayloadSizes - 1] +
                                     rtp_header_len + kRtxHeaderSize,
                                 _))
      .WillOnce(::testing::Return(true));

  options.is_retransmit = false;
  EXPECT_CALL(transport, SendRtp(_, kMaxPaddingSize + rtp_header_len, _))
      .WillOnce(::testing::DoAll(::testing::SaveArg<2>(&options),
                                 ::testing::Return(true)));
  EXPECT_EQ(kPayloadSizes[kNumPayloadSizes - 1] + kMaxPaddingSize,
            rtp_sender_->TimeToSendPadding(999, PacedPacketInfo()));
  EXPECT_FALSE(options.is_retransmit);
}

TEST_P(RtpSenderTestWithoutPacer, SendGenericVideo) {
  const char payload_name[] = "GENERIC";
  const uint8_t payload_type = 127;
  PlayoutDelayOracle playout_delay_oracle;
  RTPSenderVideo rtp_sender_video(&fake_clock_, rtp_sender_.get(), nullptr,
                                  &playout_delay_oracle, nullptr, false,
                                  FieldTrialBasedConfig());
  rtp_sender_video.RegisterPayloadType(payload_type, payload_name);
  uint8_t payload[] = {47, 11, 32, 93, 89};

  // Send keyframe
  RTPVideoHeader video_header;
  ASSERT_TRUE(rtp_sender_video.SendVideo(
      VideoFrameType::kVideoFrameKey, payload_type, 1234, 4321, payload,
      sizeof(payload), nullptr, &video_header,
      kDefaultExpectedRetransmissionTimeMs));

  auto sent_payload = transport_.last_sent_packet().payload();
  uint8_t generic_header = sent_payload[0];
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kKeyFrameBit);
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kFirstPacketBit);
  EXPECT_THAT(sent_payload.subview(1), ElementsAreArray(payload));

  // Send delta frame
  payload[0] = 13;
  payload[1] = 42;
  payload[4] = 13;

  ASSERT_TRUE(rtp_sender_video.SendVideo(
      VideoFrameType::kVideoFrameDelta, payload_type, 1234, 4321, payload,
      sizeof(payload), nullptr, &video_header,
      kDefaultExpectedRetransmissionTimeMs));

  sent_payload = transport_.last_sent_packet().payload();
  generic_header = sent_payload[0];
  EXPECT_FALSE(generic_header & RtpFormatVideoGeneric::kKeyFrameBit);
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kFirstPacketBit);
  EXPECT_THAT(sent_payload.subview(1), ElementsAreArray(payload));
}

TEST_P(RtpSenderTest, SendFlexfecPackets) {
  constexpr uint32_t kTimestamp = 1234;
  constexpr int kMediaPayloadType = 127;
  constexpr int kFlexfecPayloadType = 118;
  constexpr uint32_t kMediaSsrc = 1234;
  constexpr uint32_t kFlexfecSsrc = 5678;
  const std::vector<RtpExtension> kNoRtpExtensions;
  const std::vector<RtpExtensionSize> kNoRtpExtensionSizes;
  FlexfecSender flexfec_sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                               kNoMid, kNoRtpExtensions, kNoRtpExtensionSizes,
                               nullptr /* rtp_state */, &fake_clock_);

  // Reset |rtp_sender_| to use FlexFEC.
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, &mock_paced_sender_, kFlexfecSsrc,
      &seq_num_allocator_, nullptr, nullptr, nullptr, &mock_rtc_event_log_,
      &send_packet_observer_, &retransmission_rate_limiter_, nullptr, false,
      nullptr, false, false, FieldTrialBasedConfig()));
  rtp_sender_->SetSSRC(kMediaSsrc);
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetStorePacketsStatus(true, 10);

  PlayoutDelayOracle playout_delay_oracle;
  RTPSenderVideo rtp_sender_video(&fake_clock_, rtp_sender_.get(),
                                  &flexfec_sender, &playout_delay_oracle,
                                  nullptr, false, FieldTrialBasedConfig());
  rtp_sender_video.RegisterPayloadType(kMediaPayloadType, "GENERIC");

  // Parameters selected to generate a single FEC packet per media packet.
  FecProtectionParams params;
  params.fec_rate = 15;
  params.max_fec_frames = 1;
  params.fec_mask_type = kFecMaskRandom;
  rtp_sender_video.SetFecParameters(params, params);

  EXPECT_CALL(mock_paced_sender_,
              InsertPacket(RtpPacketSender::kLowPriority, kMediaSsrc, kSeqNum,
                           _, _, false));
  uint16_t flexfec_seq_num;
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kLowPriority,
                                               kFlexfecSsrc, _, _, _, false))
      .WillOnce(::testing::SaveArg<2>(&flexfec_seq_num));

  RTPVideoHeader video_header;
  EXPECT_TRUE(rtp_sender_video.SendVideo(
      VideoFrameType::kVideoFrameKey, kMediaPayloadType, kTimestamp,
      fake_clock_.TimeInMilliseconds(), kPayloadData, sizeof(kPayloadData),
      nullptr, &video_header, kDefaultExpectedRetransmissionTimeMs));

  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(2);
  EXPECT_TRUE(rtp_sender_->TimeToSendPacket(kMediaSsrc, kSeqNum,
                                            fake_clock_.TimeInMilliseconds(),
                                            false, PacedPacketInfo()));
  EXPECT_TRUE(rtp_sender_->TimeToSendPacket(kFlexfecSsrc, flexfec_seq_num,
                                            fake_clock_.TimeInMilliseconds(),
                                            false, PacedPacketInfo()));
  ASSERT_EQ(2, transport_.packets_sent());
  const RtpPacketReceived& media_packet = transport_.sent_packets_[0];
  EXPECT_EQ(kMediaPayloadType, media_packet.PayloadType());
  EXPECT_EQ(kSeqNum, media_packet.SequenceNumber());
  EXPECT_EQ(kMediaSsrc, media_packet.Ssrc());
  const RtpPacketReceived& flexfec_packet = transport_.sent_packets_[1];
  EXPECT_EQ(kFlexfecPayloadType, flexfec_packet.PayloadType());
  EXPECT_EQ(flexfec_seq_num, flexfec_packet.SequenceNumber());
  EXPECT_EQ(kFlexfecSsrc, flexfec_packet.Ssrc());
}

// TODO(ilnik): because of webrtc:7859. Once FEC moved below pacer, this test
// should be removed.
TEST_P(RtpSenderTest, NoFlexfecForTimingFrames) {
  constexpr uint32_t kTimestamp = 1234;
  const int64_t kCaptureTimeMs = fake_clock_.TimeInMilliseconds();
  constexpr int kMediaPayloadType = 127;
  constexpr int kFlexfecPayloadType = 118;
  constexpr uint32_t kMediaSsrc = 1234;
  constexpr uint32_t kFlexfecSsrc = 5678;
  const std::vector<RtpExtension> kNoRtpExtensions;
  const std::vector<RtpExtensionSize> kNoRtpExtensionSizes;

  FlexfecSender flexfec_sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                               kNoMid, kNoRtpExtensions, kNoRtpExtensionSizes,
                               nullptr /* rtp_state */, &fake_clock_);

  // Reset |rtp_sender_| to use FlexFEC.
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, &mock_paced_sender_,
      flexfec_sender.ssrc(), &seq_num_allocator_, nullptr, nullptr, nullptr,
      &mock_rtc_event_log_, &send_packet_observer_,
      &retransmission_rate_limiter_, nullptr, false, nullptr, false, false,
      FieldTrialBasedConfig()));
  rtp_sender_->SetSSRC(kMediaSsrc);
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetStorePacketsStatus(true, 10);

  PlayoutDelayOracle playout_delay_oracle;
  RTPSenderVideo rtp_sender_video(&fake_clock_, rtp_sender_.get(),
                                  &flexfec_sender, &playout_delay_oracle,
                                  nullptr, false, FieldTrialBasedConfig());
  rtp_sender_video.RegisterPayloadType(kMediaPayloadType, "GENERIC");

  // Need extension to be registered for timing frames to be sent.
  ASSERT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoTiming, kVideoTimingExtensionId));

  // Parameters selected to generate a single FEC packet per media packet.
  FecProtectionParams params;
  params.fec_rate = 15;
  params.max_fec_frames = 1;
  params.fec_mask_type = kFecMaskRandom;
  rtp_sender_video.SetFecParameters(params, params);

  EXPECT_CALL(mock_paced_sender_,
              InsertPacket(RtpPacketSender::kLowPriority, kMediaSsrc, kSeqNum,
                           _, _, false));
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kLowPriority,
                                               kFlexfecSsrc, _, _, _, false))
      .Times(0);  // Not called because packet should not be protected.

  RTPVideoHeader video_header;
  video_header.video_timing.flags = VideoSendTiming::kTriggeredByTimer;
  EXPECT_TRUE(rtp_sender_video.SendVideo(
      VideoFrameType::kVideoFrameKey, kMediaPayloadType, kTimestamp,
      kCaptureTimeMs, kPayloadData, sizeof(kPayloadData), nullptr,
      &video_header, kDefaultExpectedRetransmissionTimeMs));

  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(1);
  EXPECT_TRUE(rtp_sender_->TimeToSendPacket(kMediaSsrc, kSeqNum,
                                            fake_clock_.TimeInMilliseconds(),
                                            false, PacedPacketInfo()));
  ASSERT_EQ(1, transport_.packets_sent());
  const RtpPacketReceived& media_packet = transport_.sent_packets_[0];
  EXPECT_EQ(kMediaPayloadType, media_packet.PayloadType());
  EXPECT_EQ(kSeqNum, media_packet.SequenceNumber());
  EXPECT_EQ(kMediaSsrc, media_packet.Ssrc());

  // Now try to send not a timing frame.
  uint16_t flexfec_seq_num;
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kLowPriority,
                                               kFlexfecSsrc, _, _, _, false))
      .WillOnce(::testing::SaveArg<2>(&flexfec_seq_num));
  EXPECT_CALL(mock_paced_sender_,
              InsertPacket(RtpPacketSender::kLowPriority, kMediaSsrc,
                           kSeqNum + 1, _, _, false));
  video_header.video_timing.flags = VideoSendTiming::kInvalid;
  EXPECT_TRUE(rtp_sender_video.SendVideo(
      VideoFrameType::kVideoFrameKey, kMediaPayloadType, kTimestamp + 1,
      kCaptureTimeMs + 1, kPayloadData, sizeof(kPayloadData), nullptr,
      &video_header, kDefaultExpectedRetransmissionTimeMs));

  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(2);
  EXPECT_TRUE(rtp_sender_->TimeToSendPacket(kMediaSsrc, kSeqNum + 1,
                                            fake_clock_.TimeInMilliseconds(),
                                            false, PacedPacketInfo()));
  EXPECT_TRUE(rtp_sender_->TimeToSendPacket(kFlexfecSsrc, flexfec_seq_num,
                                            fake_clock_.TimeInMilliseconds(),
                                            false, PacedPacketInfo()));
  ASSERT_EQ(3, transport_.packets_sent());
  const RtpPacketReceived& media_packet2 = transport_.sent_packets_[1];
  EXPECT_EQ(kMediaPayloadType, media_packet2.PayloadType());
  EXPECT_EQ(kSeqNum + 1, media_packet2.SequenceNumber());
  EXPECT_EQ(kMediaSsrc, media_packet2.Ssrc());
  const RtpPacketReceived& flexfec_packet = transport_.sent_packets_[2];
  EXPECT_EQ(kFlexfecPayloadType, flexfec_packet.PayloadType());
  EXPECT_EQ(flexfec_seq_num, flexfec_packet.SequenceNumber());
  EXPECT_EQ(kFlexfecSsrc, flexfec_packet.Ssrc());
}

TEST_P(RtpSenderTestWithoutPacer, SendFlexfecPackets) {
  constexpr uint32_t kTimestamp = 1234;
  constexpr int kMediaPayloadType = 127;
  constexpr int kFlexfecPayloadType = 118;
  constexpr uint32_t kMediaSsrc = 1234;
  constexpr uint32_t kFlexfecSsrc = 5678;
  const std::vector<RtpExtension> kNoRtpExtensions;
  const std::vector<RtpExtensionSize> kNoRtpExtensionSizes;
  FlexfecSender flexfec_sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                               kNoMid, kNoRtpExtensions, kNoRtpExtensionSizes,
                               nullptr /* rtp_state */, &fake_clock_);

  // Reset |rtp_sender_| to use FlexFEC.
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, nullptr, flexfec_sender.ssrc(),
      &seq_num_allocator_, nullptr, nullptr, nullptr, &mock_rtc_event_log_,
      &send_packet_observer_, &retransmission_rate_limiter_, nullptr, false,
      nullptr, false, false, FieldTrialBasedConfig()));
  rtp_sender_->SetSSRC(kMediaSsrc);
  rtp_sender_->SetSequenceNumber(kSeqNum);

  PlayoutDelayOracle playout_delay_oracle;
  RTPSenderVideo rtp_sender_video(&fake_clock_, rtp_sender_.get(),
                                  &flexfec_sender, &playout_delay_oracle,
                                  nullptr, false, FieldTrialBasedConfig());
  rtp_sender_video.RegisterPayloadType(kMediaPayloadType, "GENERIC");

  // Parameters selected to generate a single FEC packet per media packet.
  FecProtectionParams params;
  params.fec_rate = 15;
  params.max_fec_frames = 1;
  params.fec_mask_type = kFecMaskRandom;
  rtp_sender_video.SetFecParameters(params, params);

  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(2);
  RTPVideoHeader video_header;
  EXPECT_TRUE(rtp_sender_video.SendVideo(
      VideoFrameType::kVideoFrameKey, kMediaPayloadType, kTimestamp,
      fake_clock_.TimeInMilliseconds(), kPayloadData, sizeof(kPayloadData),
      nullptr, &video_header, kDefaultExpectedRetransmissionTimeMs));

  ASSERT_EQ(2, transport_.packets_sent());
  const RtpPacketReceived& media_packet = transport_.sent_packets_[0];
  EXPECT_EQ(kMediaPayloadType, media_packet.PayloadType());
  EXPECT_EQ(kMediaSsrc, media_packet.Ssrc());
  const RtpPacketReceived& flexfec_packet = transport_.sent_packets_[1];
  EXPECT_EQ(kFlexfecPayloadType, flexfec_packet.PayloadType());
  EXPECT_EQ(kFlexfecSsrc, flexfec_packet.Ssrc());
}

// Test that the MID header extension is included on sent packets when
// configured.
TEST_P(RtpSenderTestWithoutPacer, MidIncludedOnSentPackets) {
  const char kMid[] = "mid";

  // Register MID header extension and set the MID for the RTPSender.
  rtp_sender_->SetSendingMediaStatus(false);
  rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionMid, kMidExtensionId);
  rtp_sender_->SetMid(kMid);
  rtp_sender_->SetSendingMediaStatus(true);

  // Send a couple packets.
  SendGenericPacket();
  SendGenericPacket();

  // Expect both packets to have the MID set.
  ASSERT_EQ(2u, transport_.sent_packets_.size());
  for (const RtpPacketReceived& packet : transport_.sent_packets_) {
    std::string mid;
    ASSERT_TRUE(packet.GetExtension<RtpMid>(&mid));
    EXPECT_EQ(kMid, mid);
  }
}

TEST_P(RtpSenderTestWithoutPacer, RidIncludedOnSentPackets) {
  const char kRid[] = "f";

  rtp_sender_->SetSendingMediaStatus(false);
  rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionRtpStreamId,
                                          kRidExtensionId);
  rtp_sender_->SetRid(kRid);
  rtp_sender_->SetSendingMediaStatus(true);

  SendGenericPacket();

  ASSERT_EQ(1u, transport_.sent_packets_.size());
  const RtpPacketReceived& packet = transport_.sent_packets_[0];
  std::string rid;
  ASSERT_TRUE(packet.GetExtension<RtpStreamId>(&rid));
  EXPECT_EQ(kRid, rid);
}

TEST_P(RtpSenderTestWithoutPacer, RidIncludedOnRtxSentPackets) {
  const char kRid[] = "f";

  rtp_sender_->SetSendingMediaStatus(false);
  rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionRtpStreamId,
                                          kRidExtensionId);
  rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionRepairedRtpStreamId,
                                          kRepairedRidExtensionId);
  rtp_sender_->SetRid(kRid);
  rtp_sender_->SetSendingMediaStatus(true);

  rtp_sender_->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  rtp_sender_->SetRtxSsrc(1234);
  rtp_sender_->SetRtxPayloadType(kRtxPayload, kPayload);

  rtp_sender_->SetStorePacketsStatus(true, 10);

  SendGenericPacket();
  ASSERT_EQ(1u, transport_.sent_packets_.size());
  const RtpPacketReceived& packet = transport_.sent_packets_[0];
  std::string rid;
  ASSERT_TRUE(packet.GetExtension<RtpStreamId>(&rid));
  EXPECT_EQ(kRid, rid);
  rid = kNoRid;
  EXPECT_FALSE(packet.GetExtension<RepairedRtpStreamId>(&rid));

  uint16_t packet_id = packet.SequenceNumber();
  rtp_sender_->ReSendPacket(packet_id);
  ASSERT_EQ(2u, transport_.sent_packets_.size());
  const RtpPacketReceived& rtx_packet = transport_.sent_packets_[1];
  ASSERT_TRUE(rtx_packet.GetExtension<RepairedRtpStreamId>(&rid));
  EXPECT_EQ(kRid, rid);
  EXPECT_FALSE(rtx_packet.HasExtension<RtpStreamId>());
}

TEST_P(RtpSenderTest, FecOverheadRate) {
  constexpr uint32_t kTimestamp = 1234;
  constexpr int kMediaPayloadType = 127;
  constexpr int kFlexfecPayloadType = 118;
  constexpr uint32_t kMediaSsrc = 1234;
  constexpr uint32_t kFlexfecSsrc = 5678;
  const std::vector<RtpExtension> kNoRtpExtensions;
  const std::vector<RtpExtensionSize> kNoRtpExtensionSizes;
  FlexfecSender flexfec_sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                               kNoMid, kNoRtpExtensions, kNoRtpExtensionSizes,
                               nullptr /* rtp_state */, &fake_clock_);

  // Reset |rtp_sender_| to use FlexFEC.
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, &mock_paced_sender_,
      flexfec_sender.ssrc(), &seq_num_allocator_, nullptr, nullptr, nullptr,
      &mock_rtc_event_log_, &send_packet_observer_,
      &retransmission_rate_limiter_, nullptr, false, nullptr, false, false,
      FieldTrialBasedConfig()));
  rtp_sender_->SetSSRC(kMediaSsrc);
  rtp_sender_->SetSequenceNumber(kSeqNum);

  PlayoutDelayOracle playout_delay_oracle;
  RTPSenderVideo rtp_sender_video(&fake_clock_, rtp_sender_.get(),
                                  &flexfec_sender, &playout_delay_oracle,
                                  nullptr, false, FieldTrialBasedConfig());
  rtp_sender_video.RegisterPayloadType(kMediaPayloadType, "GENERIC");
  // Parameters selected to generate a single FEC packet per media packet.
  FecProtectionParams params;
  params.fec_rate = 15;
  params.max_fec_frames = 1;
  params.fec_mask_type = kFecMaskRandom;
  rtp_sender_video.SetFecParameters(params, params);

  constexpr size_t kNumMediaPackets = 10;
  constexpr size_t kNumFecPackets = kNumMediaPackets;
  constexpr int64_t kTimeBetweenPacketsMs = 10;
  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, _, _, _, _, false))
      .Times(kNumMediaPackets + kNumFecPackets);
  for (size_t i = 0; i < kNumMediaPackets; ++i) {
    RTPVideoHeader video_header;

    EXPECT_TRUE(rtp_sender_video.SendVideo(
        VideoFrameType::kVideoFrameKey, kMediaPayloadType, kTimestamp,
        fake_clock_.TimeInMilliseconds(), kPayloadData, sizeof(kPayloadData),
        nullptr, &video_header, kDefaultExpectedRetransmissionTimeMs));

    fake_clock_.AdvanceTimeMilliseconds(kTimeBetweenPacketsMs);
  }
  constexpr size_t kRtpHeaderLength = 12;
  constexpr size_t kFlexfecHeaderLength = 20;
  constexpr size_t kGenericCodecHeaderLength = 1;
  constexpr size_t kPayloadLength = sizeof(kPayloadData);
  constexpr size_t kPacketLength = kRtpHeaderLength + kFlexfecHeaderLength +
                                   kGenericCodecHeaderLength + kPayloadLength;
  EXPECT_NEAR(kNumFecPackets * kPacketLength * 8 /
                  (kNumFecPackets * kTimeBetweenPacketsMs / 1000.0f),
              rtp_sender_video.FecOverheadRate(), 500);
}

TEST_P(RtpSenderTest, BitrateCallbacks) {
  class TestCallback : public BitrateStatisticsObserver {
   public:
    TestCallback()
        : BitrateStatisticsObserver(),
          num_calls_(0),
          ssrc_(0),
          total_bitrate_(0),
          retransmit_bitrate_(0) {}
    ~TestCallback() override = default;

    void Notify(uint32_t total_bitrate,
                uint32_t retransmit_bitrate,
                uint32_t ssrc) override {
      ++num_calls_;
      ssrc_ = ssrc;
      total_bitrate_ = total_bitrate;
      retransmit_bitrate_ = retransmit_bitrate;
    }

    uint32_t num_calls_;
    uint32_t ssrc_;
    uint32_t total_bitrate_;
    uint32_t retransmit_bitrate_;
  } callback;
  rtp_sender_.reset(
      new RTPSender(false, &fake_clock_, &transport_, nullptr, absl::nullopt,
                    nullptr, nullptr, &callback, nullptr, nullptr, nullptr,
                    &retransmission_rate_limiter_, nullptr, false, nullptr,
                    false, false, FieldTrialBasedConfig()));
  rtp_sender_->SetSSRC(kSsrc);

  PlayoutDelayOracle playout_delay_oracle;
  RTPSenderVideo rtp_sender_video(&fake_clock_, rtp_sender_.get(), nullptr,
                                  &playout_delay_oracle, nullptr, false,
                                  FieldTrialBasedConfig());
  const char payload_name[] = "GENERIC";
  const uint8_t payload_type = 127;
  rtp_sender_video.RegisterPayloadType(payload_type, payload_name);

  // Simulate kNumPackets sent with kPacketInterval ms intervals, with the
  // number of packets selected so that we fill (but don't overflow) the one
  // second averaging window.
  const uint32_t kWindowSizeMs = 1000;
  const uint32_t kPacketInterval = 20;
  const uint32_t kNumPackets =
      (kWindowSizeMs - kPacketInterval) / kPacketInterval;
  // Overhead = 12 bytes RTP header + 1 byte generic header.
  const uint32_t kPacketOverhead = 13;

  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  // Initial process call so we get a new time window.
  rtp_sender_->ProcessBitrate();

  // Send a few frames.
  RTPVideoHeader video_header;
  for (uint32_t i = 0; i < kNumPackets; ++i) {
    ASSERT_TRUE(rtp_sender_video.SendVideo(
        VideoFrameType::kVideoFrameKey, payload_type, 1234, 4321, payload,
        sizeof(payload), nullptr, &video_header,
        kDefaultExpectedRetransmissionTimeMs));
    fake_clock_.AdvanceTimeMilliseconds(kPacketInterval);
  }

  rtp_sender_->ProcessBitrate();

  // We get one call for every stats updated, thus two calls since both the
  // stream stats and the retransmit stats are updated once.
  EXPECT_EQ(2u, callback.num_calls_);
  EXPECT_EQ(ssrc, callback.ssrc_);
  const uint32_t kTotalPacketSize = kPacketOverhead + sizeof(payload);
  // Bitrate measured over delta between last and first timestamp, plus one.
  const uint32_t kExpectedWindowMs = kNumPackets * kPacketInterval + 1;
  const uint32_t kExpectedBitsAccumulated = kTotalPacketSize * kNumPackets * 8;
  const uint32_t kExpectedRateBps =
      (kExpectedBitsAccumulated * 1000 + (kExpectedWindowMs / 2)) /
      kExpectedWindowMs;
  EXPECT_EQ(kExpectedRateBps, callback.total_bitrate_);

  rtp_sender_.reset();
}

TEST_P(RtpSenderTestWithoutPacer, StreamDataCountersCallbacks) {
  class TestCallback : public StreamDataCountersCallback {
   public:
    TestCallback() : StreamDataCountersCallback(), ssrc_(0), counters_() {}
    ~TestCallback() override = default;

    void DataCountersUpdated(const StreamDataCounters& counters,
                             uint32_t ssrc) override {
      ssrc_ = ssrc;
      counters_ = counters;
    }

    uint32_t ssrc_;
    StreamDataCounters counters_;

    void MatchPacketCounter(const RtpPacketCounter& expected,
                            const RtpPacketCounter& actual) {
      EXPECT_EQ(expected.payload_bytes, actual.payload_bytes);
      EXPECT_EQ(expected.header_bytes, actual.header_bytes);
      EXPECT_EQ(expected.padding_bytes, actual.padding_bytes);
      EXPECT_EQ(expected.packets, actual.packets);
    }

    void Matches(uint32_t ssrc, const StreamDataCounters& counters) {
      EXPECT_EQ(ssrc, ssrc_);
      MatchPacketCounter(counters.transmitted, counters_.transmitted);
      MatchPacketCounter(counters.retransmitted, counters_.retransmitted);
      EXPECT_EQ(counters.fec.packets, counters_.fec.packets);
    }
  } callback;

  const uint8_t kRedPayloadType = 96;
  const uint8_t kUlpfecPayloadType = 97;
  const char payload_name[] = "GENERIC";
  const uint8_t payload_type = 127;
  PlayoutDelayOracle playout_delay_oracle;
  RTPSenderVideo rtp_sender_video(&fake_clock_, rtp_sender_.get(), nullptr,
                                  &playout_delay_oracle, nullptr, false,
                                  FieldTrialBasedConfig());
  rtp_sender_video.RegisterPayloadType(payload_type, payload_name);
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  rtp_sender_->RegisterRtpStatisticsCallback(&callback);

  // Send a frame.
  RTPVideoHeader video_header;
  ASSERT_TRUE(rtp_sender_video.SendVideo(
      VideoFrameType::kVideoFrameKey, payload_type, 1234, 4321, payload,
      sizeof(payload), nullptr, &video_header,
      kDefaultExpectedRetransmissionTimeMs));
  StreamDataCounters expected;
  expected.transmitted.payload_bytes = 6;
  expected.transmitted.header_bytes = 12;
  expected.transmitted.padding_bytes = 0;
  expected.transmitted.packets = 1;
  expected.retransmitted.payload_bytes = 0;
  expected.retransmitted.header_bytes = 0;
  expected.retransmitted.padding_bytes = 0;
  expected.retransmitted.packets = 0;
  expected.fec.packets = 0;
  callback.Matches(ssrc, expected);

  // Retransmit a frame.
  uint16_t seqno = rtp_sender_->SequenceNumber() - 1;
  rtp_sender_->ReSendPacket(seqno);
  expected.transmitted.payload_bytes = 12;
  expected.transmitted.header_bytes = 24;
  expected.transmitted.packets = 2;
  expected.retransmitted.payload_bytes = 6;
  expected.retransmitted.header_bytes = 12;
  expected.retransmitted.padding_bytes = 0;
  expected.retransmitted.packets = 1;
  callback.Matches(ssrc, expected);

  // Send padding.
  rtp_sender_->TimeToSendPadding(kMaxPaddingSize, PacedPacketInfo());
  expected.transmitted.payload_bytes = 12;
  expected.transmitted.header_bytes = 36;
  expected.transmitted.padding_bytes = kMaxPaddingSize;
  expected.transmitted.packets = 3;
  callback.Matches(ssrc, expected);

  // Send ULPFEC.
  rtp_sender_video.SetUlpfecConfig(kRedPayloadType, kUlpfecPayloadType);
  FecProtectionParams fec_params;
  fec_params.fec_mask_type = kFecMaskRandom;
  fec_params.fec_rate = 1;
  fec_params.max_fec_frames = 1;
  rtp_sender_video.SetFecParameters(fec_params, fec_params);
  ASSERT_TRUE(rtp_sender_video.SendVideo(
      VideoFrameType::kVideoFrameDelta, payload_type, 1234, 4321, payload,
      sizeof(payload), nullptr, &video_header,
      kDefaultExpectedRetransmissionTimeMs));
  expected.transmitted.payload_bytes = 40;
  expected.transmitted.header_bytes = 60;
  expected.transmitted.packets = 5;
  expected.fec.packets = 1;
  callback.Matches(ssrc, expected);

  rtp_sender_->RegisterRtpStatisticsCallback(nullptr);
}

TEST_P(RtpSenderTestWithoutPacer, BytesReportedCorrectly) {
  // XXX const char* kPayloadName = "GENERIC";
  const uint8_t kPayloadType = 127;
  rtp_sender_->SetSSRC(1234);
  rtp_sender_->SetRtxSsrc(4321);
  rtp_sender_->SetRtxPayloadType(kPayloadType - 1, kPayloadType);
  rtp_sender_->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);

  SendGenericPacket();
  // Will send 2 full-size padding packets.
  rtp_sender_->TimeToSendPadding(1, PacedPacketInfo());
  rtp_sender_->TimeToSendPadding(1, PacedPacketInfo());

  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  rtp_sender_->GetDataCounters(&rtp_stats, &rtx_stats);

  // Payload
  EXPECT_GT(rtp_stats.first_packet_time_ms, -1);
  EXPECT_EQ(rtp_stats.transmitted.payload_bytes, sizeof(kPayloadData));
  EXPECT_EQ(rtp_stats.transmitted.header_bytes, 12u);
  EXPECT_EQ(rtp_stats.transmitted.padding_bytes, 0u);
  EXPECT_EQ(rtx_stats.transmitted.payload_bytes, 0u);
  EXPECT_EQ(rtx_stats.transmitted.header_bytes, 24u);
  EXPECT_EQ(rtx_stats.transmitted.padding_bytes, 2 * kMaxPaddingSize);

  EXPECT_EQ(rtp_stats.transmitted.TotalBytes(),
            rtp_stats.transmitted.payload_bytes +
                rtp_stats.transmitted.header_bytes +
                rtp_stats.transmitted.padding_bytes);
  EXPECT_EQ(rtx_stats.transmitted.TotalBytes(),
            rtx_stats.transmitted.payload_bytes +
                rtx_stats.transmitted.header_bytes +
                rtx_stats.transmitted.padding_bytes);

  EXPECT_EQ(
      transport_.total_bytes_sent_,
      rtp_stats.transmitted.TotalBytes() + rtx_stats.transmitted.TotalBytes());
}

TEST_P(RtpSenderTestWithoutPacer, RespectsNackBitrateLimit) {
  const int32_t kPacketSize = 1400;
  const int32_t kNumPackets = 30;

  retransmission_rate_limiter_.SetMaxRate(kPacketSize * kNumPackets * 8);

  rtp_sender_->SetStorePacketsStatus(true, kNumPackets);
  const uint16_t kStartSequenceNumber = rtp_sender_->SequenceNumber();
  std::vector<uint16_t> sequence_numbers;
  for (int32_t i = 0; i < kNumPackets; ++i) {
    sequence_numbers.push_back(kStartSequenceNumber + i);
    fake_clock_.AdvanceTimeMilliseconds(1);
    SendPacket(fake_clock_.TimeInMilliseconds(), kPacketSize);
  }
  EXPECT_EQ(kNumPackets, transport_.packets_sent());

  fake_clock_.AdvanceTimeMilliseconds(1000 - kNumPackets);

  // Resending should work - brings the bandwidth up to the limit.
  // NACK bitrate is capped to the same bitrate as the encoder, since the max
  // protection overhead is 50% (see MediaOptimization::SetTargetRates).
  rtp_sender_->OnReceivedNack(sequence_numbers, 0);
  EXPECT_EQ(kNumPackets * 2, transport_.packets_sent());

  // Must be at least 5ms in between retransmission attempts.
  fake_clock_.AdvanceTimeMilliseconds(5);

  // Resending should not work, bandwidth exceeded.
  rtp_sender_->OnReceivedNack(sequence_numbers, 0);
  EXPECT_EQ(kNumPackets * 2, transport_.packets_sent());
}

TEST_P(RtpSenderTest, OnOverheadChanged) {
  MockOverheadObserver mock_overhead_observer;
  rtp_sender_.reset(
      new RTPSender(false, &fake_clock_, &transport_, nullptr, absl::nullopt,
                    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                    &retransmission_rate_limiter_, &mock_overhead_observer,
                    false, nullptr, false, false, FieldTrialBasedConfig()));
  rtp_sender_->SetSSRC(kSsrc);

  // RTP overhead is 12B.
  EXPECT_CALL(mock_overhead_observer, OnOverheadChanged(12)).Times(1);
  SendGenericPacket();

  rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                          kTransmissionTimeOffsetExtensionId);

  // TransmissionTimeOffset extension has a size of 8B.
  // 12B + 8B = 20B
  EXPECT_CALL(mock_overhead_observer, OnOverheadChanged(20)).Times(1);
  SendGenericPacket();
}

TEST_P(RtpSenderTest, DoesNotUpdateOverheadOnEqualSize) {
  MockOverheadObserver mock_overhead_observer;
  rtp_sender_.reset(
      new RTPSender(false, &fake_clock_, &transport_, nullptr, absl::nullopt,
                    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                    &retransmission_rate_limiter_, &mock_overhead_observer,
                    false, nullptr, false, false, FieldTrialBasedConfig()));
  rtp_sender_->SetSSRC(kSsrc);

  EXPECT_CALL(mock_overhead_observer, OnOverheadChanged(_)).Times(1);
  SendGenericPacket();
  SendGenericPacket();
}

INSTANTIATE_TEST_SUITE_P(WithAndWithoutOverhead,
                         RtpSenderTest,
                         ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(WithAndWithoutOverhead,
                         RtpSenderTestWithoutPacer,
                         ::testing::Bool());

}  // namespace webrtc
