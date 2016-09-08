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

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/rate_limiter.h"
#include "webrtc/call/mock/mock_rtc_event_log.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_cvo.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender_video.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/include/stl_util.h"
#include "webrtc/test/mock_transport.h"
#include "webrtc/typedefs.h"

namespace webrtc {

namespace {
const int kTransmissionTimeOffsetExtensionId = 1;
const int kAbsoluteSendTimeExtensionId = 14;
const int kTransportSequenceNumberExtensionId = 13;
const int kPayload = 100;
const int kRtxPayload = 98;
const uint32_t kTimestamp = 10;
const uint16_t kSeqNum = 33;
const int kTimeOffset = 22222;
const int kMaxPacketLength = 1500;
const uint32_t kAbsoluteSendTime = 0x00aabbcc;
const uint8_t kAudioLevel = 0x5a;
const uint16_t kTransportSequenceNumber = 0xaabbu;
const uint8_t kAudioLevelExtensionId = 9;
const int kAudioPayload = 103;
const uint64_t kStartTime = 123456789;
const size_t kMaxPaddingSize = 224u;
const int kVideoRotationExtensionId = 5;
const VideoRotation kRotation = kVideoRotation_270;
const size_t kGenericHeaderLength = 1;
const uint8_t kPayloadData[] = {47, 11, 32, 93, 89};

using ::testing::_;
using ::testing::ElementsAreArray;

const uint8_t* GetPayloadData(const RTPHeader& rtp_header,
                              const uint8_t* packet) {
  return packet + rtp_header.headerLength;
}

size_t GetPayloadDataLength(const RTPHeader& rtp_header,
                            const size_t packet_length) {
  return packet_length - rtp_header.headerLength - rtp_header.paddingLength;
}

uint64_t ConvertMsToAbsSendTime(int64_t time_ms) {
  return (((time_ms << 18) + 500) / 1000) & 0x00ffffff;
}

class LoopbackTransportTest : public webrtc::Transport {
 public:
  LoopbackTransportTest()
      : packets_sent_(0),
        last_sent_packet_len_(0),
        total_bytes_sent_(0),
        last_sent_packet_(nullptr),
        last_packet_id_(-1) {}

  ~LoopbackTransportTest() {
    STLDeleteContainerPointers(sent_packets_.begin(), sent_packets_.end());
  }
  bool SendRtp(const uint8_t* data,
               size_t len,
               const PacketOptions& options) override {
    packets_sent_++;
    rtc::Buffer* buffer =
        new rtc::Buffer(reinterpret_cast<const uint8_t*>(data), len);
    last_sent_packet_ = buffer->data();
    last_sent_packet_len_ = len;
    last_packet_id_ = options.packet_id;
    total_bytes_sent_ += len;
    sent_packets_.push_back(buffer);
    return true;
  }
  bool SendRtcp(const uint8_t* data, size_t len) override { return false; }
  int packets_sent_;
  size_t last_sent_packet_len_;
  size_t total_bytes_sent_;
  uint8_t* last_sent_packet_;
  int last_packet_id_;
  std::vector<rtc::Buffer*> sent_packets_;
};

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

class MockSendPacketObserver : public SendPacketObserver {
 public:
  MOCK_METHOD3(OnSendPacket, void(uint16_t, int64_t, uint32_t));
};

class MockTransportFeedbackObserver : public TransportFeedbackObserver {
 public:
  MOCK_METHOD3(AddPacket, void(uint16_t, size_t, int));
  MOCK_METHOD1(OnTransportFeedback, void(const rtcp::TransportFeedback&));
  MOCK_CONST_METHOD0(GetTransportFeedbackVector, std::vector<PacketInfo>());
};

class RtpSenderTest : public ::testing::Test {
 protected:
  RtpSenderTest()
      : fake_clock_(kStartTime),
        mock_rtc_event_log_(),
        mock_paced_sender_(),
        retransmission_rate_limiter_(&fake_clock_, 1000),
        rtp_sender_(),
        payload_(kPayload),
        transport_(),
        kMarkerBit(true) {}

  void SetUp() override { SetUpRtpSender(true); }

  void SetUpRtpSender(bool pacer) {
    rtp_sender_.reset(new RTPSender(
        false, &fake_clock_, &transport_, pacer ? &mock_paced_sender_ : nullptr,
        &seq_num_allocator_, nullptr, nullptr, nullptr, nullptr,
        &mock_rtc_event_log_, &send_packet_observer_,
        &retransmission_rate_limiter_));
    rtp_sender_->SetSequenceNumber(kSeqNum);
    rtp_sender_->SetSendPayloadType(kPayload);
    rtp_sender_->SetTimestampOffset(0);
  }

  SimulatedClock fake_clock_;
  testing::NiceMock<MockRtcEventLog> mock_rtc_event_log_;
  MockRtpPacketSender mock_paced_sender_;
  testing::StrictMock<MockTransportSequenceNumberAllocator> seq_num_allocator_;
  testing::StrictMock<MockSendPacketObserver> send_packet_observer_;
  testing::StrictMock<MockTransportFeedbackObserver> feedback_observer_;
  RateLimiter retransmission_rate_limiter_;
  std::unique_ptr<RTPSender> rtp_sender_;
  int payload_;
  LoopbackTransportTest transport_;
  const bool kMarkerBit;
  uint8_t packet_[kMaxPacketLength];

  void VerifyRTPHeaderCommon(const RTPHeader& rtp_header) {
    VerifyRTPHeaderCommon(rtp_header, kMarkerBit, 0);
  }

  void VerifyRTPHeaderCommon(const RTPHeader& rtp_header, bool marker_bit) {
    VerifyRTPHeaderCommon(rtp_header, marker_bit, 0);
  }

  void VerifyRTPHeaderCommon(const RTPHeader& rtp_header,
                             bool marker_bit,
                             uint8_t number_of_csrcs) {
    EXPECT_EQ(marker_bit, rtp_header.markerBit);
    EXPECT_EQ(payload_, rtp_header.payloadType);
    EXPECT_EQ(kSeqNum, rtp_header.sequenceNumber);
    EXPECT_EQ(kTimestamp, rtp_header.timestamp);
    EXPECT_EQ(rtp_sender_->SSRC(), rtp_header.ssrc);
    EXPECT_EQ(number_of_csrcs, rtp_header.numCSRCs);
    EXPECT_EQ(0U, rtp_header.paddingLength);
  }

  void SendPacket(int64_t capture_time_ms, int payload_length) {
    uint32_t timestamp = capture_time_ms * 90;
    int32_t rtp_length = rtp_sender_->BuildRtpHeader(
        packet_, kPayload, kMarkerBit, timestamp, capture_time_ms);
    ASSERT_GE(rtp_length, 0);

    // Packet should be stored in a send bucket.
    EXPECT_TRUE(rtp_sender_->SendToNetwork(
        packet_, payload_length, rtp_length, capture_time_ms,
        kAllowRetransmission, RtpPacketSender::kNormalPriority));
  }

