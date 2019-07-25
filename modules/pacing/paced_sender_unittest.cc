/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/paced_sender.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "modules/pacing/packet_router.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

namespace {
constexpr unsigned kFirstClusterBps = 900000;
constexpr unsigned kSecondClusterBps = 1800000;

// The error stems from truncating the time interval of probe packets to integer
// values. This results in probing slightly higher than the target bitrate.
// For 1.8 Mbps, this comes to be about 120 kbps with 1200 probe packets.
constexpr int kBitrateProbingError = 150000;

const float kPaceMultiplier = 2.5f;

constexpr uint32_t kAudioSsrc = 12345;
constexpr uint32_t kVideoSsrc = 234565;
constexpr uint32_t kVideoRtxSsrc = 34567;
constexpr uint32_t kFlexFecSsrc = 45678;
}  // namespace

namespace webrtc {
namespace test {

static const int kTargetBitrateBps = 800000;

enum class PacerMode { kReferencePackets, kOwnPackets };
std::string GetFieldTrialStirng(PacerMode mode) {
  std::string field_trial = "WebRTC-Pacer-LegacyPacketReferencing/";
  switch (mode) {
    case PacerMode::kOwnPackets:
      field_trial += "Disabled";
      break;
    case PacerMode::kReferencePackets:
      field_trial += "Enabled";
      break;
  }
  field_trial += "/";
  return field_trial;
}

// Mock callback proxy, where both new and old api redirects to common mock
// methods that focus on core aspects.
class MockPacedSenderCallback : public PacketRouter {
 public:
  RtpPacketSendResult TimeToSendPacket(uint32_t ssrc,
                                       uint16_t sequence_number,
                                       int64_t capture_timestamp,
                                       bool retransmission,
                                       const PacedPacketInfo& packet_info) {
    SendPacket(ssrc, sequence_number, capture_timestamp, retransmission, false);
    return RtpPacketSendResult::kSuccess;
  }

  void SendPacket(std::unique_ptr<RtpPacketToSend> packet,
                  const PacedPacketInfo& cluster_info) override {
    SendPacket(packet->Ssrc(), packet->SequenceNumber(),
               packet->capture_time_ms(),
               packet->packet_type() == RtpPacketToSend::Type::kRetransmission,
               packet->packet_type() == RtpPacketToSend::Type::kPadding);
  }

  size_t TimeToSendPadding(size_t bytes,
                           const PacedPacketInfo& packet_info) override {
    return SendPadding(bytes);
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t target_size_bytes) override {
    std::vector<std::unique_ptr<RtpPacketToSend>> ret;
    size_t padding_size = SendPadding(target_size_bytes);
    if (padding_size > 0) {
      auto packet = absl::make_unique<RtpPacketToSend>(nullptr);
      packet->SetPayloadSize(padding_size);
      packet->set_packet_type(RtpPacketToSend::Type::kPadding);
      ret.emplace_back(std::move(packet));
    }
    return ret;
  }

  MOCK_METHOD5(SendPacket,
               void(uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_timestamp,
                    bool retransmission,
                    bool padding));
  MOCK_METHOD1(SendPadding, size_t(size_t target_size));
};

// Mock callback implementing the raw api.
class MockCallback : public PacketRouter {
 public:
  MOCK_METHOD5(TimeToSendPacket,
               RtpPacketSendResult(uint32_t ssrc,
                                   uint16_t sequence_number,
                                   int64_t capture_time_ms,
                                   bool retransmission,
                                   const PacedPacketInfo& pacing_info));
  MOCK_METHOD2(TimeToSendPadding,
               size_t(size_t bytes, const PacedPacketInfo& pacing_info));

  MOCK_METHOD2(SendPacket,
               void(std::unique_ptr<RtpPacketToSend> packet,
                    const PacedPacketInfo& cluster_info));
  MOCK_METHOD1(
      GeneratePadding,
      std::vector<std::unique_ptr<RtpPacketToSend>>(size_t target_size_bytes));
};

// TODO(bugs.webrtc.org/10633): Remove when packets are always owned by pacer.
RtpPacketSender::Priority PacketTypeToPriority(RtpPacketToSend::Type type) {
  switch (type) {
    case RtpPacketToSend::Type::kAudio:
      return RtpPacketSender::Priority::kHighPriority;
    case RtpPacketToSend::Type::kVideo:
      return RtpPacketSender::Priority::kLowPriority;
    case RtpPacketToSend::Type::kRetransmission:
      return RtpPacketSender::Priority::kNormalPriority;
    case RtpPacketToSend::Type::kForwardErrorCorrection:
      return RtpPacketSender::Priority::kLowPriority;
      break;
    case RtpPacketToSend::Type::kPadding:
      RTC_NOTREACHED() << "Unexpected type for legacy path: kPadding";
      break;
  }
  return RtpPacketSender::Priority::kLowPriority;
}

std::unique_ptr<RtpPacketToSend> BuildPacket(RtpPacketToSend::Type type,
                                             uint32_t ssrc,
                                             uint16_t sequence_number,
                                             int64_t capture_time_ms,
                                             size_t size) {
  auto packet = absl::make_unique<RtpPacketToSend>(nullptr);
  packet->set_packet_type(type);
  packet->SetSsrc(ssrc);
  packet->SetSequenceNumber(sequence_number);
  packet->set_capture_time_ms(capture_time_ms);
  packet->SetPayloadSize(size);
  return packet;
}

class PacedSenderPadding : public PacketRouter {
 public:
  static const size_t kPaddingPacketSize = 224;

  PacedSenderPadding() : padding_sent_(0) {}

  RtpPacketSendResult TimeToSendPacket(
      uint32_t ssrc,
      uint16_t sequence_number,
      int64_t capture_time_ms,
      bool retransmission,
      const PacedPacketInfo& pacing_info) override {
    return RtpPacketSendResult::kSuccess;
  }

  void SendPacket(std::unique_ptr<RtpPacketToSend> packet,
                  const PacedPacketInfo& pacing_info) override {}

  size_t TimeToSendPadding(size_t bytes,
                           const PacedPacketInfo& pacing_info) override {
    size_t num_packets = (bytes + kPaddingPacketSize - 1) / kPaddingPacketSize;
    padding_sent_ += kPaddingPacketSize * num_packets;
    return kPaddingPacketSize * num_packets;
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t target_size_bytes) override {
    size_t num_packets =
        (target_size_bytes + kPaddingPacketSize - 1) / kPaddingPacketSize;
    std::vector<std::unique_ptr<RtpPacketToSend>> packets;
    for (size_t i = 0; i < num_packets; ++i) {
      packets.emplace_back(absl::make_unique<RtpPacketToSend>(nullptr));
      packets.back()->SetPadding(kPaddingPacketSize);
      packets.back()->set_packet_type(RtpPacketToSend::Type::kPadding);
      padding_sent_ += kPaddingPacketSize;
    }
    return packets;
  }

