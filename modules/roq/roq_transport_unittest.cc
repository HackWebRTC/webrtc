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
    return RoqSendResult::kQueued;
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
  size_t invalid_datagrams_ = 0;
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

TEST(RoqPacketTransportTest, ReportsOnlyTheFirstSubmittedKeyVideoFrame) {
  SimulatedClock clock(Timestamp::Millis(100));
  FakeRoqSession session;
  int submitted_key_frames = 0;
  RoqPacketTransportConfig config = VideoConfig(&session, &clock);
  config.key_frame_submitted =
      [&](uint32_t, uint32_t, uint16_t) { ++submitted_key_frames; };
  RoqPacketTransport transport(std::move(config));
  constexpr std::array<uint8_t, 14> first = {
      0x80, 0xe0, 0x00, 0x2a, 0x00, 0x01, 0x5f,
      0x90, 0x01, 0x53, 0x15, 0x8e, 0x65, 0xaa};
  constexpr std::array<uint8_t, 14> second = {
      0x80, 0xe0, 0x00, 0x2b, 0x00, 0x02, 0xbf,
      0x20, 0x01, 0x53, 0x15, 0x8e, 0x65, 0xbb};

  ASSERT_TRUE(transport.SendRtp(first, PacketOptions()));
  ASSERT_TRUE(session.on_submitted_);
  session.on_submitted_();
  ASSERT_TRUE(transport.SendRtp(second, PacketOptions()));
  ASSERT_TRUE(session.on_submitted_);
  session.on_submitted_();

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