  void SendGenericPayload() {
    const uint32_t kTimestamp = 1234;
    const uint8_t kPayloadType = 127;
    const int64_t kCaptureTimeMs = fake_clock_.TimeInMilliseconds();
    char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
    EXPECT_EQ(0, rtp_sender_->RegisterPayload(payload_name, kPayloadType, 90000,
                                              0, 1500));

    EXPECT_TRUE(rtp_sender_->SendOutgoingData(
        kVideoFrameKey, kPayloadType, kTimestamp, kCaptureTimeMs, kPayloadData,
        sizeof(kPayloadData), nullptr, nullptr, nullptr));
  }
};

// TODO(pbos): Move tests over from WithoutPacer to RtpSenderTest as this is our
// default code path.
class RtpSenderTestWithoutPacer : public RtpSenderTest {
 public:
  void SetUp() override { SetUpRtpSender(false); }
};

class RtpSenderVideoTest : public RtpSenderTest {
 protected:
  void SetUp() override {
    // TODO(pbos): Set up to use pacer.
    SetUpRtpSender(false);
    rtp_sender_video_.reset(
        new RTPSenderVideo(&fake_clock_, rtp_sender_.get()));
  }
  std::unique_ptr<RTPSenderVideo> rtp_sender_video_;

  void VerifyCVOPacket(uint8_t* data,
                       size_t len,
                       bool expect_cvo,
                       RtpHeaderExtensionMap* map,
                       uint16_t seq_num,
                       VideoRotation rotation) {
    webrtc::RtpUtility::RtpHeaderParser rtp_parser(data, len);

    webrtc::RTPHeader rtp_header;
    size_t length = static_cast<size_t>(rtp_sender_->BuildRtpHeader(
        packet_, kPayload, expect_cvo /* marker_bit */, kTimestamp, 0));
    if (expect_cvo) {
      ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionLength(),
                length);
    } else {
      ASSERT_EQ(kRtpHeaderSize, length);
    }
    ASSERT_TRUE(rtp_parser.Parse(&rtp_header, map));
    ASSERT_FALSE(rtp_parser.RTCP());
    EXPECT_EQ(payload_, rtp_header.payloadType);
    EXPECT_EQ(seq_num, rtp_header.sequenceNumber);
    EXPECT_EQ(kTimestamp, rtp_header.timestamp);
    EXPECT_EQ(rtp_sender_->SSRC(), rtp_header.ssrc);
    EXPECT_EQ(0, rtp_header.numCSRCs);
    EXPECT_EQ(0U, rtp_header.paddingLength);
    EXPECT_EQ(rotation, rtp_header.extension.videoRotation);
  }
};

TEST_F(RtpSenderTestWithoutPacer,
       RegisterRtpTransmissionTimeOffsetHeaderExtension) {
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(kRtpOneByteHeaderLength + kTransmissionTimeOffsetLength,
            rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset));
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionLength());
}

TEST_F(RtpSenderTestWithoutPacer, RegisterRtpAbsoluteSendTimeHeaderExtension) {
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  EXPECT_EQ(RtpUtility::Word32Align(kRtpOneByteHeaderLength +
                                    kAbsoluteSendTimeLength),
            rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
                   kRtpExtensionAbsoluteSendTime));
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionLength());
}

TEST_F(RtpSenderTestWithoutPacer, RegisterRtpAudioLevelHeaderExtension) {
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));
  EXPECT_EQ(
      RtpUtility::Word32Align(kRtpOneByteHeaderLength + kAudioLevelLength),
      rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(0,
            rtp_sender_->DeregisterRtpHeaderExtension(kRtpExtensionAudioLevel));
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionLength());
}

TEST_F(RtpSenderTestWithoutPacer, RegisterRtpHeaderExtensions) {
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(RtpUtility::Word32Align(kRtpOneByteHeaderLength +
                                    kTransmissionTimeOffsetLength),
            rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  EXPECT_EQ(RtpUtility::Word32Align(kRtpOneByteHeaderLength +
                                    kTransmissionTimeOffsetLength +
                                    kAbsoluteSendTimeLength),
            rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));
  EXPECT_EQ(RtpUtility::Word32Align(
                kRtpOneByteHeaderLength + kTransmissionTimeOffsetLength +
                kAbsoluteSendTimeLength + kAudioLevelLength),
            rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));
  EXPECT_TRUE(rtp_sender_->ActivateCVORtpHeaderExtension());
  EXPECT_EQ(RtpUtility::Word32Align(kRtpOneByteHeaderLength +
                                    kTransmissionTimeOffsetLength +
                                    kAbsoluteSendTimeLength +
                                    kAudioLevelLength + kVideoRotationLength),
            rtp_sender_->RtpHeaderExtensionLength());

  // Deregister starts.
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset));
  EXPECT_EQ(RtpUtility::Word32Align(kRtpOneByteHeaderLength +
                                    kAbsoluteSendTimeLength +
                                    kAudioLevelLength + kVideoRotationLength),
            rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
                   kRtpExtensionAbsoluteSendTime));
  EXPECT_EQ(RtpUtility::Word32Align(kRtpOneByteHeaderLength +
                                    kAudioLevelLength + kVideoRotationLength),
            rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(0,
            rtp_sender_->DeregisterRtpHeaderExtension(kRtpExtensionAudioLevel));
  EXPECT_EQ(
      RtpUtility::Word32Align(kRtpOneByteHeaderLength + kVideoRotationLength),
      rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(
      0, rtp_sender_->DeregisterRtpHeaderExtension(kRtpExtensionVideoRotation));
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionLength());
}

TEST_F(RtpSenderTestWithoutPacer, RegisterRtpVideoRotationHeaderExtension) {
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionLength());

  EXPECT_TRUE(rtp_sender_->ActivateCVORtpHeaderExtension());
  EXPECT_EQ(
      RtpUtility::Word32Align(kRtpOneByteHeaderLength + kVideoRotationLength),
      rtp_sender_->RtpHeaderExtensionLength());
  EXPECT_EQ(
      0, rtp_sender_->DeregisterRtpHeaderExtension(kRtpExtensionVideoRotation));
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionLength());
}

TEST_F(RtpSenderTestWithoutPacer, AllocatePacketSetCsrc) {
  // Configure rtp_sender with csrc.
  std::vector<uint32_t> csrcs;
  csrcs.push_back(0x23456789);
  rtp_sender_->SetCsrcs(csrcs);

  auto packet = rtp_sender_->AllocatePacket();

  ASSERT_TRUE(packet);
  EXPECT_EQ(rtp_sender_->SSRC(), packet->Ssrc());
  EXPECT_EQ(csrcs, packet->Csrcs());
}

TEST_F(RtpSenderTestWithoutPacer, AllocatePacketReserveExtensions) {
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

TEST_F(RtpSenderTestWithoutPacer, AssignSequenceNumberAdvanceSequenceNumber) {
  auto packet = rtp_sender_->AllocatePacket();
  ASSERT_TRUE(packet);
  const uint16_t sequence_number = rtp_sender_->SequenceNumber();

  EXPECT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));

  EXPECT_EQ(sequence_number, packet->SequenceNumber());
  EXPECT_EQ(sequence_number + 1, rtp_sender_->SequenceNumber());
}

TEST_F(RtpSenderTestWithoutPacer, AssignSequenceNumberFailsOnNotSending) {
  auto packet = rtp_sender_->AllocatePacket();
  ASSERT_TRUE(packet);

  rtp_sender_->SetSendingMediaStatus(false);
  EXPECT_FALSE(rtp_sender_->AssignSequenceNumber(packet.get()));
}