  size_t padding_sent() { return padding_sent_; }

 private:
  size_t padding_sent_;
};

class PacedSenderProbing : public PacketRouter {
 public:
  PacedSenderProbing() : packets_sent_(0), padding_sent_(0) {}

  RtpPacketSendResult TimeToSendPacket(
      uint32_t ssrc,
      uint16_t sequence_number,
      int64_t capture_time_ms,
      bool retransmission,
      const PacedPacketInfo& pacing_info) override {
    ++packets_sent_;
    return RtpPacketSendResult::kSuccess;
  }

  void SendPacket(std::unique_ptr<RtpPacketToSend> packet,
                  const PacedPacketInfo& pacing_info) override {
    if (packet->packet_type() != RtpPacketToSend::Type::kPadding) {
      ++packets_sent_;
    }
  }

  size_t TimeToSendPadding(size_t bytes,
                           const PacedPacketInfo& pacing_info) override {
    padding_sent_ += bytes;
    return padding_sent_;
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t target_size_bytes) override {
    std::vector<std::unique_ptr<RtpPacketToSend>> packets;
    packets.emplace_back(absl::make_unique<RtpPacketToSend>(nullptr));
    packets.back()->SetPadding(target_size_bytes);
    packets.back()->set_packet_type(RtpPacketToSend::Type::kPadding);
    padding_sent_ += target_size_bytes;
    return packets;
  }

  int packets_sent() const { return packets_sent_; }

  int padding_sent() const { return padding_sent_; }

 private:
  int packets_sent_;
  int padding_sent_;
};

class PacedSenderTest : public ::testing::TestWithParam<PacerMode> {
 protected:
  PacedSenderTest()
      : clock_(123456), field_trial_(GetFieldTrialStirng(GetParam())) {
    srand(0);
    // Need to initialize PacedSender after we initialize clock.
    send_bucket_ = absl::make_unique<PacedSender>(&clock_, &callback_, nullptr);
    Init();
  }

  void Init() {
    send_bucket_->CreateProbeCluster(kFirstClusterBps, /*cluster_id=*/0);
    send_bucket_->CreateProbeCluster(kSecondClusterBps, /*cluster_id=*/1);
    // Default to bitrate probing disabled for testing purposes. Probing tests
    // have to enable probing, either by creating a new PacedSender instance or
    // by calling SetProbingEnabled(true).
    send_bucket_->SetProbingEnabled(false);
    send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier, 0);

    clock_.AdvanceTimeMilliseconds(send_bucket_->TimeUntilNextProcess());
  }

  void Send(RtpPacketToSend::Type type,
            uint32_t ssrc,
            uint16_t sequence_number,
            int64_t capture_time_ms,
            size_t size) {
    if (GetParam() == PacerMode::kReferencePackets) {
      send_bucket_->InsertPacket(
          PacketTypeToPriority(type), ssrc, sequence_number, capture_time_ms,
          size, type == RtpPacketToSend::Type::kRetransmission);
    } else {
      send_bucket_->EnqueuePacket(
          BuildPacket(type, ssrc, sequence_number, capture_time_ms, size));
    }
  }

  void SendAndExpectPacket(RtpPacketToSend::Type type,
                           uint32_t ssrc,
                           uint16_t sequence_number,
                           int64_t capture_time_ms,
                           size_t size) {
    Send(type, ssrc, sequence_number, capture_time_ms, size);
    EXPECT_CALL(
        callback_,
        SendPacket(ssrc, sequence_number, capture_time_ms,
                   type == RtpPacketToSend::Type::kRetransmission, false))
        .Times(1);
  }

  void ExpectSendPadding() {
    if (GetParam() == PacerMode::kOwnPackets) {
      EXPECT_CALL(callback_, SendPacket(_, _, _, _, true)).Times(1);
    }
  }

  std::unique_ptr<RtpPacketToSend> BuildRtpPacket(RtpPacketToSend::Type type) {
    auto packet = absl::make_unique<RtpPacketToSend>(nullptr);
    packet->set_packet_type(type);
    switch (type) {
      case RtpPacketToSend::Type::kAudio:
        packet->SetSsrc(kAudioSsrc);
        break;
      case RtpPacketToSend::Type::kVideo:
        packet->SetSsrc(kVideoSsrc);
        break;
      case RtpPacketToSend::Type::kRetransmission:
      case RtpPacketToSend::Type::kPadding:
        packet->SetSsrc(kVideoRtxSsrc);
        break;
      case RtpPacketToSend::Type::kForwardErrorCorrection:
        packet->SetSsrc(kFlexFecSsrc);
        break;
    }

    packet->SetPayloadSize(234);
    return packet;
  }

  SimulatedClock clock_;
  ScopedFieldTrials field_trial_;
  MockPacedSenderCallback callback_;
  std::unique_ptr<PacedSender> send_bucket_;
};

class PacedSenderFieldTrialTest : public ::testing::TestWithParam<PacerMode> {
 protected:
  struct MediaStream {
    const RtpPacketToSend::Type type;
    const uint32_t ssrc;
    const size_t packet_size;
    uint16_t seq_num;
  };

  const int kProcessIntervalsPerSecond = 1000 / 5;

