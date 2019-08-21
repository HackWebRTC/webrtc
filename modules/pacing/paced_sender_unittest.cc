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
using ::testing::Return;

namespace {
constexpr uint32_t kAudioSsrc = 12345;
constexpr uint32_t kVideoSsrc = 234565;
constexpr uint32_t kVideoRtxSsrc = 34567;
constexpr uint32_t kFlexFecSsrc = 45678;
constexpr size_t kDefaultPacketSize = 234;
}  // namespace

namespace webrtc {
namespace test {


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

  packet->SetPayloadSize(kDefaultPacketSize);
  return packet;
}

TEST(PacedSenderTest, PacesPackets) {
  SimulatedClock clock(0);
  MockCallback callback;
  PacedSender pacer(&clock, &callback, nullptr, nullptr);

  // Insert a number of packets, covering one second.
  static constexpr size_t kPacketsToSend = 42;
  pacer.SetPacingRates(DataRate::bps(kDefaultPacketSize * 8 * kPacketsToSend),
                       DataRate::Zero());
  for (size_t i = 0; i < kPacketsToSend; ++i) {
    pacer.EnqueuePacket(BuildRtpPacket(RtpPacketToSend::Type::kVideo));
  }

  // Expect all of them to be sent.
  size_t packets_sent = 0;
  clock.AdvanceTimeMilliseconds(pacer.TimeUntilNextProcess());
  EXPECT_CALL(callback, SendPacket)
      .WillRepeatedly(
          [&](std::unique_ptr<RtpPacketToSend> packet,
              const PacedPacketInfo& cluster_info) { ++packets_sent; });

  const Timestamp start_time = clock.CurrentTime();

  while (packets_sent < kPacketsToSend) {
    clock.AdvanceTimeMilliseconds(pacer.TimeUntilNextProcess());
    pacer.Process();
  }

  // Packets should be sent over a period of close to 1s. Expect a little lower
  // than this since initial probing is a bit quicker.
  TimeDelta duration = clock.CurrentTime() - start_time;
  EXPECT_GT(duration, TimeDelta::ms(900));
}

}  // namespace test
}  // namespace webrtc