TEST_F(RtpSenderTestWithoutPacer, AssignSequenceNumberMayAllowPadding) {
  constexpr size_t kPaddingSize = 100;
  auto packet = rtp_sender_->AllocatePacket();
  ASSERT_TRUE(packet);

  ASSERT_FALSE(rtp_sender_->SendPadData(kPaddingSize, false, 0, 0, -1));
  packet->SetMarker(false);
  ASSERT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
  // Packet without marker bit doesn't allow padding.
  EXPECT_FALSE(rtp_sender_->SendPadData(kPaddingSize, false, 0, 0, -1));

  packet->SetMarker(true);
  ASSERT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
  // Packet with marker bit allows send padding.
  EXPECT_TRUE(rtp_sender_->SendPadData(kPaddingSize, false, 0, 0, -1));
}

TEST_F(RtpSenderTestWithoutPacer, AssignSequenceNumberSetPaddingTimestamps) {
  constexpr size_t kPaddingSize = 100;
  auto packet = rtp_sender_->AllocatePacket();
  ASSERT_TRUE(packet);
  packet->SetMarker(true);
  packet->SetTimestamp(kTimestamp);

  ASSERT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
  ASSERT_TRUE(rtp_sender_->SendPadData(kPaddingSize, false, 0, 0, -1));

  ASSERT_EQ(1u, transport_.sent_packets_.size());
  // Parse the padding packet and verify its timestamp.
  RtpPacketToSend padding_packet(nullptr);
  ASSERT_TRUE(padding_packet.Parse(transport_.sent_packets_[0]->data(),
                                   transport_.sent_packets_[0]->size()));
  EXPECT_EQ(kTimestamp, padding_packet.Timestamp());
}

TEST_F(RtpSenderTestWithoutPacer, BuildRTPPacket) {
  size_t length = static_cast<size_t>(rtp_sender_->BuildRtpHeader(
      packet_, kPayload, kMarkerBit, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize, length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, nullptr);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_FALSE(rtp_header.extension.hasTransmissionTimeOffset);
  EXPECT_FALSE(rtp_header.extension.hasAbsoluteSendTime);
  EXPECT_FALSE(rtp_header.extension.hasAudioLevel);
  EXPECT_EQ(0, rtp_header.extension.transmissionTimeOffset);
  EXPECT_EQ(0u, rtp_header.extension.absoluteSendTime);
  EXPECT_FALSE(rtp_header.extension.voiceActivity);
  EXPECT_EQ(0u, rtp_header.extension.audioLevel);
  EXPECT_EQ(0u, rtp_header.extension.videoRotation);
}

TEST_F(RtpSenderTestWithoutPacer,
       BuildRTPPacketWithTransmissionOffsetExtension) {
  EXPECT_EQ(0, rtp_sender_->SetTransmissionTimeOffset(kTimeOffset));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));

  size_t length = static_cast<size_t>(rtp_sender_->BuildRtpHeader(
      packet_, kPayload, kMarkerBit, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionLength(), length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(kTimeOffset, rtp_header.extension.transmissionTimeOffset);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(&rtp_header2, nullptr);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_FALSE(rtp_header2.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(0, rtp_header2.extension.transmissionTimeOffset);
}

TEST_F(RtpSenderTestWithoutPacer,
       BuildRTPPacketWithNegativeTransmissionOffsetExtension) {
  const int kNegTimeOffset = -500;
  EXPECT_EQ(0, rtp_sender_->SetTransmissionTimeOffset(kNegTimeOffset));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));

  size_t length = static_cast<size_t>(rtp_sender_->BuildRtpHeader(
      packet_, kPayload, kMarkerBit, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionLength(), length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(kNegTimeOffset, rtp_header.extension.transmissionTimeOffset);
}

TEST_F(RtpSenderTestWithoutPacer, BuildRTPPacketWithAbsoluteSendTimeExtension) {
  EXPECT_EQ(0, rtp_sender_->SetAbsoluteSendTime(kAbsoluteSendTime));
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));

  size_t length = static_cast<size_t>(rtp_sender_->BuildRtpHeader(
      packet_, kPayload, kMarkerBit, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionLength(), length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasAbsoluteSendTime);
  EXPECT_EQ(kAbsoluteSendTime, rtp_header.extension.absoluteSendTime);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(&rtp_header2, nullptr);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_FALSE(rtp_header2.extension.hasAbsoluteSendTime);
  EXPECT_EQ(0u, rtp_header2.extension.absoluteSendTime);
}

TEST_F(RtpSenderTestWithoutPacer, SendsPacketsWithTransportSequenceNumber) {
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, nullptr,
      &seq_num_allocator_, &feedback_observer_, nullptr, nullptr, nullptr,
      &mock_rtc_event_log_, &send_packet_observer_,
      &retransmission_rate_limiter_));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);
  EXPECT_CALL(feedback_observer_,
              AddPacket(kTransportSequenceNumber,
                        sizeof(kPayloadData) + kGenericHeaderLength,
                        PacketInfo::kNotAProbe))
      .Times(1);

  SendGenericPayload();

  RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                         transport_.last_sent_packet_len_);
  webrtc::RTPHeader rtp_header;
  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransportSequenceNumber,
               kTransportSequenceNumberExtensionId);
  EXPECT_TRUE(rtp_parser.Parse(&rtp_header, &map));
  EXPECT_TRUE(rtp_header.extension.hasTransportSequenceNumber);
  EXPECT_EQ(kTransportSequenceNumber,
            rtp_header.extension.transportSequenceNumber);
  EXPECT_EQ(transport_.last_packet_id_,
            rtp_header.extension.transportSequenceNumber);
}

TEST_F(RtpSenderTestWithoutPacer, NoAllocationIfNotRegistered) {
  SendGenericPayload();
}

TEST_F(RtpSenderTestWithoutPacer, OnSendPacketUpdated) {
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);

  SendGenericPayload();
}

// Test CVO header extension is only set when marker bit is true.
TEST_F(RtpSenderTestWithoutPacer, BuildRTPPacketWithVideoRotation_MarkerBit) {
  rtp_sender_->SetVideoRotation(kRotation);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));
  EXPECT_TRUE(rtp_sender_->ActivateCVORtpHeaderExtension());

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionVideoRotation, kVideoRotationExtensionId);

  size_t length = static_cast<size_t>(
      rtp_sender_->BuildRtpHeader(packet_, kPayload, true, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionLength(), length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  ASSERT_TRUE(rtp_parser.Parse(&rtp_header, &map));
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasVideoRotation);
  EXPECT_EQ(kRotation, rtp_header.extension.videoRotation);
}

// Test CVO header extension is not set when marker bit is false.
TEST_F(RtpSenderTestWithoutPacer,
       DISABLED_BuildRTPPacketWithVideoRotation_NoMarkerBit) {
  rtp_sender_->SetVideoRotation(kRotation);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));
  EXPECT_TRUE(rtp_sender_->ActivateCVORtpHeaderExtension());

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionVideoRotation, kVideoRotationExtensionId);

  size_t length = static_cast<size_t>(
      rtp_sender_->BuildRtpHeader(packet_, kPayload, false, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize, length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  ASSERT_TRUE(rtp_parser.Parse(&rtp_header, &map));
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header, false);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_FALSE(rtp_header.extension.hasVideoRotation);
}