  PacedSenderFieldTrialTest() : clock_(123456) {}
  void InsertPacket(PacedSender* pacer, MediaStream* stream) {
    if (GetParam() == PacerMode::kReferencePackets) {
      pacer->InsertPacket(PacketTypeToPriority(stream->type), stream->ssrc,
                          stream->seq_num++, clock_.TimeInMilliseconds(),
                          stream->packet_size, false);
    } else {
      pacer->EnqueuePacket(
          BuildPacket(stream->type, stream->ssrc, stream->seq_num++,
                      clock_.TimeInMilliseconds(), stream->packet_size));
    }
  }
  void ProcessNext(PacedSender* pacer) {
    clock_.AdvanceTimeMilliseconds(5);
    pacer->Process();
  }
  MediaStream audio{/*type*/ RtpPacketToSend::Type::kAudio,
                    /*ssrc*/ 3333, /*packet_size*/ 100, /*seq_num*/ 1000};
  MediaStream video{/*type*/ RtpPacketToSend::Type::kVideo,
                    /*ssrc*/ 4444, /*packet_size*/ 1000, /*seq_num*/ 1000};
  SimulatedClock clock_;
  MockPacedSenderCallback callback_;
};

TEST_P(PacedSenderFieldTrialTest, DefaultNoPaddingInSilence) {
  PacedSender pacer(&clock_, &callback_, nullptr);
  pacer.SetPacingRates(kTargetBitrateBps, 0);
  // Video packet to reset last send time and provide padding data.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  pacer.Process();
  EXPECT_CALL(callback_, SendPadding).Times(0);
  // Waiting 500 ms should not trigger sending of padding.
  clock_.AdvanceTimeMilliseconds(500);
  pacer.Process();
}

TEST_P(PacedSenderFieldTrialTest, PaddingInSilenceWithTrial) {
  ScopedFieldTrials trial(GetFieldTrialStirng(GetParam()) +
                          "WebRTC-Pacer-PadInSilence/Enabled/");
  PacedSender pacer(&clock_, &callback_, nullptr);
  pacer.SetPacingRates(kTargetBitrateBps, 0);
  // Video packet to reset last send time and provide padding data.
  InsertPacket(&pacer, &video);
  if (GetParam() == PacerMode::kReferencePackets) {
    // Only payload, not padding, sent by pacer in legacy mode.
    EXPECT_CALL(callback_, SendPacket).Times(1);
  } else {
    EXPECT_CALL(callback_, SendPacket).Times(2);
  }
  clock_.AdvanceTimeMilliseconds(5);
  pacer.Process();
  EXPECT_CALL(callback_, SendPadding).WillOnce(Return(1000));
  // Waiting 500 ms should trigger sending of padding.
  clock_.AdvanceTimeMilliseconds(500);
  pacer.Process();
}

TEST_P(PacedSenderFieldTrialTest, DefaultCongestionWindowAffectsAudio) {
  EXPECT_CALL(callback_, SendPadding).Times(0);
  PacedSender pacer(&clock_, &callback_, nullptr);
  pacer.SetPacingRates(10000000, 0);
  pacer.SetCongestionWindow(800);
  pacer.UpdateOutstandingData(0);
  // Video packet fills congestion window.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
  // Audio packet blocked due to congestion.
  InsertPacket(&pacer, &audio);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  ProcessNext(&pacer);
  ProcessNext(&pacer);
  // Audio packet unblocked when congestion window clear.
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  pacer.UpdateOutstandingData(0);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
}

TEST_P(PacedSenderFieldTrialTest, CongestionWindowDoesNotAffectAudioInTrial) {
  ScopedFieldTrials trial(GetFieldTrialStirng(GetParam()) +
                          "WebRTC-Pacer-BlockAudio/Disabled/");
  EXPECT_CALL(callback_, SendPadding).Times(0);
  PacedSender pacer(&clock_, &callback_, nullptr);
  pacer.SetPacingRates(10000000, 0);
  pacer.SetCongestionWindow(800);
  pacer.UpdateOutstandingData(0);
  // Video packet fills congestion window.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
  // Audio not blocked due to congestion.
  InsertPacket(&pacer, &audio);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
}

TEST_P(PacedSenderFieldTrialTest, DefaultBudgetAffectsAudio) {
  PacedSender pacer(&clock_, &callback_, nullptr);
  pacer.SetPacingRates(video.packet_size / 3 * 8 * kProcessIntervalsPerSecond,
                       0);
  // Video fills budget for following process periods.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
  // Audio packet blocked due to budget limit.
  EXPECT_CALL(callback_, SendPacket).Times(0);
  InsertPacket(&pacer, &audio);
  ProcessNext(&pacer);
  ProcessNext(&pacer);
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  // Audio packet unblocked when the budget has recovered.
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
  ProcessNext(&pacer);
}

TEST_P(PacedSenderFieldTrialTest, BudgetDoesNotAffectAudioInTrial) {
  ScopedFieldTrials trial(GetFieldTrialStirng(GetParam()) +
                          "WebRTC-Pacer-BlockAudio/Disabled/");
  EXPECT_CALL(callback_, SendPadding).Times(0);
  PacedSender pacer(&clock_, &callback_, nullptr);
  pacer.SetPacingRates(video.packet_size / 3 * 8 * kProcessIntervalsPerSecond,
                       0);
  // Video fills budget for following process periods.
  InsertPacket(&pacer, &video);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  ProcessNext(&pacer);
  // Audio packet not blocked due to budget limit.
  EXPECT_CALL(callback_, SendPacket).Times(1);
  InsertPacket(&pacer, &audio);
  ProcessNext(&pacer);
}

INSTANTIATE_TEST_SUITE_P(ReferencingAndOwningPackets,
                         PacedSenderFieldTrialTest,
                         ::testing::Values(PacerMode::kReferencePackets,
                                           PacerMode::kOwnPackets));

TEST_P(PacedSenderTest, FirstSentPacketTimeIsSet) {
  uint16_t sequence_number = 1234;
  const uint32_t kSsrc = 12345;
  const size_t kSizeBytes = 250;
  const size_t kPacketToSend = 3;
  const int64_t kStartMs = clock_.TimeInMilliseconds();

  // No packet sent.
  EXPECT_EQ(-1, send_bucket_->FirstSentPacketTimeMs());

  for (size_t i = 0; i < kPacketToSend; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, kSsrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kSizeBytes);
    send_bucket_->Process();
    clock_.AdvanceTimeMilliseconds(send_bucket_->TimeUntilNextProcess());
  }
  EXPECT_EQ(kStartMs, send_bucket_->FirstSentPacketTimeMs());
}

TEST_P(PacedSenderTest, QueuePacket) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }

  int64_t queued_packet_timestamp = clock_.TimeInMilliseconds();
  Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number,
       queued_packet_timestamp, 250);
  EXPECT_EQ(packets_to_send + 1, send_bucket_->QueueSizePackets());
  send_bucket_->Process();
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  EXPECT_CALL(callback_, SendPadding).Times(0);
  clock_.AdvanceTimeMilliseconds(4);
  EXPECT_EQ(1, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(1);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(1u, send_bucket_->QueueSizePackets());
  EXPECT_CALL(callback_, SendPacket(ssrc, sequence_number++,
                                    queued_packet_timestamp, false, false))
      .Times(1);
  send_bucket_->Process();
  sequence_number++;
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());

  // We can send packets_to_send -1 packets of size 250 during the current
  // interval since one packet has already been sent.
  for (size_t i = 0; i < packets_to_send - 1; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }
  Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
       clock_.TimeInMilliseconds(), 250);
  EXPECT_EQ(packets_to_send, send_bucket_->QueueSizePackets());
  send_bucket_->Process();
  EXPECT_EQ(1u, send_bucket_->QueueSizePackets());
}

