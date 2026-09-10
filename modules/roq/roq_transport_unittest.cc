/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/roq_transport.h"

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "api/roq/roq_session.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc::roq {
namespace {

class FakeRoqSession final : public RoqSession {
 public:
  bool SendReplaySafeMessage(std::span<const uint8_t>) override {
    return false;
  }
  void SetEarlyDatagramsAllowed(bool) override {}
  void RequireIndependentVideoFrame(const std::string&, uint32_t) override {}
  void RequestPing() override {}
  void Abort() override {}
  void BeginReceiveFlowUpdate() override {}
  void UpdateReceiveFlowIds(std::vector<uint64_t>) override {}
  bool Start(std::string*) override { return true; }
  bool WaitUntilReady(TimeDelta) override { return true; }
  bool WaitUntilEstablished(TimeDelta) override { return true; }
  RoqSendResult SendMedia(uint64_t flow_id,
                          std::span<const uint8_t> packet,
                          RoqDatagramMetadata metadata,
                          std::function<void()> on_submitted) override {
    flow_id_ = flow_id;
    bytes_.assign(packet.begin(), packet.end());
    metadata_ = std::move(metadata);
    on_submitted_ = std::move(on_submitted);
    submissions_.push_back(on_submitted_);
    return RoqSendResult::kQueued;
  }
  bool ReserveMediaBatch(const RoqDatagramMetadata& metadata,
                         size_t packet_count,
                         size_t byte_size) override {
    reserved_metadata_ = metadata;
    reserved_packet_count_ = packet_count;
    reserved_byte_size_ = byte_size;
    return reserve_result_;
  }
  void RecordInvalidDatagram() override { ++invalid_datagrams_; }
  void UpdatePeerDatagramPayloadLimit(size_t) override {}
  void Close(uint64_t, std::string) override {}
  bool WaitUntilClosed(TimeDelta) override { return true; }
  RoqSessionStats stats() const override { return {}; }
  std::string error() const override { return {}; }