TEST_F(RtpSenderTestWithoutPacer, BuildRTPPacketWithAudioLevelExtension) {
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));

  size_t length = static_cast<size_t>(rtp_sender_->BuildRtpHeader(
      packet_, kPayload, kMarkerBit, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionLength(), length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  // Updating audio level is done in RTPSenderAudio, so simulate it here.
  rtp_parser.Parse(&rtp_header);
  rtp_sender_->UpdateAudioLevel(packet_, length, rtp_header, true, kAudioLevel);

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasAudioLevel);
  EXPECT_TRUE(rtp_header.extension.voiceActivity);
  EXPECT_EQ(kAudioLevel, rtp_header.extension.audioLevel);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(&rtp_header2, nullptr);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_FALSE(rtp_header2.extension.hasAudioLevel);
  EXPECT_FALSE(rtp_header2.extension.voiceActivity);
  EXPECT_EQ(0u, rtp_header2.extension.audioLevel);
}

TEST_F(RtpSenderTestWithoutPacer,
       BuildRTPPacketWithCSRCAndAudioLevelExtension) {
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));
  std::vector<uint32_t> csrcs;
  csrcs.push_back(0x23456789);
  rtp_sender_->SetCsrcs(csrcs);
  size_t length = static_cast<size_t>(rtp_sender_->BuildRtpHeader(
      packet_, kPayload, kMarkerBit, kTimestamp, 0));

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  // Updating audio level is done in RTPSenderAudio, so simulate it here.
  rtp_parser.Parse(&rtp_header);
  EXPECT_TRUE(rtp_sender_->UpdateAudioLevel(packet_, length, rtp_header, true,
                                            kAudioLevel));

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header, kMarkerBit, csrcs.size());
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasAudioLevel);
  EXPECT_TRUE(rtp_header.extension.voiceActivity);
  EXPECT_EQ(kAudioLevel, rtp_header.extension.audioLevel);
  EXPECT_EQ(1u, rtp_header.numCSRCs);
  EXPECT_EQ(csrcs[0], rtp_header.arrOfCSRCs[0]);
}

TEST_F(RtpSenderTestWithoutPacer, BuildRTPPacketWithHeaderExtensions) {
  EXPECT_EQ(0, rtp_sender_->SetTransmissionTimeOffset(kTimeOffset));
  EXPECT_EQ(0, rtp_sender_->SetAbsoluteSendTime(kAbsoluteSendTime));
  EXPECT_EQ(0,
            rtp_sender_->SetTransportSequenceNumber(kTransportSequenceNumber));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  size_t length = static_cast<size_t>(rtp_sender_->BuildRtpHeader(
      packet_, kPayload, kMarkerBit, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionLength(), length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  // Updating audio level is done in RTPSenderAudio, so simulate it here.
  rtp_parser.Parse(&rtp_header);
  rtp_sender_->UpdateAudioLevel(packet_, length, rtp_header, true, kAudioLevel);

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  map.Register(kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId);
  map.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  map.Register(kRtpExtensionTransportSequenceNumber,
               kTransportSequenceNumberExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasTransmissionTimeOffset);
  EXPECT_TRUE(rtp_header.extension.hasAbsoluteSendTime);
  EXPECT_TRUE(rtp_header.extension.hasAudioLevel);
  EXPECT_TRUE(rtp_header.extension.hasTransportSequenceNumber);
  EXPECT_EQ(kTimeOffset, rtp_header.extension.transmissionTimeOffset);
  EXPECT_EQ(kAbsoluteSendTime, rtp_header.extension.absoluteSendTime);
  EXPECT_TRUE(rtp_header.extension.voiceActivity);
  EXPECT_EQ(kAudioLevel, rtp_header.extension.audioLevel);
  EXPECT_EQ(kTransportSequenceNumber,
            rtp_header.extension.transportSequenceNumber);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(&rtp_header2, nullptr);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_FALSE(rtp_header2.extension.hasTransmissionTimeOffset);
  EXPECT_FALSE(rtp_header2.extension.hasAbsoluteSendTime);
  EXPECT_FALSE(rtp_header2.extension.hasAudioLevel);
  EXPECT_FALSE(rtp_header2.extension.hasTransportSequenceNumber);

  EXPECT_EQ(0, rtp_header2.extension.transmissionTimeOffset);
  EXPECT_EQ(0u, rtp_header2.extension.absoluteSendTime);
  EXPECT_FALSE(rtp_header2.extension.voiceActivity);
  EXPECT_EQ(0u, rtp_header2.extension.audioLevel);
  EXPECT_EQ(0u, rtp_header2.extension.transportSequenceNumber);
}

TEST_F(RtpSenderTest, SendsPacketsWithTransportSequenceNumber) {
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, &mock_paced_sender_,
      &seq_num_allocator_, &feedback_observer_, nullptr, nullptr, nullptr,
      &mock_rtc_event_log_, &send_packet_observer_,
      &retransmission_rate_limiter_));
  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  uint16_t seq_num = 0;
  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, _, _, _, _, _))
      .Times(1).WillRepeatedly(testing::SaveArg<2>(&seq_num));
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);
  const int kProbeClusterId = 1;
  EXPECT_CALL(
      feedback_observer_,
      AddPacket(kTransportSequenceNumber,
                sizeof(kPayloadData) + kGenericHeaderLength, kProbeClusterId))
      .Times(1);

  SendGenericPayload();
  rtp_sender_->TimeToSendPacket(seq_num, fake_clock_.TimeInMilliseconds(),
                                false, kProbeClusterId);

  RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                         transport_.last_sent_packet_len_);
  webrtc::RTPHeader rtp_header;
  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransportSequenceNumber,
               kTransportSequenceNumberExtensionId);
  EXPECT_TRUE(rtp_parser.Parse(&rtp_header, &map));
  EXPECT_TRUE(rtp_header.extension.hasTransportSequenceNumber);
  EXPECT_EQ(kTransportSequenceNumber,
            rtp_header.extension.transportSequenceNumber);
  EXPECT_EQ(transport_.last_packet_id_,
            rtp_header.extension.transportSequenceNumber);
}

TEST_F(RtpSenderTest, TrafficSmoothingWithExtensions) {
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               _, kSeqNum, _, _, _));
  EXPECT_CALL(mock_rtc_event_log_,
              LogRtpHeader(PacketDirection::kOutgoingPacket, _, _, _));

  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  int rtp_length_int = rtp_sender_->BuildRtpHeader(
      packet_, kPayload, kMarkerBit, kTimestamp, capture_time_ms);
  ASSERT_NE(-1, rtp_length_int);
  size_t rtp_length = static_cast<size_t>(rtp_length_int);

  // Packet should be stored in a send bucket.
  EXPECT_TRUE(rtp_sender_->SendToNetwork(packet_, 0, rtp_length,
                                         capture_time_ms, kAllowRetransmission,
                                         RtpPacketSender::kNormalPriority));

  EXPECT_EQ(0, transport_.packets_sent_);

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);

  rtp_sender_->TimeToSendPacket(kSeqNum, capture_time_ms, false,
                                PacketInfo::kNotAProbe);

  // Process send bucket. Packet should now be sent.
  EXPECT_EQ(1, transport_.packets_sent_);
  EXPECT_EQ(rtp_length, transport_.last_sent_packet_len_);
  // Parse sent packet.
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                                 rtp_length);
  webrtc::RTPHeader rtp_header;
  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  map.Register(kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);
  ASSERT_TRUE(valid_rtp_header);

  // Verify transmission time offset.
  EXPECT_EQ(kStoredTimeInMs * 90, rtp_header.extension.transmissionTimeOffset);
  uint64_t expected_send_time =
      ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
}