TEST_P(PacedSenderTest, PaceQueuedPackets) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }

  for (size_t j = 0; j < packets_to_send_per_interval * 10; ++j) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), 250);
  }
  EXPECT_EQ(packets_to_send_per_interval + packets_to_send_per_interval * 10,
            send_bucket_->QueueSizePackets());
  send_bucket_->Process();
  EXPECT_EQ(packets_to_send_per_interval * 10,
            send_bucket_->QueueSizePackets());
  EXPECT_CALL(callback_, SendPadding).Times(0);
  for (int k = 0; k < 10; ++k) {
    EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
    clock_.AdvanceTimeMilliseconds(5);
    EXPECT_CALL(callback_, SendPacket(ssrc, _, _, false, false))
        .Times(packets_to_send_per_interval);
    EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
    send_bucket_->Process();
  }
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());
  send_bucket_->Process();

  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }
  Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number,
       clock_.TimeInMilliseconds(), 250);
  send_bucket_->Process();
  EXPECT_EQ(1u, send_bucket_->QueueSizePackets());
}

TEST_P(PacedSenderTest, RepeatedRetransmissionsAllowed) {
  // Send one packet, then two retransmissions of that packet.
  for (size_t i = 0; i < 3; i++) {
    constexpr uint32_t ssrc = 333;
    constexpr uint16_t sequence_number = 444;
    constexpr size_t bytes = 250;
    bool is_retransmission = (i != 0);  // Original followed by retransmissions.
    SendAndExpectPacket(
        is_retransmission ? RtpPacketToSend::Type::kRetransmission
                          : RtpPacketToSend::Type::kVideo,
        ssrc, sequence_number, clock_.TimeInMilliseconds(), bytes);
    clock_.AdvanceTimeMilliseconds(5);
  }
  send_bucket_->Process();
}

TEST_P(PacedSenderTest, CanQueuePacketsWithSameSequenceNumberOnDifferentSsrcs) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number,
                      clock_.TimeInMilliseconds(), 250);

  // Expect packet on second ssrc to be queued and sent as well.
  SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc + 1, sequence_number,
                      clock_.TimeInMilliseconds(), 250);

  clock_.AdvanceTimeMilliseconds(1000);
  send_bucket_->Process();
}

TEST_P(PacedSenderTest, Padding) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier,
                               kTargetBitrateBps);

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }
  // No padding is expected since we have sent too much already.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());

  // 5 milliseconds later should not send padding since we filled the buffers
  // initially.
  EXPECT_CALL(callback_, SendPadding(250)).Times(0);
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();

  // 5 milliseconds later we have enough budget to send some padding.
  EXPECT_CALL(callback_, SendPadding(250)).WillOnce(Return(250));
  ExpectSendPadding();
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();
}

TEST_P(PacedSenderTest, NoPaddingBeforeNormalPacket) {
  send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier,
                               kTargetBitrateBps);

  EXPECT_CALL(callback_, SendPadding).Times(0);
  send_bucket_->Process();
  clock_.AdvanceTimeMilliseconds(send_bucket_->TimeUntilNextProcess());

  send_bucket_->Process();
  clock_.AdvanceTimeMilliseconds(send_bucket_->TimeUntilNextProcess());

  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                      capture_time_ms, 250);
  EXPECT_CALL(callback_, SendPadding(250)).WillOnce(Return(250));
  ExpectSendPadding();
  send_bucket_->Process();
}

TEST_P(PacedSenderTest, VerifyPaddingUpToBitrate) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  const int kTimeStep = 5;
  const int64_t kBitrateWindow = 100;
  send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier,
                               kTargetBitrateBps);

  int64_t start_time = clock_.TimeInMilliseconds();
  while (clock_.TimeInMilliseconds() - start_time < kBitrateWindow) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        capture_time_ms, 250);
    EXPECT_CALL(callback_, SendPadding(250)).WillOnce(Return(250));
    ExpectSendPadding();
    send_bucket_->Process();
    clock_.AdvanceTimeMilliseconds(kTimeStep);
  }
}

TEST_P(PacedSenderTest, VerifyAverageBitrateVaryingMediaPayload) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  const int kTimeStep = 5;
  const int64_t kBitrateWindow = 10000;
  PacedSenderPadding callback;
  send_bucket_.reset(new PacedSender(&clock_, &callback, nullptr));
  send_bucket_->SetProbingEnabled(false);
  send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier,
                               kTargetBitrateBps);

  int64_t start_time = clock_.TimeInMilliseconds();
  size_t media_bytes = 0;
  while (clock_.TimeInMilliseconds() - start_time < kBitrateWindow) {
    int rand_value = rand();  // NOLINT (rand_r instead of rand)
    size_t media_payload = rand_value % 100 + 200;  // [200, 300] bytes.
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         capture_time_ms, media_payload);
    media_bytes += media_payload;
    clock_.AdvanceTimeMilliseconds(kTimeStep);
    send_bucket_->Process();
  }
  EXPECT_NEAR(kTargetBitrateBps / 1000,
              static_cast<int>(8 * (media_bytes + callback.padding_sent()) /
                               kBitrateWindow),
              1);
}

TEST_P(PacedSenderTest, Priority) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  int64_t capture_time_ms_low_priority = 1234567;

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kRetransmission, ssrc,
                        sequence_number++, clock_.TimeInMilliseconds(), 250);
  }
  send_bucket_->Process();
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());

  // Expect normal and low priority to be queued and high to pass through.
  Send(RtpPacketToSend::Type::kVideo, ssrc_low_priority, sequence_number++,
       capture_time_ms_low_priority, 250);

  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    Send(RtpPacketToSend::Type::kRetransmission, ssrc, sequence_number++,
         capture_time_ms, 250);
  }
  Send(RtpPacketToSend::Type::kAudio, ssrc, sequence_number++, capture_time_ms,
       250);

  // Expect all high and normal priority to be sent out first.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  EXPECT_CALL(callback_, SendPacket(ssrc, _, capture_time_ms, _, _))
      .Times(packets_to_send_per_interval + 1);

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();
  EXPECT_EQ(1u, send_bucket_->QueueSizePackets());

  EXPECT_CALL(callback_, SendPacket(ssrc_low_priority, _,
                                    capture_time_ms_low_priority, _, _))
      .Times(1);

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();
}

