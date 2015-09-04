/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <limits>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/remote_bitrate_estimator/include/send_time_history.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {
namespace test {

static const int kDefaultHistoryLengthMs = 1000;

class SendTimeHistoryTest : public ::testing::Test {
 protected:
  SendTimeHistoryTest() : history_(kDefaultHistoryLengthMs), clock_(0) {}
  ~SendTimeHistoryTest() {}

  virtual void SetUp() {}

  virtual void TearDown() {}

  SendTimeHistory history_;
  webrtc::SimulatedClock clock_;
};

// Help class extended so we can do EXPECT_EQ and collections.
class PacketInfo : public webrtc::PacketInfo {
 public:
  PacketInfo() : webrtc::PacketInfo(0, 0, 0, 0, false) {}
  PacketInfo(int64_t arrival_time_ms,
             int64_t send_time_ms,
             uint16_t sequence_number,
             size_t payload_size,
             bool was_paced)
      : webrtc::PacketInfo(arrival_time_ms,
                           send_time_ms,
                           sequence_number,
                           payload_size,
                           was_paced) {}
  bool operator==(const PacketInfo& other) const {
    return arrival_time_ms == other.arrival_time_ms &&
           send_time_ms == other.send_time_ms &&
           sequence_number == other.sequence_number &&
           payload_size == other.payload_size && was_paced == other.was_paced;
  }
};

TEST_F(SendTimeHistoryTest, AddRemoveOne) {
  const uint16_t kSeqNo = 10;
  const PacketInfo kSentPacket = {0, 1, kSeqNo, 1, true};
  history_.AddAndRemoveOld(kSentPacket);

  PacketInfo received_packet = {0, 0, kSeqNo, 0, false};
  EXPECT_TRUE(history_.GetInfo(&received_packet, false));
  EXPECT_EQ(kSentPacket, received_packet);

  received_packet = {0, 0, kSeqNo, 0, false};
  EXPECT_TRUE(history_.GetInfo(&received_packet, true));
  EXPECT_EQ(kSentPacket, received_packet);

  received_packet = {0, 0, kSeqNo, 0, false};
  EXPECT_FALSE(history_.GetInfo(&received_packet, true));
}

TEST_F(SendTimeHistoryTest, UpdateSendTime) {
  const uint16_t kSeqNo = 10;
  const int64_t kSendTime = 1000;
  const int64_t kSendTimeUpdated = 2000;
  const PacketInfo kSentPacket = {0, kSendTime, kSeqNo, 1, true};
  const PacketInfo kUpdatedPacket = {0, kSendTimeUpdated, kSeqNo, 1, true};

  history_.AddAndRemoveOld(kSentPacket);
  PacketInfo info = {0, 0, kSeqNo, 0, false};
  EXPECT_TRUE(history_.GetInfo(&info, false));
  EXPECT_EQ(kSentPacket, info);

  EXPECT_TRUE(history_.UpdateSendTime(kSeqNo, kSendTimeUpdated));

  info = {0, 0, kSeqNo, 0, false};
  EXPECT_TRUE(history_.GetInfo(&info, true));
  EXPECT_EQ(kUpdatedPacket, info);

  EXPECT_FALSE(history_.UpdateSendTime(kSeqNo, kSendTimeUpdated));
}

TEST_F(SendTimeHistoryTest, PopulatesExpectedFields) {
  const uint16_t kSeqNo = 10;
  const int64_t kSendTime = 1000;
  const int64_t kReceiveTime = 2000;
  const size_t kPayloadSize = 42;
  const bool kPaced = true;
  const PacketInfo kSentPacket = {0, kSendTime, kSeqNo, kPayloadSize, kPaced};

  history_.AddAndRemoveOld(kSentPacket);

  PacketInfo info = {kReceiveTime, 0, kSeqNo, 0, false};
  EXPECT_TRUE(history_.GetInfo(&info, true));
  EXPECT_EQ(kReceiveTime, info.arrival_time_ms);
  EXPECT_EQ(kSendTime, info.send_time_ms);
  EXPECT_EQ(kSeqNo, info.sequence_number);
  EXPECT_EQ(kPayloadSize, info.payload_size);
  EXPECT_EQ(kPaced, info.was_paced);
}

TEST_F(SendTimeHistoryTest, AddThenRemoveOutOfOrder) {
  std::vector<PacketInfo> sent_packets;
  std::vector<PacketInfo> received_packets;
  const size_t num_items = 100;
  const size_t kPacketSize = 400;
  const size_t kTransmissionTime = 1234;
  const bool kPaced = true;
  for (size_t i = 0; i < num_items; ++i) {
    sent_packets.push_back(PacketInfo(0, static_cast<int64_t>(i),
                                      static_cast<uint16_t>(i), kPacketSize,
                                      kPaced));
    received_packets.push_back(
        PacketInfo(static_cast<int64_t>(i) + kTransmissionTime, 0,
                   static_cast<uint16_t>(i), kPacketSize, false));
  }
  for (size_t i = 0; i < num_items; ++i)
    history_.AddAndRemoveOld(sent_packets[i]);
  std::random_shuffle(received_packets.begin(), received_packets.end());
  for (size_t i = 0; i < num_items; ++i) {
    PacketInfo packet = received_packets[i];
    EXPECT_TRUE(history_.GetInfo(&packet, false));
    PacketInfo sent_packet = sent_packets[packet.sequence_number];
    sent_packet.arrival_time_ms = packet.arrival_time_ms;
    EXPECT_EQ(sent_packet, packet);
    EXPECT_TRUE(history_.GetInfo(&packet, true));
  }
  for (PacketInfo packet : sent_packets)
    EXPECT_FALSE(history_.GetInfo(&packet, false));
}

TEST_F(SendTimeHistoryTest, HistorySize) {
  const int kItems = kDefaultHistoryLengthMs / 100;
  for (int i = 0; i < kItems; ++i)
    history_.AddAndRemoveOld(PacketInfo(0, i * 100, i, 0, false));
  for (int i = 0; i < kItems; ++i) {
    PacketInfo info = {0, 0, static_cast<uint16_t>(i), 0, false};
    EXPECT_TRUE(history_.GetInfo(&info, false));
    EXPECT_EQ(i * 100, info.send_time_ms);
  }
  history_.AddAndRemoveOld(PacketInfo(0, kItems * 100, kItems, 0, false));
  PacketInfo info = {0, 0, 0, 0, false};
  EXPECT_FALSE(history_.GetInfo(&info, false));
  for (int i = 1; i < (kItems + 1); ++i) {
    info = {0, 0, static_cast<uint16_t>(i), 0, false};
    EXPECT_TRUE(history_.GetInfo(&info, false));
    EXPECT_EQ(i * 100, info.send_time_ms);
  }
}

TEST_F(SendTimeHistoryTest, HistorySizeWithWraparound) {
  const uint16_t kMaxSeqNo = std::numeric_limits<uint16_t>::max();
  history_.AddAndRemoveOld(PacketInfo(0, 0, kMaxSeqNo - 2, 0, false));
  history_.AddAndRemoveOld(PacketInfo(0, 100, kMaxSeqNo - 1, 0, false));
  history_.AddAndRemoveOld(PacketInfo(0, 200, kMaxSeqNo, 0, false));
  history_.AddAndRemoveOld(PacketInfo(0, kDefaultHistoryLengthMs, 0, 0, false));
  PacketInfo info = {0, 0, static_cast<uint16_t>(kMaxSeqNo - 2), 0, false};
  EXPECT_FALSE(history_.GetInfo(&info, false));
  info = {0, 0, static_cast<uint16_t>(kMaxSeqNo - 1), 0, false};
  EXPECT_TRUE(history_.GetInfo(&info, false));
  info = {0, 0, static_cast<uint16_t>(kMaxSeqNo), 0, false};
  EXPECT_TRUE(history_.GetInfo(&info, false));
  info = {0, 0, 0, 0, false};
  EXPECT_TRUE(history_.GetInfo(&info, false));

  // Create a gap (kMaxSeqNo - 1) -> 0.
  info = {0, 0, kMaxSeqNo, 0, false};
  EXPECT_TRUE(history_.GetInfo(&info, true));

  history_.AddAndRemoveOld(PacketInfo(0, 1100, 1, 0, false));

  info = {0, 0, static_cast<uint16_t>(kMaxSeqNo - 2), 0, false};
  EXPECT_FALSE(history_.GetInfo(&info, false));
  info = {0, 0, static_cast<uint16_t>(kMaxSeqNo - 1), 0, false};
  EXPECT_FALSE(history_.GetInfo(&info, false));
  info = {0, 0, kMaxSeqNo, 0, false};
  EXPECT_FALSE(history_.GetInfo(&info, false));
  info = {0, 0, 0, 0, false};
  EXPECT_TRUE(history_.GetInfo(&info, false));
  info = {0, 0, 1, 0, false};
  EXPECT_TRUE(history_.GetInfo(&info, false));
}

TEST_F(SendTimeHistoryTest, InterlievedGetAndRemove) {
  const uint16_t kSeqNo = 1;
  const int64_t kTimestamp = 2;
  PacketInfo packets[3] = {{0, kTimestamp, kSeqNo, 0, false},
                           {0, kTimestamp + 1, kSeqNo + 1, 0, false},
                           {0, kTimestamp + 2, kSeqNo + 2, 0, false}};

  history_.AddAndRemoveOld(packets[0]);
  history_.AddAndRemoveOld(packets[1]);

  PacketInfo info = {0, 0, packets[0].sequence_number, 0, false};
  EXPECT_TRUE(history_.GetInfo(&info, true));
  EXPECT_EQ(packets[0], info);

  history_.AddAndRemoveOld(packets[2]);

  info = {0, 0, packets[1].sequence_number, 0, false};
  EXPECT_TRUE(history_.GetInfo(&info, true));
  EXPECT_EQ(packets[1], info);

  info = {0, 0, packets[2].sequence_number, 0, false};
  EXPECT_TRUE(history_.GetInfo(&info, true));
  EXPECT_EQ(packets[2], info);
}

}  // namespace test
}  // namespace webrtc