TEST_F(RtpSenderTest, TrafficSmoothingRetransmits) {
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               _, kSeqNum, _, _, _));
  EXPECT_CALL(mock_rtc_event_log_,
              LogRtpHeader(PacketDirection::kOutgoingPacket, _, _, _));

  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  int rtp_length_int = rtp_sender_->BuildRtpHeader(
      packet_, kPayload, kMarkerBit, kTimestamp, capture_time_ms);
  ASSERT_NE(-1, rtp_length_int);
  size_t rtp_length = static_cast<size_t>(rtp_length_int);

  // Packet should be stored in a send bucket.
  EXPECT_TRUE(rtp_sender_->SendToNetwork(packet_, 0, rtp_length,
                                         capture_time_ms, kAllowRetransmission,
                                         RtpPacketSender::kNormalPriority));

  EXPECT_EQ(0, transport_.packets_sent_);

  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               _, kSeqNum, _, _, _));

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);

  EXPECT_EQ(rtp_length_int, rtp_sender_->ReSendPacket(kSeqNum));
  EXPECT_EQ(0, transport_.packets_sent_);

  rtp_sender_->TimeToSendPacket(kSeqNum, capture_time_ms, false,
                                PacketInfo::kNotAProbe);

  // Process send bucket. Packet should now be sent.
  EXPECT_EQ(1, transport_.packets_sent_);
  EXPECT_EQ(rtp_length, transport_.last_sent_packet_len_);

  // Parse sent packet.
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                                 rtp_length);
  webrtc::RTPHeader rtp_header;
  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  map.Register(kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);
  ASSERT_TRUE(valid_rtp_header);

  // Verify transmission time offset.
  EXPECT_EQ(kStoredTimeInMs * 90, rtp_header.extension.transmissionTimeOffset);
  uint64_t expected_send_time =
      ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
}

// This test sends 1 regular video packet, then 4 padding packets, and then
// 1 more regular packet.
TEST_F(RtpSenderTest, SendPadding) {
  // Make all (non-padding) packets go to send queue.
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               _, kSeqNum, _, _, _));
  EXPECT_CALL(mock_rtc_event_log_,
              LogRtpHeader(PacketDirection::kOutgoingPacket, _, _, _))
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

  // Create and set up parser.
  std::unique_ptr<webrtc::RtpHeaderParser> rtp_parser(
      webrtc::RtpHeaderParser::Create());
  ASSERT_TRUE(rtp_parser.get() != nullptr);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                         kTransmissionTimeOffsetExtensionId);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                         kAbsoluteSendTimeExtensionId);
  webrtc::RTPHeader rtp_header;

  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  int rtp_length_int = rtp_sender_->BuildRtpHeader(
      packet_, kPayload, kMarkerBit, timestamp, capture_time_ms);
  const uint32_t media_packet_timestamp = timestamp;
  ASSERT_NE(-1, rtp_length_int);
  size_t rtp_length = static_cast<size_t>(rtp_length_int);

  // Packet should be stored in a send bucket.
  EXPECT_TRUE(rtp_sender_->SendToNetwork(packet_, 0, rtp_length,
                                         capture_time_ms, kAllowRetransmission,
                                         RtpPacketSender::kNormalPriority));

  int total_packets_sent = 0;
  EXPECT_EQ(total_packets_sent, transport_.packets_sent_);

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);
  rtp_sender_->TimeToSendPacket(seq_num++, capture_time_ms, false,
                                PacketInfo::kNotAProbe);
  // Packet should now be sent. This test doesn't verify the regular video
  // packet, since it is tested in another test.
  EXPECT_EQ(++total_packets_sent, transport_.packets_sent_);
  timestamp += 90 * kStoredTimeInMs;

  // Send padding 4 times, waiting 50 ms between each.
  for (int i = 0; i < 4; ++i) {
    const int kPaddingPeriodMs = 50;
    const size_t kPaddingBytes = 100;
    const size_t kMaxPaddingLength = 224;  // Value taken from rtp_sender.cc.
    // Padding will be forced to full packets.
    EXPECT_EQ(kMaxPaddingLength, rtp_sender_->TimeToSendPadding(
                                     kPaddingBytes, PacketInfo::kNotAProbe));

    // Process send bucket. Padding should now be sent.
    EXPECT_EQ(++total_packets_sent, transport_.packets_sent_);
    EXPECT_EQ(kMaxPaddingLength + rtp_header_len,
              transport_.last_sent_packet_len_);
    // Parse sent packet.
    ASSERT_TRUE(rtp_parser->Parse(transport_.last_sent_packet_,
                                  transport_.last_sent_packet_len_,
                                  &rtp_header));
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
  rtp_length_int = rtp_sender_->BuildRtpHeader(packet_, kPayload, kMarkerBit,
                                               timestamp, capture_time_ms);
  ASSERT_NE(-1, rtp_length_int);
  rtp_length = static_cast<size_t>(rtp_length_int);

  EXPECT_CALL(mock_paced_sender_,
              InsertPacket(RtpPacketSender::kNormalPriority, _, _, _, _, _));

  // Packet should be stored in a send bucket.
  EXPECT_TRUE(rtp_sender_->SendToNetwork(packet_, 0, rtp_length,
                                         capture_time_ms, kAllowRetransmission,
                                         RtpPacketSender::kNormalPriority));

  rtp_sender_->TimeToSendPacket(seq_num, capture_time_ms, false,
                                PacketInfo::kNotAProbe);
  // Process send bucket.
  EXPECT_EQ(++total_packets_sent, transport_.packets_sent_);
  EXPECT_EQ(rtp_length, transport_.last_sent_packet_len_);
  // Parse sent packet.
  ASSERT_TRUE(
      rtp_parser->Parse(transport_.last_sent_packet_, rtp_length, &rtp_header));

  // Verify sequence number and timestamp.
  EXPECT_EQ(seq_num, rtp_header.sequenceNumber);
  EXPECT_EQ(timestamp, rtp_header.timestamp);
  // Verify transmission time offset. This packet is sent without delay.
  EXPECT_EQ(0, rtp_header.extension.transmissionTimeOffset);
  uint64_t expected_send_time =
      ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
}

TEST_F(RtpSenderTest, OnSendPacketUpdated) {
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  rtp_sender_->SetStorePacketsStatus(true, 10);

  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, _, _, _, _, _)).Times(1);

  SendGenericPayload();  // Packet passed to pacer.
  const bool kIsRetransmit = false;
  rtp_sender_->TimeToSendPacket(kSeqNum, fake_clock_.TimeInMilliseconds(),
                                kIsRetransmit, PacketInfo::kNotAProbe);
  EXPECT_EQ(1, transport_.packets_sent_);
}