TEST_P(PacedSenderTest, RetransmissionPriority) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 45678;
  int64_t capture_time_ms_retransmission = 56789;

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  send_bucket_->Process();
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());

  // Alternate retransmissions and normal packets.
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         capture_time_ms, 250);
    Send(RtpPacketToSend::Type::kRetransmission, ssrc, sequence_number++,
         capture_time_ms_retransmission, 250);
  }
  EXPECT_EQ(2 * packets_to_send_per_interval, send_bucket_->QueueSizePackets());

  // Expect all retransmissions to be sent out first despite having a later
  // capture time.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  EXPECT_CALL(callback_, SendPacket(_, _, _, false, _)).Times(0);
  EXPECT_CALL(callback_,
              SendPacket(ssrc, _, capture_time_ms_retransmission, true, _))
      .Times(packets_to_send_per_interval);

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();
  EXPECT_EQ(packets_to_send_per_interval, send_bucket_->QueueSizePackets());

  // Expect the remaining (non-retransmission) packets to be sent.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  EXPECT_CALL(callback_, SendPacket(_, _, _, true, _)).Times(0);
  EXPECT_CALL(callback_, SendPacket(ssrc, _, capture_time_ms, false, _))
      .Times(packets_to_send_per_interval);

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  send_bucket_->Process();

  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());
}

TEST_P(PacedSenderTest, HighPrioDoesntAffectBudget) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  // As high prio packets doesn't affect the budget, we should be able to send
  // a high number of them at once.
  for (int i = 0; i < 25; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kAudio, ssrc, sequence_number++,
                        capture_time_ms, 250);
  }
  send_bucket_->Process();
  // Low prio packets does affect the budget.
  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }
  Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number, capture_time_ms,
       250);
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  EXPECT_EQ(1u, send_bucket_->QueueSizePackets());
  EXPECT_CALL(callback_,
              SendPacket(ssrc, sequence_number++, capture_time_ms, false, _))
      .Times(1);
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  EXPECT_EQ(0u, send_bucket_->QueueSizePackets());
}

TEST_P(PacedSenderTest, SendsOnlyPaddingWhenCongested) {
  uint32_t ssrc = 202020;
  uint16_t sequence_number = 1000;
  int kPacketSize = 250;
  int kCongestionWindow = kPacketSize * 10;

  send_bucket_->UpdateOutstandingData(0);
  send_bucket_->SetCongestionWindow(kCongestionWindow);
  int sent_data = 0;
  while (sent_data < kCongestionWindow) {
    sent_data += kPacketSize;
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize);
    clock_.AdvanceTimeMilliseconds(5);
    send_bucket_->Process();
  }
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  EXPECT_CALL(callback_, SendPadding).Times(0);

  size_t blocked_packets = 0;
  int64_t expected_time_until_padding = 500;
  while (expected_time_until_padding > 5) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), kPacketSize);
    blocked_packets++;
    clock_.AdvanceTimeMilliseconds(5);
    send_bucket_->Process();
    expected_time_until_padding -= 5;
  }
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, SendPadding(1)).WillOnce(Return(1));
  ExpectSendPadding();
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  EXPECT_EQ(blocked_packets, send_bucket_->QueueSizePackets());
}

TEST_P(PacedSenderTest, DoesNotAllowOveruseAfterCongestion) {
  uint32_t ssrc = 202020;
  uint16_t seq_num = 1000;
  int size = 1000;
  auto now_ms = [this] { return clock_.TimeInMilliseconds(); };
  EXPECT_CALL(callback_, SendPadding).Times(0);
  // The pacing rate is low enough that the budget should not allow two packets
  // to be sent in a row.
  send_bucket_->SetPacingRates(400 * 8 * 1000 / 5, 0);
  // The congestion window is small enough to only let one packet through.
  send_bucket_->SetCongestionWindow(800);
  send_bucket_->UpdateOutstandingData(0);
  // Not yet budget limited or congested, packet is sent.
  Send(RtpPacketToSend::Type::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  // Packet blocked due to congestion.
  Send(RtpPacketToSend::Type::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  // Packet blocked due to congestion.
  Send(RtpPacketToSend::Type::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  send_bucket_->UpdateOutstandingData(0);
  // Congestion removed and budget has recovered, packet is sent.
  Send(RtpPacketToSend::Type::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(1);
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  send_bucket_->UpdateOutstandingData(0);
  // Should be blocked due to budget limitation as congestion has be removed.
  Send(RtpPacketToSend::Type::kVideo, ssrc, seq_num++, now_ms(), size);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
}

TEST_P(PacedSenderTest, ResumesSendingWhenCongestionEnds) {
  uint32_t ssrc = 202020;
  uint16_t sequence_number = 1000;
  int64_t kPacketSize = 250;
  int64_t kCongestionCount = 10;
  int64_t kCongestionWindow = kPacketSize * kCongestionCount;
  int64_t kCongestionTimeMs = 1000;

  send_bucket_->UpdateOutstandingData(0);
  send_bucket_->SetCongestionWindow(kCongestionWindow);
  int sent_data = 0;
  while (sent_data < kCongestionWindow) {
    sent_data += kPacketSize;
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize);
    clock_.AdvanceTimeMilliseconds(5);
    send_bucket_->Process();
  }
  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, SendPacket).Times(0);
  int unacked_packets = 0;
  for (int duration = 0; duration < kCongestionTimeMs; duration += 5) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), kPacketSize);
    unacked_packets++;
    clock_.AdvanceTimeMilliseconds(5);
    send_bucket_->Process();
  }
  ::testing::Mock::VerifyAndClearExpectations(&callback_);

  // First mark half of the congested packets as cleared and make sure that just
  // as many are sent
  int ack_count = kCongestionCount / 2;
  EXPECT_CALL(callback_, SendPacket(ssrc, _, _, false, _)).Times(ack_count);
  send_bucket_->UpdateOutstandingData(kCongestionWindow -
                                      kPacketSize * ack_count);

  for (int duration = 0; duration < kCongestionTimeMs; duration += 5) {
    clock_.AdvanceTimeMilliseconds(5);
    send_bucket_->Process();
  }
  unacked_packets -= ack_count;
  ::testing::Mock::VerifyAndClearExpectations(&callback_);

  // Second make sure all packets are sent if sent packets are continuously
  // marked as acked.
  EXPECT_CALL(callback_, SendPacket(ssrc, _, _, false, _))
      .Times(unacked_packets);
  for (int duration = 0; duration < kCongestionTimeMs; duration += 5) {
    send_bucket_->UpdateOutstandingData(0);
    clock_.AdvanceTimeMilliseconds(5);
    send_bucket_->Process();
  }
}

