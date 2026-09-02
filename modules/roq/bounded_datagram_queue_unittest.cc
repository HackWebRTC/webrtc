/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/bounded_datagram_queue.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc::roq {
namespace {

QueuedDatagram Datagram(
    uint8_t value,
    DatagramPriority priority,
    Timestamp enqueue_time,
    TimeDelta lifetime,
    std::optional<DatagramFrameId> frame_id = std::nullopt) {
  return {.bytes = {value},
          .enqueue_time = enqueue_time,
          .deadline = enqueue_time + lifetime,
          .priority = priority,
          .frame_id = frame_id};
}

TEST(BoundedDatagramQueueTest, LegacyQueueRejectsNewestPacketWhenFull) {
  BoundedDatagramQueue queue(/*capacity=*/2, /*max_datagram_size=*/8);

  EXPECT_EQ(queue.Enqueue({1, 2}), DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue({3, 4, 5}), DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue({6}), DatagramEnqueueResult::kQueueFull);
  std::optional<QueuedDatagram> first = queue.Pop();
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->bytes, (std::vector<uint8_t>{1, 2}));
  EXPECT_EQ(queue.queue_full_drops(), 1u);
  EXPECT_EQ(queue.peak_size(), 2u);
}

TEST(BoundedDatagramQueueTest, KeepsNewestPacketWhenCapacityIsFull) {
  BoundedDatagramQueue queue(/*capacity=*/2, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);

  EXPECT_EQ(queue.Enqueue(Datagram(1, DatagramPriority::kVideoDelta, now,
                                   TimeDelta::Seconds(1)),
                          now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(Datagram(2, DatagramPriority::kVideoDelta, now,
                                   TimeDelta::Seconds(1)),
                          now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(Datagram(3, DatagramPriority::kVideoDelta, now,
                                   TimeDelta::Seconds(1)),
                          now),
            DatagramEnqueueResult::kQueued);

  ASSERT_TRUE(queue.Pop().has_value());
  EXPECT_EQ(queue.Pop()->bytes, (std::vector<uint8_t>{3}));
}

TEST(BoundedDatagramQueueTest, PopsHighestPriorityThenOldestPacket) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);

  EXPECT_EQ(queue.Enqueue(Datagram(1, DatagramPriority::kVideoDelta, start,
                                   TimeDelta::Seconds(1)),
                          start),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(
                Datagram(2, DatagramPriority::kAudio,
                         start + TimeDelta::Millis(1), TimeDelta::Seconds(1)),
                start),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(
                Datagram(3, DatagramPriority::kRtcp,
                         start + TimeDelta::Millis(2), TimeDelta::Seconds(1)),
                start),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(
                Datagram(4, DatagramPriority::kVideoKey,
                         start + TimeDelta::Millis(3), TimeDelta::Seconds(1)),
                start),
            DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.Pop(start)->bytes, (std::vector<uint8_t>{3}));
  EXPECT_EQ(queue.Pop(start)->bytes, (std::vector<uint8_t>{2}));
  EXPECT_EQ(queue.Pop(start)->bytes, (std::vector<uint8_t>{4}));
  EXPECT_EQ(queue.Pop(start)->bytes, (std::vector<uint8_t>{1}));
}

TEST(BoundedDatagramQueueTest, RecordsMaximumQueueDelay) {
  BoundedDatagramQueue queue(/*capacity=*/1, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  EXPECT_EQ(queue.Enqueue(Datagram(1, DatagramPriority::kAudio, start,
                                   TimeDelta::Seconds(1)),
                          start),
            DatagramEnqueueResult::kQueued);

  ASSERT_TRUE(queue.Pop(start + TimeDelta::Millis(25)).has_value());
  EXPECT_EQ(queue.max_queue_delay(), TimeDelta::Millis(25));
}

TEST(BoundedDatagramQueueTest, RejectsLowerPriorityPacketWhenFull) {
  BoundedDatagramQueue queue(/*capacity=*/1, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  EXPECT_EQ(queue.Enqueue(Datagram(1, DatagramPriority::kAudio, now,
                                   TimeDelta::Seconds(1)),
                          now),
            DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.Enqueue(Datagram(2, DatagramPriority::kVideoDelta, now,
                                   TimeDelta::Seconds(1)),
                          now),
            DatagramEnqueueResult::kQueueFull);
  EXPECT_EQ(queue.Pop(now)->bytes, (std::vector<uint8_t>{1}));
  EXPECT_EQ(queue.queue_full_drops(), 1u);
}

TEST(BoundedDatagramQueueTest, EvictsWholeOldVideoFrameForAudio) {
  BoundedDatagramQueue queue(/*capacity=*/2, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {.ssrc = 7, .timestamp = 90000};
  EXPECT_EQ(queue.Enqueue(Datagram(1, DatagramPriority::kVideoDelta, now,
                                   TimeDelta::Millis(100), frame),
                          now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(Datagram(2, DatagramPriority::kVideoDelta, now,
                                   TimeDelta::Millis(100), frame),
                          now),
            DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.Enqueue(Datagram(3, DatagramPriority::kAudio, now,
                                   TimeDelta::Millis(60)),
                          now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.size(), 1u);
  EXPECT_EQ(queue.Pop(now)->bytes, (std::vector<uint8_t>{3}));
  EXPECT_EQ(queue.queue_full_drops(), 2u);
  EXPECT_EQ(queue.dropped_by_priority(DatagramPriority::kVideoDelta), 2u);
}

TEST(BoundedDatagramQueueTest, PurgesWholeFrameWhenOnePacketExpires) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {.ssrc = 7, .timestamp = 90000};
  EXPECT_EQ(queue.Enqueue(Datagram(1, DatagramPriority::kVideoDelta, start,
                                   TimeDelta::Millis(10), frame),
                          start),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(Datagram(2, DatagramPriority::kVideoDelta, start,
                                   TimeDelta::Millis(20), frame),
                          start),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(Datagram(3, DatagramPriority::kAudio, start,
                                   TimeDelta::Millis(60)),
                          start),
            DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.PurgeExpired(start + TimeDelta::Millis(11)), 2u);
  EXPECT_EQ(queue.expired_drops(), 2u);
  EXPECT_EQ(queue.Pop(start + TimeDelta::Millis(11))->bytes,
            (std::vector<uint8_t>{3}));
}

TEST(BoundedDatagramQueueTest, PromotesWholeFrameWhenKeyPacketArrives) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {.ssrc = 7, .timestamp = 90000};
  QueuedDatagram initial = Datagram(1, DatagramPriority::kVideoDelta, start,
                                    TimeDelta::Millis(100), frame);
  initial.packet_id = DatagramPacketId{.ssrc = 7, .sequence_number = 9};
  EXPECT_EQ(queue.Enqueue(std::move(initial), start),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(Datagram(2, DatagramPriority::kVideoKey,
                                   start + TimeDelta::Millis(10),
                                   TimeDelta::Millis(150), frame),
                          start + TimeDelta::Millis(10)),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(
                Datagram(3, DatagramPriority::kVideoDelta,
                         start + TimeDelta::Millis(20), TimeDelta::Millis(100)),
                start + TimeDelta::Millis(20)),
            DatagramEnqueueResult::kQueued);

  std::optional<QueuedDatagram> first = queue.Pop(start);
  std::optional<QueuedDatagram> second = queue.Pop(start);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(first->bytes, (std::vector<uint8_t>{1}));
  EXPECT_EQ(second->bytes, (std::vector<uint8_t>{2}));
  EXPECT_EQ(first->priority, DatagramPriority::kVideoKey);
  EXPECT_EQ(first->deadline, start + TimeDelta::Millis(150));
  EXPECT_EQ(second->deadline, start + TimeDelta::Millis(150));
}

TEST(BoundedDatagramQueueTest, KeepsKeyFramePolicyForLaterPackets) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {.ssrc = 7, .timestamp = 90000};
  EXPECT_EQ(queue.Enqueue(Datagram(1, DatagramPriority::kVideoKey, start,
                                   TimeDelta::Millis(150), frame),
                          start),
            DatagramEnqueueResult::kQueued);
  std::optional<QueuedDatagram> submitted = queue.Pop(start);
  ASSERT_TRUE(submitted.has_value());
  queue.RecordSubmitted(*submitted);
  EXPECT_EQ(queue.Enqueue(Datagram(2, DatagramPriority::kVideoDelta,
                                   start + TimeDelta::Millis(20),
                                   TimeDelta::Millis(100), frame),
                          start + TimeDelta::Millis(20)),
            DatagramEnqueueResult::kQueued);

  std::optional<QueuedDatagram> queued = queue.Pop(start);
  ASSERT_TRUE(queued.has_value());
  EXPECT_EQ(queued->priority, DatagramPriority::kVideoKey);
  EXPECT_EQ(queued->deadline, start + TimeDelta::Millis(150));
}

TEST(BoundedDatagramQueueTest, PromotesFrameAfterEarlierPacketWasSubmitted) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {.ssrc = 7, .timestamp = 90000};
  QueuedDatagram primary = Datagram(1, DatagramPriority::kVideoDelta, start,
                                    TimeDelta::Millis(100), frame);
  primary.packet_id = DatagramPacketId{.ssrc = 7, .sequence_number = 9};
  EXPECT_EQ(queue.Enqueue(std::move(primary), start),
            DatagramEnqueueResult::kQueued);
  std::optional<QueuedDatagram> submitted = queue.Pop(start);
  ASSERT_TRUE(submitted.has_value());
  queue.RecordSubmitted(*submitted);

  EXPECT_EQ(queue.Enqueue(Datagram(2, DatagramPriority::kVideoKey,
                                   start + TimeDelta::Millis(10),
                                   TimeDelta::Millis(150), frame),
                          start + TimeDelta::Millis(10)),
            DatagramEnqueueResult::kQueued);
  ASSERT_TRUE(queue.front());
  EXPECT_EQ(queue.front()->deadline, start + TimeDelta::Millis(150));

  EXPECT_EQ(
      queue.Enqueue({.bytes = {3},
                     .enqueue_time = start + TimeDelta::Millis(120),
                     .deadline = Timestamp::PlusInfinity(),
                     .priority = DatagramPriority::kVideoDelta,
                     .original_packet_id =
                         DatagramPacketId{.ssrc = 7, .sequence_number = 9}},
                    start + TimeDelta::Millis(120)),
      DatagramEnqueueResult::kQueued);
  ASSERT_TRUE(queue.Pop(start + TimeDelta::Millis(120)).has_value());
  std::optional<QueuedDatagram> rtx = queue.Pop(start + TimeDelta::Millis(120));
  ASSERT_TRUE(rtx.has_value());
  EXPECT_EQ(rtx->deadline, start + TimeDelta::Millis(150));
}

TEST(BoundedDatagramQueueTest, PmtuShrinkPurgesWholeVideoFrame) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {.ssrc = 7, .timestamp = 90000};
  EXPECT_EQ(queue.Enqueue({.bytes = {1, 2, 3},
                           .enqueue_time = now,
                           .deadline = now + TimeDelta::Seconds(1),
                           .priority = DatagramPriority::kVideoDelta,
                           .frame_id = frame},
                          now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue({.bytes = {4, 5, 6, 7, 8},
                           .enqueue_time = now,
                           .deadline = now + TimeDelta::Seconds(1),
                           .priority = DatagramPriority::kVideoDelta,
                           .frame_id = frame},
                          now),
            DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.SetMaxDatagramSize(4), 2u);
  EXPECT_EQ(queue.size(), 0u);
  EXPECT_EQ(queue.pmtu_purge_drops(), 2u);
}

TEST(BoundedDatagramQueueTest, RtxInheritsOriginalAbsoluteDeadline) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramPacketId primary = {.ssrc = 7, .sequence_number = 65535};
  EXPECT_EQ(queue.Enqueue({.bytes = {1},
                           .enqueue_time = start,
                           .deadline = start + TimeDelta::Millis(100),
                           .priority = DatagramPriority::kVideoDelta,
                           .packet_id = primary},
                          start),
            DatagramEnqueueResult::kQueued);
  std::optional<QueuedDatagram> submitted = queue.Pop(start);
  ASSERT_TRUE(submitted.has_value());
  queue.RecordSubmitted(*submitted);

  EXPECT_EQ(queue.Enqueue({.bytes = {2},
                           .enqueue_time = start + TimeDelta::Millis(50),
                           .deadline = Timestamp::PlusInfinity(),
                           .priority = DatagramPriority::kVideoDelta,
                           .original_packet_id = primary},
                          start + TimeDelta::Millis(50)),
            DatagramEnqueueResult::kQueued);
  ASSERT_TRUE(queue.front());
  EXPECT_EQ(queue.front()->deadline, start + TimeDelta::Millis(100));
}

TEST(BoundedDatagramQueueTest, RejectsOrphanAndExpiredRtx) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramPacketId missing = {.ssrc = 7, .sequence_number = 1};
  EXPECT_EQ(queue.Enqueue({.bytes = {1},
                           .enqueue_time = start,
                           .deadline = Timestamp::PlusInfinity(),
                           .priority = DatagramPriority::kVideoDelta,
                           .original_packet_id = missing},
                          start),
            DatagramEnqueueResult::kInvalid);
  EXPECT_EQ(queue.invalid_drops(), 1u);
}