TEST_F(RtpSenderTest, OnSendPacketNotUpdatedForRetransmits) {
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  rtp_sender_->SetStorePacketsStatus(true, 10);

  EXPECT_CALL(send_packet_observer_, OnSendPacket(_, _, _)).Times(0);
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, _, _, _, _, _)).Times(1);

  SendGenericPayload();  // Packet passed to pacer.
  const bool kIsRetransmit = true;
  rtp_sender_->TimeToSendPacket(kSeqNum, fake_clock_.TimeInMilliseconds(),
                                kIsRetransmit, PacketInfo::kNotAProbe);
  EXPECT_EQ(1, transport_.packets_sent_);
}

TEST_F(RtpSenderTest, OnSendPacketNotUpdatedWithoutSeqNumAllocator) {
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, &mock_paced_sender_,
      nullptr /* TransportSequenceNumberAllocator */, nullptr, nullptr, nullptr,
      nullptr, nullptr, &send_packet_observer_, &retransmission_rate_limiter_));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetStorePacketsStatus(true, 10);

  EXPECT_CALL(send_packet_observer_, OnSendPacket(_, _, _)).Times(0);
  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, _, _, _, _, _)).Times(1);

  SendGenericPayload();  // Packet passed to pacer.
  const bool kIsRetransmit = false;
  rtp_sender_->TimeToSendPacket(kSeqNum, fake_clock_.TimeInMilliseconds(),
                                kIsRetransmit, PacketInfo::kNotAProbe);
  EXPECT_EQ(1, transport_.packets_sent_);
}

TEST_F(RtpSenderTest, SendRedundantPayloads) {
  MockTransport transport;
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport, &mock_paced_sender_, nullptr, nullptr,
      nullptr, nullptr, nullptr, &mock_rtc_event_log_, nullptr,
      &retransmission_rate_limiter_));

  rtp_sender_->SetSequenceNumber(kSeqNum);
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

  // Create and set up parser.
  std::unique_ptr<webrtc::RtpHeaderParser> rtp_parser(
      webrtc::RtpHeaderParser::Create());
  ASSERT_TRUE(rtp_parser.get() != nullptr);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                         kTransmissionTimeOffsetExtensionId);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                         kAbsoluteSendTimeExtensionId);
  const size_t kNumPayloadSizes = 10;
  const size_t kPayloadSizes[kNumPayloadSizes] = {500, 550, 600, 650, 700,
                                                  750, 800, 850, 900, 950};
  // Expect all packets go through the pacer.
  EXPECT_CALL(mock_paced_sender_,
              InsertPacket(RtpPacketSender::kNormalPriority, _, _, _, _, _))
      .Times(kNumPayloadSizes);
  EXPECT_CALL(mock_rtc_event_log_,
              LogRtpHeader(PacketDirection::kOutgoingPacket, _, _, _))
      .Times(kNumPayloadSizes);

  // Send 10 packets of increasing size.
  for (size_t i = 0; i < kNumPayloadSizes; ++i) {
    int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
    EXPECT_CALL(transport, SendRtp(_, _, _)).WillOnce(testing::Return(true));
    SendPacket(capture_time_ms, kPayloadSizes[i]);
    rtp_sender_->TimeToSendPacket(seq_num++, capture_time_ms, false,
                                  PacketInfo::kNotAProbe);
    fake_clock_.AdvanceTimeMilliseconds(33);
  }

  EXPECT_CALL(mock_rtc_event_log_,
              LogRtpHeader(PacketDirection::kOutgoingPacket, _, _, _))
      .Times(::testing::AtLeast(4));

  // The amount of padding to send it too small to send a payload packet.
  EXPECT_CALL(transport, SendRtp(_, kMaxPaddingSize + rtp_header_len, _))
      .WillOnce(testing::Return(true));
  EXPECT_EQ(kMaxPaddingSize,
            rtp_sender_->TimeToSendPadding(49, PacketInfo::kNotAProbe));

  EXPECT_CALL(transport,
              SendRtp(_, kPayloadSizes[0] + rtp_header_len + kRtxHeaderSize, _))
      .WillOnce(testing::Return(true));
  EXPECT_EQ(kPayloadSizes[0],
            rtp_sender_->TimeToSendPadding(500, PacketInfo::kNotAProbe));

  EXPECT_CALL(transport, SendRtp(_, kPayloadSizes[kNumPayloadSizes - 1] +
                                        rtp_header_len + kRtxHeaderSize,
                                 _))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(transport, SendRtp(_, kMaxPaddingSize + rtp_header_len, _))
      .WillOnce(testing::Return(true));
  EXPECT_EQ(kPayloadSizes[kNumPayloadSizes - 1] + kMaxPaddingSize,
            rtp_sender_->TimeToSendPadding(999, PacketInfo::kNotAProbe));
}

TEST_F(RtpSenderTestWithoutPacer, SendGenericVideo) {
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  // Send keyframe
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(kVideoFrameKey, payload_type, 1234,
                                            4321, payload, sizeof(payload),
                                            nullptr, nullptr, nullptr));

  RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                         transport_.last_sent_packet_len_);
  webrtc::RTPHeader rtp_header;
  ASSERT_TRUE(rtp_parser.Parse(&rtp_header));

  const uint8_t* payload_data =
      GetPayloadData(rtp_header, transport_.last_sent_packet_);
  uint8_t generic_header = *payload_data++;

  ASSERT_EQ(sizeof(payload) + sizeof(generic_header),
            GetPayloadDataLength(rtp_header, transport_.last_sent_packet_len_));

  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kKeyFrameBit);
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kFirstPacketBit);

  EXPECT_EQ(0, memcmp(payload, payload_data, sizeof(payload)));

  // Send delta frame
  payload[0] = 13;
  payload[1] = 42;
  payload[4] = 13;

  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameDelta, payload_type, 1234, 4321, payload, sizeof(payload),
      nullptr, nullptr, nullptr));

  RtpUtility::RtpHeaderParser rtp_parser2(transport_.last_sent_packet_,
                                          transport_.last_sent_packet_len_);
  ASSERT_TRUE(rtp_parser.Parse(&rtp_header));

  payload_data = GetPayloadData(rtp_header, transport_.last_sent_packet_);
  generic_header = *payload_data++;

  EXPECT_FALSE(generic_header & RtpFormatVideoGeneric::kKeyFrameBit);
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kFirstPacketBit);

  ASSERT_EQ(sizeof(payload) + sizeof(generic_header),
            GetPayloadDataLength(rtp_header, transport_.last_sent_packet_len_));

  EXPECT_EQ(0, memcmp(payload, payload_data, sizeof(payload)));
}