  uint64_t flow_id_ = 0;
  std::vector<uint8_t> bytes_;
  RoqDatagramMetadata metadata_;
  std::function<void()> on_submitted_;
  std::vector<std::function<void()>> submissions_;
  size_t invalid_datagrams_ = 0;
  bool reserve_result_ = true;
  RoqDatagramMetadata reserved_metadata_;
  size_t reserved_packet_count_ = 0;
  size_t reserved_byte_size_ = 0;
};

RoqPacketTransportConfig VideoConfig(FakeRoqSession* session, Clock* clock) {
  return {.session = session,
          .media_type = RoqMediaType::kVideo,
          .rtp_flow_id = 1,
          .rtcp_flow_id = 2,
          .primary_payload_type = 96,
          .primary_ssrc = 22222222,
          .rtx_payload_type = 97,
          .rtx_ssrc = 22222224,
          .clock = clock};
}

#if defined(RTC_ENABLE_H265)
TEST(RoqPacketTransportTest, ClassifiesH265UsingConfiguredCodec) {
  SimulatedClock clock(Timestamp::Millis(100));
  FakeRoqSession session;
  RoqPacketTransportConfig config = VideoConfig(&session, &clock);
  config.video_codec = kVideoCodecH265;
  config.primary_payload_type = 116;
  config.rtx_payload_type = 117;
  RoqPacketTransport transport(std::move(config));
  constexpr std::array<uint8_t, 15> packet = {
      0x80, 0xf4, 0x00, 0x2a, 0x00, 0x01, 0x5f, 0x90,
      0x01, 0x53, 0x15, 0x8e, 0x26, 0x02, 0xff};

  ASSERT_TRUE(transport.SendRtp(packet, PacketOptions()));
  EXPECT_EQ(session.metadata_.video_codec, kVideoCodecH265);
  EXPECT_TRUE(session.metadata_.is_key);
  EXPECT_EQ(session.metadata_.priority, RoqDatagramPriority::kVideoKey);
}
#endif

TEST(RoqPacketTransportTest, DefersAcceptedCallbackUntilQuicheSubmission) {
  SimulatedClock clock(Timestamp::Millis(100));
  FakeRoqSession session;
  bool accepted = false;
  RoqPacketTransport transport(VideoConfig(&session, &clock),
                               [&](uint64_t, std::span<const uint8_t>,
                                   const PacketOptions&) { accepted = true; });
  constexpr std::array<uint8_t, 14> packet = {0x80, 0xe0, 0x00, 0x2a, 0x00,
                                              0x01, 0x5f, 0x90, 0x01, 0x53,
                                              0x15, 0x8e, 0x65, 0xaa};

  EXPECT_TRUE(transport.SendRtp(packet, PacketOptions()));
  EXPECT_FALSE(accepted);
  EXPECT_EQ(transport.accepted_rtp_packets(), 0u);
  ASSERT_TRUE(session.on_submitted_);
  session.on_submitted_();
  EXPECT_TRUE(accepted);
  EXPECT_EQ(transport.accepted_rtp_packets(), 1u);
}

TEST(RoqPacketTransportTest, ClassifiesKeyVideoFrame) {
  SimulatedClock clock(Timestamp::Millis(100));
  FakeRoqSession session;
  RoqPacketTransport transport(VideoConfig(&session, &clock));
  constexpr std::array<uint8_t, 14> packet = {0x80, 0xe0, 0x00, 0x2a, 0x00,
                                              0x01, 0x5f, 0x90, 0x01, 0x53,
                                              0x15, 0x8e, 0x65, 0xaa};

  ASSERT_TRUE(transport.SendRtp(packet, PacketOptions()));
  EXPECT_EQ(session.flow_id_, 1u);
  EXPECT_EQ(session.metadata_.priority, RoqDatagramPriority::kVideoKey);
  EXPECT_EQ(session.metadata_.deadline, Timestamp::Millis(250));
  EXPECT_EQ(session.metadata_.frame_id,
            (RoqFrameId{.ssrc = 22222222, .timestamp = 90000}));
  EXPECT_EQ(session.metadata_.packet_id,
            (RoqPacketId{.ssrc = 22222222, .sequence_number = 42}));
}

TEST(RoqPacketTransportTest, ReservesPacketizedVideoBeforeRtpSubmission) {
  SimulatedClock clock(Timestamp::Millis(100));
  FakeRoqSession session;
  RoqPacketTransport transport(VideoConfig(&session, &clock));

  EXPECT_TRUE(transport.ReserveRtpBatch({.ssrc = 22222222,
                                         .timestamp = 90000,
                                         .packet_count = 3,
                                         .byte_size = 3600,
                                         .key_frame = true}));

  EXPECT_EQ(session.reserved_packet_count_, 3u);
  EXPECT_EQ(session.reserved_byte_size_, 3600u);
  EXPECT_EQ(session.reserved_metadata_.priority,
            RoqDatagramPriority::kVideoKey);
  EXPECT_EQ(session.reserved_metadata_.frame_id,
            (RoqFrameId{.ssrc = 22222222, .timestamp = 90000}));
}

TEST(RoqPacketTransportTest, DeclaresExternalTransportPacing) {
  SimulatedClock clock(Timestamp::Millis(100));
  FakeRoqSession session;
  RoqPacketTransport transport(VideoConfig(&session, &clock));

  EXPECT_EQ(transport.GetRtpPacingMode(), RtpPacingMode::kExternalTransport);
}

TEST(RoqPacketTransportTest, ReportsEachSubmittedKeyVideoFrame) {
  SimulatedClock clock(Timestamp::Millis(100));
  FakeRoqSession session;
  int submitted_key_frames = 0;
  RoqPacketTransportConfig config = VideoConfig(&session, &clock);
  config.key_frame_submitted =
      [&](uint32_t, uint32_t, uint16_t) { ++submitted_key_frames; };
  RoqPacketTransport transport(std::move(config));
  constexpr std::array<uint8_t, 22> first = {
      0x80, 0xe0, 0x00, 0x2a, 0x00, 0x01, 0x5f, 0x90, 0x01, 0x53, 0x15,
      0x8e, 0x18, 0x00, 0x01, 0x67, 0x00, 0x01, 0x68, 0x00, 0x01, 0x65};
  constexpr std::array<uint8_t, 22> second = {
      0x80, 0xe0, 0x00, 0x2b, 0x00, 0x02, 0xbf, 0x20, 0x01, 0x53, 0x15,
      0x8e, 0x18, 0x00, 0x01, 0x67, 0x00, 0x01, 0x68, 0x00, 0x01, 0x65};

  ASSERT_TRUE(transport.SendRtp(first, PacketOptions()));
  ASSERT_TRUE(session.on_submitted_);
  session.on_submitted_();
  ASSERT_TRUE(transport.SendRtp(second, PacketOptions()));
  ASSERT_TRUE(session.on_submitted_);
  session.on_submitted_();

  EXPECT_EQ(submitted_key_frames, 2);
}

TEST(RoqPacketTransportTest, ReportsKeyFrameOnlyAfterCompleteRecoveryUnitSubmits) {
  SimulatedClock clock(Timestamp::Millis(100));
  FakeRoqSession session;
  int submitted_key_frames = 0;
  RoqPacketTransportConfig config = VideoConfig(&session, &clock);
  config.key_frame_submitted =
      [&](uint32_t, uint32_t, uint16_t) { ++submitted_key_frames; };
  RoqPacketTransport transport(std::move(config));
  constexpr std::array<uint8_t, 14> sps = {
      0x80, 0x60, 0x00, 0x2a, 0x00, 0x01, 0x5f,
      0x80, 0x01, 0x53, 0x15, 0x8e, 0x67, 0xaa};
  constexpr std::array<uint8_t, 14> pps = {
      0x80, 0x60, 0x00, 0x2b, 0x00, 0x01, 0x5f,
      0x80, 0x01, 0x53, 0x15, 0x8e, 0x68, 0xbb};
  constexpr std::array<uint8_t, 14> idr_start = {
      0x80, 0x60, 0x00, 0x2c, 0x00, 0x01, 0x60,
      0x00, 0x01, 0x53, 0x15, 0x8e, 0x65, 0xcc};
  constexpr std::array<uint8_t, 14> idr_marker = {
      0x80, 0xe0, 0x00, 0x2d, 0x00, 0x01, 0x60,
      0x00, 0x01, 0x53, 0x15, 0x8e, 0x65, 0xdd};

  ASSERT_TRUE(transport.SendRtp(sps, PacketOptions()));
  ASSERT_TRUE(transport.SendRtp(pps, PacketOptions()));
  ASSERT_TRUE(transport.SendRtp(idr_start, PacketOptions()));
  ASSERT_TRUE(transport.SendRtp(idr_marker, PacketOptions()));
  ASSERT_EQ(session.submissions_.size(), 4u);
  session.submissions_[3]();
  EXPECT_EQ(submitted_key_frames, 0);
  for (size_t index = 0; index < 3; ++index) {
    session.submissions_[index]();
    EXPECT_EQ(submitted_key_frames, index == 2 ? 1 : 0);
  }
  EXPECT_EQ(submitted_key_frames, 1);
}

TEST(RoqPacketTransportTest, AllowsDeadlinePolicyOverrides) {
  SimulatedClock clock(Timestamp::Millis(100));
  FakeRoqSession session;
  RoqPacketTransportConfig config = VideoConfig(&session, &clock);
  config.video_key_deadline = TimeDelta::Millis(25);
  RoqPacketTransport transport(std::move(config));
  constexpr std::array<uint8_t, 14> packet = {0x80, 0xe0, 0x00, 0x2a, 0x00,
                                              0x01, 0x5f, 0x90, 0x01, 0x53,
                                              0x15, 0x8e, 0x65, 0xaa};

  ASSERT_TRUE(transport.SendRtp(packet, PacketOptions()));
  EXPECT_EQ(session.metadata_.deadline, Timestamp::Millis(125));
}

TEST(RoqPacketTransportTest, MapsRtxToOriginalPrimaryPacket) {
  SimulatedClock clock(Timestamp::Millis(100));
  FakeRoqSession session;
  RoqPacketTransport transport(VideoConfig(&session, &clock));
  constexpr std::array<uint8_t, 15> packet = {0x80, 0x61, 0x00, 0x2a, 0x00,
                                              0x01, 0x5f, 0x90, 0x01, 0x53,
                                              0x15, 0x90, 0xff, 0xff, 0xaa};

  ASSERT_TRUE(transport.SendRtp(packet, PacketOptions()));
  EXPECT_EQ(session.metadata_.original_packet_id,
            (RoqPacketId{.ssrc = 22222222, .sequence_number = 65535}));
}

TEST(RoqPacketTransportTest, ClassifiesCompoundFeedbackAsCriticalRtcp) {
  SimulatedClock clock(Timestamp::Millis(100));
  FakeRoqSession session;
  RoqPacketTransport transport(VideoConfig(&session, &clock));
  constexpr std::array<uint8_t, 20> packet = {0x80, 201, 0x00, 0x01, 0, 0, 0, 2,
                                              0x81, 206, 0x00, 0x02, 0, 0, 0, 1,
                                              0,    0,   0,    2};

  ASSERT_TRUE(transport.SendRtcp(packet, PacketOptions()));
  EXPECT_EQ(session.flow_id_, 2u);
  EXPECT_EQ(session.metadata_.priority, RoqDatagramPriority::kRtcp);
  EXPECT_EQ(session.metadata_.deadline, Timestamp::Millis(150));
}

TEST(RoqPacketTransportTest, ClassifiesPureReportsAsPeriodicRtcp) {
  SimulatedClock clock(Timestamp::Millis(100));
  FakeRoqSession session;
  RoqPacketTransport transport(VideoConfig(&session, &clock));
  constexpr std::array<uint8_t, 16> packet = {
      0x80, 200, 0x00, 0x01, 0, 0, 0, 1, 0x80, 201, 0x00, 0x01, 0, 0, 0, 2};

  ASSERT_TRUE(transport.SendRtcp(packet, PacketOptions()));
  EXPECT_EQ(session.metadata_.priority, RoqDatagramPriority::kPeriodicRtcp);
  EXPECT_EQ(session.metadata_.deadline, Timestamp::Millis(350));
}

TEST(RoqPacketTransportTest, RejectsMalformedPacketBeforeQueueing) {
  SimulatedClock clock(Timestamp::Millis(100));
  FakeRoqSession session;
  RoqPacketTransport transport(VideoConfig(&session, &clock));
  constexpr std::array<uint8_t, 2> packet = {0x80, 0x60};

  EXPECT_FALSE(transport.SendRtp(packet, PacketOptions()));
  EXPECT_TRUE(session.bytes_.empty());
  EXPECT_EQ(session.invalid_datagrams_, 1u);
}

}  // namespace
}  // namespace webrtc::roq