TEST(BoundedDatagramQueueTest, CountsExpiredRtxAsExpiredInsteadOfOrphan) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramPacketId primary = {.ssrc = 7, .sequence_number = 42};
  EXPECT_EQ(queue.Enqueue({.bytes = {1},
                           .enqueue_time = start,
                           .deadline = start + TimeDelta::Millis(100),
                           .priority = DatagramPriority::kVideoDelta,
                           .packet_id = primary},
                          start),
            DatagramEnqueueResult::kQueued);
  std::optional<QueuedDatagram> submitted = queue.Pop(start);
  ASSERT_TRUE(submitted.has_value());
  queue.RecordSubmitted(*submitted);

  EXPECT_EQ(queue.Enqueue({.bytes = {2},
                           .enqueue_time = start + TimeDelta::Millis(101),
                           .deadline = Timestamp::PlusInfinity(),
                           .priority = DatagramPriority::kVideoDelta,
                           .original_packet_id = primary},
                          start + TimeDelta::Millis(101)),
            DatagramEnqueueResult::kInvalid);
  EXPECT_EQ(queue.expired_drops(), 1u);
  EXPECT_EQ(queue.invalid_drops(), 0u);
}

TEST(BoundedDatagramQueueTest, ClearDropsAllPendingMedia) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  EXPECT_EQ(queue.Enqueue({1}), DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue({2}), DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.Clear(), 2u);
  EXPECT_EQ(queue.size(), 0u);
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