TEST_F(RtpSenderTest, FrameCountCallbacks) {
  class TestCallback : public FrameCountObserver {
   public:
    TestCallback() : FrameCountObserver(), num_calls_(0), ssrc_(0) {}
    virtual ~TestCallback() {}

    void FrameCountUpdated(const FrameCounts& frame_counts,
                           uint32_t ssrc) override {
      ++num_calls_;
      ssrc_ = ssrc;
      frame_counts_ = frame_counts;
    }

    uint32_t num_calls_;
    uint32_t ssrc_;
    FrameCounts frame_counts_;
  } callback;

  rtp_sender_.reset(new RTPSender(false, &fake_clock_, &transport_,
                                  &mock_paced_sender_, nullptr, nullptr,
                                  nullptr, &callback, nullptr, nullptr, nullptr,
                                  &retransmission_rate_limiter_));

  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, _, _, _, _, _))
      .Times(::testing::AtLeast(2));

  ASSERT_TRUE(rtp_sender_->SendOutgoingData(kVideoFrameKey, payload_type, 1234,
                                            4321, payload, sizeof(payload),
                                            nullptr, nullptr, nullptr));

  EXPECT_EQ(1U, callback.num_calls_);
  EXPECT_EQ(ssrc, callback.ssrc_);
  EXPECT_EQ(1, callback.frame_counts_.key_frames);
  EXPECT_EQ(0, callback.frame_counts_.delta_frames);

  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameDelta, payload_type, 1234, 4321, payload, sizeof(payload),
      nullptr, nullptr, nullptr));

  EXPECT_EQ(2U, callback.num_calls_);
  EXPECT_EQ(ssrc, callback.ssrc_);
  EXPECT_EQ(1, callback.frame_counts_.key_frames);
  EXPECT_EQ(1, callback.frame_counts_.delta_frames);

  rtp_sender_.reset();
}

TEST_F(RtpSenderTest, BitrateCallbacks) {
  class TestCallback : public BitrateStatisticsObserver {
   public:
    TestCallback()
        : BitrateStatisticsObserver(),
          num_calls_(0),
          ssrc_(0),
          total_bitrate_(0),
          retransmit_bitrate_(0) {}
    virtual ~TestCallback() {}

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
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, nullptr, nullptr, nullptr, &callback,
      nullptr, nullptr, nullptr, nullptr, &retransmission_rate_limiter_));

  // Simulate kNumPackets sent with kPacketInterval ms intervals, with the
  // number of packets selected so that we fill (but don't overflow) the one
  // second averaging window.
  const uint32_t kWindowSizeMs = 1000;
  const uint32_t kPacketInterval = 20;
  const uint32_t kNumPackets =
      (kWindowSizeMs - kPacketInterval) / kPacketInterval;
  // Overhead = 12 bytes RTP header + 1 byte generic header.
  const uint32_t kPacketOverhead = 13;

  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  // Initial process call so we get a new time window.
  rtp_sender_->ProcessBitrate();

  // Send a few frames.
  for (uint32_t i = 0; i < kNumPackets; ++i) {
    ASSERT_TRUE(rtp_sender_->SendOutgoingData(
        kVideoFrameKey, payload_type, 1234, 4321, payload, sizeof(payload),
        nullptr, nullptr, nullptr));
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

class RtpSenderAudioTest : public RtpSenderTest {
 protected:
  RtpSenderAudioTest() {}

  void SetUp() override {
    payload_ = kAudioPayload;
    rtp_sender_.reset(new RTPSender(
        true, &fake_clock_, &transport_, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, &retransmission_rate_limiter_));
    rtp_sender_->SetSequenceNumber(kSeqNum);
  }
};

TEST_F(RtpSenderTestWithoutPacer, StreamDataCountersCallbacks) {
  class TestCallback : public StreamDataCountersCallback {
   public:
    TestCallback() : StreamDataCountersCallback(), ssrc_(0), counters_() {}
    virtual ~TestCallback() {}

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
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  rtp_sender_->RegisterRtpStatisticsCallback(&callback);

  // Send a frame.
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
                      kVideoFrameKey, payload_type, 1234, 4321, payload,
                      sizeof(payload), nullptr, nullptr, nullptr));
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
  rtp_sender_->ReSendPacket(seqno, 0);
  expected.transmitted.payload_bytes = 12;
  expected.transmitted.header_bytes = 24;
  expected.transmitted.packets = 2;
  expected.retransmitted.payload_bytes = 6;
  expected.retransmitted.header_bytes = 12;
  expected.retransmitted.padding_bytes = 0;
  expected.retransmitted.packets = 1;
  callback.Matches(ssrc, expected);

  // Send padding.
  rtp_sender_->TimeToSendPadding(kMaxPaddingSize, PacketInfo::kNotAProbe);
  expected.transmitted.payload_bytes = 12;
  expected.transmitted.header_bytes = 36;
  expected.transmitted.padding_bytes = kMaxPaddingSize;
  expected.transmitted.packets = 3;
  callback.Matches(ssrc, expected);

  // Send FEC.
  rtp_sender_->SetGenericFECStatus(true, kRedPayloadType, kUlpfecPayloadType);
  FecProtectionParams fec_params;
  fec_params.fec_mask_type = kFecMaskRandom;
  fec_params.fec_rate = 1;
  fec_params.max_fec_frames = 1;
  rtp_sender_->SetFecParameters(&fec_params, &fec_params);
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
                      kVideoFrameDelta, payload_type, 1234, 4321, payload,
                      sizeof(payload), nullptr, nullptr, nullptr));
  expected.transmitted.payload_bytes = 40;
  expected.transmitted.header_bytes = 60;
  expected.transmitted.packets = 5;
  expected.fec.packets = 1;
  callback.Matches(ssrc, expected);

  rtp_sender_->RegisterRtpStatisticsCallback(nullptr);
}

TEST_F(RtpSenderAudioTest, SendAudio) {
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "PAYLOAD_NAME";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 48000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
                      kAudioFrameCN, payload_type, 1234, 4321, payload,
                      sizeof(payload), nullptr, nullptr, nullptr));

  RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                         transport_.last_sent_packet_len_);
  webrtc::RTPHeader rtp_header;
  ASSERT_TRUE(rtp_parser.Parse(&rtp_header));

  const uint8_t* payload_data =
      GetPayloadData(rtp_header, transport_.last_sent_packet_);

  ASSERT_EQ(sizeof(payload),
            GetPayloadDataLength(rtp_header, transport_.last_sent_packet_len_));

  EXPECT_EQ(0, memcmp(payload, payload_data, sizeof(payload)));
}

TEST_F(RtpSenderAudioTest, SendAudioWithAudioLevelExtension) {
  EXPECT_EQ(0, rtp_sender_->SetAudioLevel(kAudioLevel));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));

  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "PAYLOAD_NAME";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 48000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
                      kAudioFrameCN, payload_type, 1234, 4321, payload,
                      sizeof(payload), nullptr, nullptr, nullptr));

  RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                         transport_.last_sent_packet_len_);
  webrtc::RTPHeader rtp_header;
  ASSERT_TRUE(rtp_parser.Parse(&rtp_header));

  const uint8_t* payload_data =
      GetPayloadData(rtp_header, transport_.last_sent_packet_);

  ASSERT_EQ(sizeof(payload),
            GetPayloadDataLength(rtp_header, transport_.last_sent_packet_len_));

  EXPECT_EQ(0, memcmp(payload, payload_data, sizeof(payload)));

  uint8_t extension[] = {
      0xbe, 0xde, 0x00, 0x01,
      (kAudioLevelExtensionId << 4) + 0,  // ID + length.
      kAudioLevel,                        // Data.
      0x00, 0x00                          // Padding.
  };

  EXPECT_EQ(0, memcmp(extension, payload_data - sizeof(extension),
                      sizeof(extension)));
}