TEST_P(PacedSenderTest, Pause) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint32_t ssrc_high_priority = 12347;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = clock_.TimeInMilliseconds();

  EXPECT_EQ(0, send_bucket_->QueueInMs());

  // Due to the multiplicative factor we can send 5 packets during a send
  // interval. (network capacity * multiplier / (8 bits per byte *
  // (packet size * #send intervals per second)
  const size_t packets_to_send_per_interval =
      kTargetBitrateBps * kPaceMultiplier / (8 * 250 * 200);
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), 250);
  }

  send_bucket_->Process();

  send_bucket_->Pause();

  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    Send(RtpPacketToSend::Type::kVideo, ssrc_low_priority, sequence_number++,
         capture_time_ms, 250);
    Send(RtpPacketToSend::Type::kRetransmission, ssrc, sequence_number++,
         capture_time_ms, 250);
    Send(RtpPacketToSend::Type::kAudio, ssrc_high_priority, sequence_number++,
         capture_time_ms, 250);
  }
  clock_.AdvanceTimeMilliseconds(10000);
  int64_t second_capture_time_ms = clock_.TimeInMilliseconds();
  for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
    Send(RtpPacketToSend::Type::kVideo, ssrc_low_priority, sequence_number++,
         second_capture_time_ms, 250);
    Send(RtpPacketToSend::Type::kRetransmission, ssrc, sequence_number++,
         second_capture_time_ms, 250);
    Send(RtpPacketToSend::Type::kAudio, ssrc_high_priority, sequence_number++,
         second_capture_time_ms, 250);
  }

  // Expect everything to be queued.
  EXPECT_EQ(second_capture_time_ms - capture_time_ms,
            send_bucket_->QueueInMs());

  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_CALL(callback_, SendPadding(1)).WillOnce(Return(1));
  ExpectSendPadding();
  send_bucket_->Process();

  int64_t expected_time_until_send = 500;
  EXPECT_CALL(callback_, SendPadding).Times(0);
  while (expected_time_until_send >= 5) {
    send_bucket_->Process();
    clock_.AdvanceTimeMilliseconds(5);
    expected_time_until_send -= 5;
  }

  ::testing::Mock::VerifyAndClearExpectations(&callback_);
  EXPECT_CALL(callback_, SendPadding(1)).WillOnce(Return(1));
  ExpectSendPadding();
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->Process();
  ::testing::Mock::VerifyAndClearExpectations(&callback_);

  // Expect high prio packets to come out first followed by normal
  // prio packets and low prio packets (all in capture order).
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(callback_,
                SendPacket(ssrc_high_priority, _, capture_time_ms, _, _))
        .Times(packets_to_send_per_interval);
    EXPECT_CALL(callback_,
                SendPacket(ssrc_high_priority, _, second_capture_time_ms, _, _))
        .Times(packets_to_send_per_interval);

    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      EXPECT_CALL(callback_, SendPacket(ssrc, _, capture_time_ms, _, _))
          .Times(1);
    }
    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      EXPECT_CALL(callback_, SendPacket(ssrc, _, second_capture_time_ms, _, _))
          .Times(1);
    }
    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      EXPECT_CALL(callback_,
                  SendPacket(ssrc_low_priority, _, capture_time_ms, _, _))
          .Times(1);
    }
    for (size_t i = 0; i < packets_to_send_per_interval; ++i) {
      EXPECT_CALL(callback_, SendPacket(ssrc_low_priority, _,
                                        second_capture_time_ms, _, _))
          .Times(1);
    }
  }
  send_bucket_->Resume();

  // The pacer was resumed directly after the previous process call finished. It
  // will therefore wait 5 ms until next process.
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);

  for (size_t i = 0; i < 4; i++) {
    EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
    send_bucket_->Process();
    EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
    clock_.AdvanceTimeMilliseconds(5);
  }

  EXPECT_EQ(0, send_bucket_->QueueInMs());
}

TEST_P(PacedSenderTest, ResendPacket) {
  if (GetParam() == PacerMode::kOwnPackets) {
    // This test only makes sense when re-sending is supported.
    return;
  }

  MockCallback callback;

  // Need to initialize PacedSender after we initialize clock.
  send_bucket_ = absl::make_unique<PacedSender>(&clock_, &callback, nullptr);
  Init();

  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = clock_.TimeInMilliseconds();
  EXPECT_EQ(0, send_bucket_->QueueInMs());

  send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                             sequence_number, capture_time_ms, 250, false);
  clock_.AdvanceTimeMilliseconds(1);
  send_bucket_->InsertPacket(PacedSender::kNormalPriority, ssrc,
                             sequence_number + 1, capture_time_ms + 1, 250,
                             false);
  clock_.AdvanceTimeMilliseconds(9999);
  EXPECT_EQ(clock_.TimeInMilliseconds() - capture_time_ms,
            send_bucket_->QueueInMs());
  // Fails to send first packet so only one call.
  EXPECT_CALL(callback, TimeToSendPacket(ssrc, sequence_number, capture_time_ms,
                                         false, _))
      .Times(1)
      .WillOnce(Return(RtpPacketSendResult::kTransportUnavailable));
  clock_.AdvanceTimeMilliseconds(10000);
  send_bucket_->Process();

  // Queue remains unchanged.
  EXPECT_EQ(clock_.TimeInMilliseconds() - capture_time_ms,
            send_bucket_->QueueInMs());

  // Fails to send second packet.
  EXPECT_CALL(callback, TimeToSendPacket(ssrc, sequence_number, capture_time_ms,
                                         false, _))
      .WillOnce(Return(RtpPacketSendResult::kSuccess));
  EXPECT_CALL(callback, TimeToSendPacket(ssrc, sequence_number + 1,
                                         capture_time_ms + 1, false, _))
      .WillOnce(Return(RtpPacketSendResult::kTransportUnavailable));
  clock_.AdvanceTimeMilliseconds(10000);
  send_bucket_->Process();

  // Queue is reduced by 1 packet.
  EXPECT_EQ(clock_.TimeInMilliseconds() - capture_time_ms - 1,
            send_bucket_->QueueInMs());

  // Send second packet and queue becomes empty.
  EXPECT_CALL(callback, TimeToSendPacket(ssrc, sequence_number + 1,
                                         capture_time_ms + 1, false, _))
      .WillOnce(Return(RtpPacketSendResult::kSuccess));
  clock_.AdvanceTimeMilliseconds(10000);
  send_bucket_->Process();
  EXPECT_EQ(0, send_bucket_->QueueInMs());
}

