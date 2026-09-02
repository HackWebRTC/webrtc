/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/bounded_datagram_queue.h"

#include <cstdint>
#include <vector>

#include "test/gtest.h"

namespace webrtc::roq {
namespace {

TEST(BoundedDatagramQueueTest, RejectsNewestPacketWhenCapacityIsFull) {
  BoundedDatagramQueue queue(/*capacity=*/2, /*max_datagram_size=*/8);

  EXPECT_EQ(queue.Enqueue({1, 2}), DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue({3, 4, 5}), DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue({6}), DatagramEnqueueResult::kQueueFull);
  ASSERT_TRUE(queue.Pop().has_value());
  EXPECT_EQ(queue.Pop()->bytes, (std::vector<uint8_t>{3, 4, 5}));
  EXPECT_EQ(queue.queue_full_drops(), 1u);
  EXPECT_EQ(queue.peak_size(), 2u);
}

TEST(BoundedDatagramQueueTest, RejectsOversizeWithoutBlockingFollowingPacket) {
  BoundedDatagramQueue queue(/*capacity=*/2, /*max_datagram_size=*/4);

  EXPECT_EQ(queue.Enqueue({1, 2, 3, 4, 5}), DatagramEnqueueResult::kOversize);
  EXPECT_EQ(queue.Enqueue({9, 8, 7}), DatagramEnqueueResult::kQueued);
  ASSERT_TRUE(queue.Pop().has_value());
  EXPECT_EQ(queue.Pop(), std::nullopt);
  EXPECT_EQ(queue.oversize_drops(), 1u);
}

TEST(BoundedDatagramQueueTest, PmtuShrinkPurgesQueuedOversizePackets) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  EXPECT_EQ(queue.Enqueue({1, 2, 3}), DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue({4, 5, 6, 7, 8}), DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue({9, 10, 11, 12}), DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.SetMaxDatagramSize(4), 1u);
  ASSERT_TRUE(queue.Pop().has_value());
  EXPECT_EQ(queue.Pop()->bytes, (std::vector<uint8_t>{9, 10, 11, 12}));
  EXPECT_EQ(queue.pmtu_purge_drops(), 1u);
}

}  // namespace
}  // namespace webrtc::roq