// As RFC4733, named telephone events are carried as part of the audio stream
// and must use the same sequence number and timestamp base as the regular
// audio channel.
// This test checks the marker bit for the first packet and the consequent
// packets of the same telephone event. Since it is specifically for DTMF
// events, ignoring audio packets and sending kEmptyFrame instead of those.
TEST_F(RtpSenderAudioTest, CheckMarkerBitForTelephoneEvents) {
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "telephone-event";
  uint8_t payload_type = 126;
  ASSERT_EQ(0,
            rtp_sender_->RegisterPayload(payload_name, payload_type, 0, 0, 0));
  // For Telephone events, payload is not added to the registered payload list,
  // it will register only the payload used for audio stream.
  // Registering the payload again for audio stream with different payload name.
  const char kPayloadName[] = "payload_name";
  ASSERT_EQ(
      0, rtp_sender_->RegisterPayload(kPayloadName, payload_type, 8000, 1, 0));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  // DTMF event key=9, duration=500 and attenuationdB=10
  rtp_sender_->SendTelephoneEvent(9, 500, 10);
  // During start, it takes the starting timestamp as last sent timestamp.
  // The duration is calculated as the difference of current and last sent
  // timestamp. So for first call it will skip since the duration is zero.
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(kEmptyFrame, payload_type,
                                                capture_time_ms, 0, nullptr, 0,
                                                nullptr, nullptr, nullptr));
  // DTMF Sample Length is (Frequency/1000) * Duration.
  // So in this case, it is (8000/1000) * 500 = 4000.
  // Sending it as two packets.
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
                      kEmptyFrame, payload_type, capture_time_ms + 2000, 0,
                      nullptr, 0, nullptr, nullptr, nullptr));
  std::unique_ptr<webrtc::RtpHeaderParser> rtp_parser(
      webrtc::RtpHeaderParser::Create());
  ASSERT_TRUE(rtp_parser.get() != nullptr);
  webrtc::RTPHeader rtp_header;
  ASSERT_TRUE(rtp_parser->Parse(transport_.last_sent_packet_,
                                transport_.last_sent_packet_len_, &rtp_header));
  // Marker Bit should be set to 1 for first packet.
  EXPECT_TRUE(rtp_header.markerBit);

  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
                      kEmptyFrame, payload_type, capture_time_ms + 4000, 0,
                      nullptr, 0, nullptr, nullptr, nullptr));
  ASSERT_TRUE(rtp_parser->Parse(transport_.last_sent_packet_,
                                transport_.last_sent_packet_len_, &rtp_header));
  // Marker Bit should be set to 0 for rest of the packets.
  EXPECT_FALSE(rtp_header.markerBit);
}

TEST_F(RtpSenderTestWithoutPacer, BytesReportedCorrectly) {
  const char* kPayloadName = "GENERIC";
  const uint8_t kPayloadType = 127;
  rtp_sender_->SetSSRC(1234);
  rtp_sender_->SetRtxSsrc(4321);
  rtp_sender_->SetRtxPayloadType(kPayloadType - 1, kPayloadType);
  rtp_sender_->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);

  ASSERT_EQ(0, rtp_sender_->RegisterPayload(kPayloadName, kPayloadType, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
                      kVideoFrameKey, kPayloadType, 1234, 4321, payload,
                      sizeof(payload), nullptr, nullptr, nullptr));

  // Will send 2 full-size padding packets.
  rtp_sender_->TimeToSendPadding(1, PacketInfo::kNotAProbe);
  rtp_sender_->TimeToSendPadding(1, PacketInfo::kNotAProbe);

  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  rtp_sender_->GetDataCounters(&rtp_stats, &rtx_stats);

  // Payload + 1-byte generic header.
  EXPECT_GT(rtp_stats.first_packet_time_ms, -1);
  EXPECT_EQ(rtp_stats.transmitted.payload_bytes, sizeof(payload) + 1);
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

TEST_F(RtpSenderTestWithoutPacer, RespectsNackBitrateLimit) {
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
  EXPECT_EQ(kNumPackets, transport_.packets_sent_);

  fake_clock_.AdvanceTimeMilliseconds(1000 - kNumPackets);

  // Resending should work - brings the bandwidth up to the limit.
  // NACK bitrate is capped to the same bitrate as the encoder, since the max
  // protection overhead is 50% (see MediaOptimization::SetTargetRates).
  rtp_sender_->OnReceivedNack(sequence_numbers, 0);
  EXPECT_EQ(kNumPackets * 2, transport_.packets_sent_);

  // Must be at least 5ms in between retransmission attempts.
  fake_clock_.AdvanceTimeMilliseconds(5);

  // Resending should not work, bandwidth exceeded.
  rtp_sender_->OnReceivedNack(sequence_numbers, 0);
  EXPECT_EQ(kNumPackets * 2, transport_.packets_sent_);
}

// Verify that all packets of a frame have CVO byte set.
TEST_F(RtpSenderVideoTest, SendVideoWithCVO) {
  RTPVideoHeader hdr = {0};
  hdr.rotation = kVideoRotation_90;

  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));
  EXPECT_TRUE(rtp_sender_->ActivateCVORtpHeaderExtension());

  EXPECT_EQ(
      RtpUtility::Word32Align(kRtpOneByteHeaderLength + kVideoRotationLength),
      rtp_sender_->RtpHeaderExtensionLength());

  rtp_sender_video_->SendVideo(kRtpVideoGeneric, kVideoFrameKey, kPayload,
                               kTimestamp, 0, packet_, sizeof(packet_), nullptr,
                               &hdr);

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionVideoRotation, kVideoRotationExtensionId);

  // Verify that this packet does have CVO byte.
  VerifyCVOPacket(
      reinterpret_cast<uint8_t*>(transport_.sent_packets_[0]->data()),
      transport_.sent_packets_[0]->size(), true, &map, kSeqNum, hdr.rotation);

  // Verify that this packet does have CVO byte.
  VerifyCVOPacket(
      reinterpret_cast<uint8_t*>(transport_.sent_packets_[1]->data()),
      transport_.sent_packets_[1]->size(), true, &map, kSeqNum + 1,
      hdr.rotation);
}

// Make sure rotation is parsed correctly when the Camera (C) and Flip (F) bits
// are set in the CVO byte.
TEST_F(RtpSenderVideoTest, SendVideoWithCameraAndFlipCVO) {
  // Test extracting rotation when Camera (C) and Flip (F) bits are zero.
  EXPECT_EQ(kVideoRotation_0, ConvertCVOByteToVideoRotation(0));
  EXPECT_EQ(kVideoRotation_90, ConvertCVOByteToVideoRotation(1));
  EXPECT_EQ(kVideoRotation_180, ConvertCVOByteToVideoRotation(2));
  EXPECT_EQ(kVideoRotation_270, ConvertCVOByteToVideoRotation(3));
  // Test extracting rotation when Camera (C) and Flip (F) bits are set.
  const int flip_bit = 1 << 2;
  const int camera_bit = 1 << 3;
  EXPECT_EQ(kVideoRotation_0,
            ConvertCVOByteToVideoRotation(flip_bit | camera_bit | 0));
  EXPECT_EQ(kVideoRotation_90,
            ConvertCVOByteToVideoRotation(flip_bit | camera_bit | 1));
  EXPECT_EQ(kVideoRotation_180,
            ConvertCVOByteToVideoRotation(flip_bit | camera_bit | 2));
  EXPECT_EQ(kVideoRotation_270,
            ConvertCVOByteToVideoRotation(flip_bit | camera_bit | 3));
}

}  // namespace webrtc