TEST_P(PacedSenderTest, ExpectedQueueTimeMs) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kNumPackets = 60;
  const size_t kPacketSize = 1200;
  const int32_t kMaxBitrate = kPaceMultiplier * 30000;
  EXPECT_EQ(0, send_bucket_->ExpectedQueueTimeMs());

  send_bucket_->SetPacingRates(30000 * kPaceMultiplier, 0);
  for (size_t i = 0; i < kNumPackets; ++i) {
    SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize);
  }

  // Queue in ms = 1000 * (bytes in queue) *8 / (bits per second)
  int64_t queue_in_ms =
      static_cast<int64_t>(1000 * kNumPackets * kPacketSize * 8 / kMaxBitrate);
  EXPECT_EQ(queue_in_ms, send_bucket_->ExpectedQueueTimeMs());

  int64_t time_start = clock_.TimeInMilliseconds();
  while (send_bucket_->QueueSizePackets() > 0) {
    int time_until_process = send_bucket_->TimeUntilNextProcess();
    if (time_until_process <= 0) {
      send_bucket_->Process();
    } else {
      clock_.AdvanceTimeMilliseconds(time_until_process);
    }
  }
  int64_t duration = clock_.TimeInMilliseconds() - time_start;

  EXPECT_EQ(0, send_bucket_->ExpectedQueueTimeMs());

  // Allow for aliasing, duration should be within one pack of max time limit.
  EXPECT_NEAR(duration, PacedSender::kMaxQueueLengthMs,
              static_cast<int64_t>(1000 * kPacketSize * 8 / kMaxBitrate));
}

TEST_P(PacedSenderTest, QueueTimeGrowsOverTime) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  EXPECT_EQ(0, send_bucket_->QueueInMs());

  send_bucket_->SetPacingRates(30000 * kPaceMultiplier, 0);
  SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number,
                      clock_.TimeInMilliseconds(), 1200);

  clock_.AdvanceTimeMilliseconds(500);
  EXPECT_EQ(500, send_bucket_->QueueInMs());
  send_bucket_->Process();
  EXPECT_EQ(0, send_bucket_->QueueInMs());
}

TEST_P(PacedSenderTest, ProbingWithInsertedPackets) {
  const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;

  PacedSenderProbing packet_sender;
  send_bucket_.reset(new PacedSender(&clock_, &packet_sender, nullptr));
  send_bucket_->CreateProbeCluster(kFirstClusterBps, /*cluster_id=*/0);
  send_bucket_->CreateProbeCluster(kSecondClusterBps, /*cluster_id=*/1);
  send_bucket_->SetPacingRates(kInitialBitrateBps * kPaceMultiplier, 0);

  for (int i = 0; i < 10; ++i) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), kPacketSize);
  }

  int64_t start = clock_.TimeInMilliseconds();
  while (packet_sender.packets_sent() < 5) {
    int time_until_process = send_bucket_->TimeUntilNextProcess();
    clock_.AdvanceTimeMilliseconds(time_until_process);
    send_bucket_->Process();
  }
  int packets_sent = packet_sender.packets_sent();
  // Validate first cluster bitrate. Note that we have to account for number
  // of intervals and hence (packets_sent - 1) on the first cluster.
  EXPECT_NEAR((packets_sent - 1) * kPacketSize * 8000 /
                  (clock_.TimeInMilliseconds() - start),
              kFirstClusterBps, kBitrateProbingError);
  EXPECT_EQ(0, packet_sender.padding_sent());

  clock_.AdvanceTimeMilliseconds(send_bucket_->TimeUntilNextProcess());
  start = clock_.TimeInMilliseconds();
  while (packet_sender.packets_sent() < 10) {
    int time_until_process = send_bucket_->TimeUntilNextProcess();
    clock_.AdvanceTimeMilliseconds(time_until_process);
    send_bucket_->Process();
  }
  packets_sent = packet_sender.packets_sent() - packets_sent;
  // Validate second cluster bitrate.
  EXPECT_NEAR((packets_sent - 1) * kPacketSize * 8000 /
                  (clock_.TimeInMilliseconds() - start),
              kSecondClusterBps, kBitrateProbingError);
}

TEST_P(PacedSenderTest, ProbingWithPaddingSupport) {
  const size_t kPacketSize = 1200;
  const int kInitialBitrateBps = 300000;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;

  PacedSenderProbing packet_sender;
  send_bucket_.reset(new PacedSender(&clock_, &packet_sender, nullptr));
  send_bucket_->CreateProbeCluster(kFirstClusterBps, /*cluster_id=*/0);
  send_bucket_->SetPacingRates(kInitialBitrateBps * kPaceMultiplier, 0);

  for (int i = 0; i < 3; ++i) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), kPacketSize);
  }

  int64_t start = clock_.TimeInMilliseconds();
  int process_count = 0;
  while (process_count < 5) {
    int time_until_process = send_bucket_->TimeUntilNextProcess();
    clock_.AdvanceTimeMilliseconds(time_until_process);
    send_bucket_->Process();
    ++process_count;
  }
  int packets_sent = packet_sender.packets_sent();
  int padding_sent = packet_sender.padding_sent();
  EXPECT_GT(packets_sent, 0);
  EXPECT_GT(padding_sent, 0);
  // Note that the number of intervals here for kPacketSize is
  // packets_sent due to padding in the same cluster.
  EXPECT_NEAR((packets_sent * kPacketSize * 8000 + padding_sent) /
                  (clock_.TimeInMilliseconds() - start),
              kFirstClusterBps, kBitrateProbingError);
}

TEST_P(PacedSenderTest, PaddingOveruse) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 1200;

  send_bucket_->Process();
  send_bucket_->SetPacingRates(60000 * kPaceMultiplier, 0);

  SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                      clock_.TimeInMilliseconds(), kPacketSize);
  send_bucket_->Process();

  // Add 30kbit padding. When increasing budget, media budget will increase from
  // negative (overuse) while padding budget will increase from 0.
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->SetPacingRates(60000 * kPaceMultiplier, 30000);

  SendAndExpectPacket(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
                      clock_.TimeInMilliseconds(), kPacketSize);
  EXPECT_LT(5u, send_bucket_->ExpectedQueueTimeMs());
  // Don't send padding if queue is non-empty, even if padding budget > 0.
  EXPECT_CALL(callback_, SendPadding).Times(0);
  send_bucket_->Process();
}

