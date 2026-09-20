/* Copyright (c) 2026 The WebRTC project authors. */
#include "modules/roq/bounded_datagram_queue.h"
#include "test/gtest.h"

namespace webrtc::roq {
namespace {
QueuedDatagram VideoPacket(uint8_t value, Timestamp now) {
  return {.bytes = {value},
          .binding_id = "video",
          .flow_id = 1,
          .enqueue_time = now,
          .deadline = now + TimeDelta::Millis(100),
          .priority = DatagramPriority::kVideoDelta,
          .frame_id = DatagramFrameId{7, 90000, "video"},
          .packet_id = DatagramPacketId{7, value}};
}
TEST(PacketForwardingTest, IncompleteFrameDoesNotBlockOrExpirePacket) {
  const auto now = Timestamp::Millis(100);
  BoundedDatagramQueue q(4, 1024);
  ASSERT_EQ(q.Enqueue(VideoPacket(100, now), now),
            DatagramEnqueueResult::kQueued);
  auto packet = q.Pop(now + TimeDelta::Seconds(2));
  ASSERT_TRUE(packet);
  EXPECT_EQ(packet->bytes, (std::vector<uint8_t>{100}));
}
TEST(PacketForwardingTest, RTXDoesNotRequireOriginalTransportSubmission) {
  const auto now = Timestamp::Millis(100);
  BoundedDatagramQueue q(4, 1024);
  auto rtx = VideoPacket(101, now);
  rtx.original_packet_id = DatagramPacketId{7, 100};
  ASSERT_EQ(q.Enqueue(std::move(rtx), now), DatagramEnqueueResult::kQueued);
  EXPECT_TRUE(q.Pop(now + TimeDelta::Seconds(2)));
}
TEST(PacketForwardingTest, FullQueueRejectsEqualPriorityIncomingPacket) {
  const auto now = Timestamp::Millis(100);
  BoundedDatagramQueue q(1, 1024);
  auto first = VideoPacket(100, now);
  first.frame_complete = true;
  ASSERT_EQ(q.Enqueue(std::move(first), now), DatagramEnqueueResult::kQueued);
  QueuedDatagram video{.bytes = {9},
                       .binding_id = "other-video",
                       .priority = DatagramPriority::kVideoDelta};
  EXPECT_EQ(q.Enqueue(std::move(video), now),
            DatagramEnqueueResult::kQueueFull);
  auto packet = q.Pop(now);
  ASSERT_TRUE(packet);
  EXPECT_EQ(packet->bytes, (std::vector<uint8_t>{100}));
  ASSERT_EQ(q.Enqueue(VideoPacket(102, now), now),
            DatagramEnqueueResult::kQueued);
  EXPECT_TRUE(q.Pop(now));
}
TEST(PacketForwardingTest, RetryPreservesOrderAndCapacity) {
  const auto now = Timestamp::Millis(100);
  BoundedDatagramQueue q(2, 1024, 2);
  ASSERT_EQ(q.Enqueue(VideoPacket(100, now), now),
            DatagramEnqueueResult::kQueued);
  auto second = VideoPacket(101, now);
  second.priority = DatagramPriority::kVideoKey;
  ASSERT_EQ(q.Enqueue(std::move(second), now), DatagramEnqueueResult::kQueued);
  auto first = q.Pop(now);
  ASSERT_TRUE(first);
  EXPECT_EQ(first->bytes, (std::vector<uint8_t>{100}));
  q.Requeue(std::move(*first));
  EXPECT_EQ(q.Enqueue(VideoPacket(102, now), now),
            DatagramEnqueueResult::kQueueFull);
  EXPECT_EQ(q.byte_size(), 2u);
  EXPECT_EQ(q.Pop(now)->bytes, (std::vector<uint8_t>{100}));
  EXPECT_EQ(q.Pop(now)->bytes, (std::vector<uint8_t>{101}));
}
TEST(PacketForwardingTest, FairnessNeverSkipsLargeHeadPacket) {
  const auto now = Timestamp::Millis(100);
  BoundedDatagramQueue q(4, 1350, 4096);
  for (auto size : {1000, 1000, 100}) {
    auto p = VideoPacket(1, now);
    p.bytes.resize(size);
    ASSERT_EQ(q.Enqueue(std::move(p), now), DatagramEnqueueResult::kQueued);
  }
  EXPECT_EQ(q.Pop(now)->bytes.size(), 1000u);
  EXPECT_EQ(q.Pop(now)->bytes.size(), 1000u);
  EXPECT_EQ(q.Pop(now)->bytes.size(), 100u);
}
}  // namespace
}  // namespace webrtc::roq