TEST_P(PacedSenderTest, ProbeClusterId) {
  MockCallback callback;

  send_bucket_ = absl::make_unique<PacedSender>(&clock_, &callback, nullptr);
  Init();

  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = 1200;

  send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier,
                               kTargetBitrateBps);
  send_bucket_->SetProbingEnabled(true);
  for (int i = 0; i < 10; ++i) {
    Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number++,
         clock_.TimeInMilliseconds(), kPacketSize);
  }

  // First probing cluster.
  if (GetParam() == PacerMode::kReferencePackets) {
    EXPECT_CALL(callback,
                TimeToSendPacket(_, _, _, _,
                                 Field(&PacedPacketInfo::probe_cluster_id, 0)))
        .Times(5)
        .WillRepeatedly(Return(RtpPacketSendResult::kSuccess));
  } else {
    EXPECT_CALL(callback,
                SendPacket(_, Field(&PacedPacketInfo::probe_cluster_id, 0)))
        .Times(5);
  }

  for (int i = 0; i < 5; ++i) {
    clock_.AdvanceTimeMilliseconds(20);
    send_bucket_->Process();
  }

  // Second probing cluster.
  if (GetParam() == PacerMode::kReferencePackets) {
    EXPECT_CALL(callback,
                TimeToSendPacket(_, _, _, _,
                                 Field(&PacedPacketInfo::probe_cluster_id, 1)))
        .Times(5)
        .WillRepeatedly(Return(RtpPacketSendResult::kSuccess));
  } else {
    EXPECT_CALL(callback,
                SendPacket(_, Field(&PacedPacketInfo::probe_cluster_id, 1)))
        .Times(5);
  }

  for (int i = 0; i < 5; ++i) {
    clock_.AdvanceTimeMilliseconds(20);
    send_bucket_->Process();
  }

  // Needed for the Field comparer below.
  const int kNotAProbe = PacedPacketInfo::kNotAProbe;
  // No more probing packets.
  if (GetParam() == PacerMode::kReferencePackets) {
    EXPECT_CALL(callback,
                TimeToSendPadding(
                    _, Field(&PacedPacketInfo::probe_cluster_id, kNotAProbe)))
        .WillOnce(Return(500));
  } else {
    EXPECT_CALL(callback, GeneratePadding).WillOnce([&](size_t padding_bytes) {
      std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets;
      padding_packets.emplace_back(
          BuildPacket(RtpPacketToSend::Type::kPadding, ssrc, sequence_number++,
                      clock_.TimeInMilliseconds(), padding_bytes));
      return padding_packets;
    });
    EXPECT_CALL(
        callback,
        SendPacket(_, Field(&PacedPacketInfo::probe_cluster_id, kNotAProbe)))
        .Times(1);
  }
  send_bucket_->Process();
}

TEST_P(PacedSenderTest, AvoidBusyLoopOnSendFailure) {
  if (GetParam() != PacerMode::kReferencePackets) {
    // This test only makes sense when send failure is supported.
    return;
  }

  MockCallback callback;

  send_bucket_ = absl::make_unique<PacedSender>(&clock_, &callback, nullptr);
  Init();

  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const size_t kPacketSize = kFirstClusterBps / (8000 / 10);

  send_bucket_->SetPacingRates(kTargetBitrateBps * kPaceMultiplier,
                               kTargetBitrateBps);
  send_bucket_->SetProbingEnabled(true);
  Send(RtpPacketToSend::Type::kVideo, ssrc, sequence_number,
       clock_.TimeInMilliseconds(), kPacketSize);

  EXPECT_CALL(callback, TimeToSendPacket)
      .WillOnce(Return(RtpPacketSendResult::kSuccess));
  send_bucket_->Process();
  EXPECT_EQ(10, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(9);

  EXPECT_CALL(callback, TimeToSendPadding).Times(2).WillRepeatedly(Return(0));
  send_bucket_->Process();
  EXPECT_EQ(1, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(1);
  send_bucket_->Process();
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
}

TEST_P(PacedSenderTest, OwnedPacketPrioritizedOnType) {
  if (GetParam() != PacerMode::kOwnPackets) {
    // This test only makes sense when using the new code path.
    return;
  }

  MockCallback callback;
  send_bucket_ = absl::make_unique<PacedSender>(&clock_, &callback, nullptr);
  Init();

  // Insert a packet of each type, from low to high priority. Since priority
  // is weighted higher than insert order, these should come out of the pacer
  // in backwards order with the exception of FEC and Video.
  for (RtpPacketToSend::Type type :
       {RtpPacketToSend::Type::kPadding,
        RtpPacketToSend::Type::kForwardErrorCorrection,
        RtpPacketToSend::Type::kVideo, RtpPacketToSend::Type::kRetransmission,
        RtpPacketToSend::Type::kAudio}) {
    send_bucket_->EnqueuePacket(BuildRtpPacket(type));
  }

  ::testing::InSequence seq;
  EXPECT_CALL(
      callback,
      SendPacket(Pointee(Property(&RtpPacketToSend::Ssrc, kAudioSsrc)), _));
  EXPECT_CALL(
      callback,
      SendPacket(Pointee(Property(&RtpPacketToSend::Ssrc, kVideoRtxSsrc)), _));

  // FEC and video actually have the same priority, so will come out in
  // insertion order.
  EXPECT_CALL(
      callback,
      SendPacket(Pointee(Property(&RtpPacketToSend::Ssrc, kFlexFecSsrc)), _));
  EXPECT_CALL(
      callback,
      SendPacket(Pointee(Property(&RtpPacketToSend::Ssrc, kVideoSsrc)), _));

  EXPECT_CALL(
      callback,
      SendPacket(Pointee(Property(&RtpPacketToSend::Ssrc, kVideoRtxSsrc)), _));

  clock_.AdvanceTimeMilliseconds(200);
  send_bucket_->Process();
}

INSTANTIATE_TEST_SUITE_P(ReferencingAndOwningPackets,
                         PacedSenderTest,
                         ::testing::Values(PacerMode::kReferencePackets,
                                           PacerMode::kOwnPackets));

}  // namespace test
}  // namespace webrtc
