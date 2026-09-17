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

TEST(BoundedDatagramQueueTest, PmtuShrinkRetiresTheWholeDamagedFrame) {
  BoundedDatagramQueue queue(10, 1200);
  const Timestamp now = Timestamp::Millis(100);
  const DatagramFrameId frame{.ssrc = 1, .timestamp = 9000, .binding_id = "v"};
  ASSERT_EQ(queue.Enqueue({.bytes = std::vector<uint8_t>(1100),
                           .binding_id = "v",
                           .frame_id = frame},
                          now),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(queue.Enqueue({.bytes = std::vector<uint8_t>(200),
                           .binding_id = "v",
                           .frame_id = frame},
                          now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.SetMaxDatagramSize(1000, now), 2u);
  EXPECT_EQ(queue.BindingStats("v").recovery_requests, 1u);
  EXPECT_FALSE(queue.Pop(now));
  EXPECT_EQ(queue.Enqueue({.bytes = std::vector<uint8_t>(200),
                           .binding_id = "v",
                           .frame_id = frame,
                           .frame_complete = true},
                          now),
            DatagramEnqueueResult::kDropped);
  EXPECT_EQ(queue.Enqueue({.bytes = std::vector<uint8_t>(200),
                           .binding_id = "v",
                           .frame_id = DatagramFrameId{.ssrc = 1,
                                                       .timestamp = 12000,
                                                       .binding_id = "v"}},
                          now),
            DatagramEnqueueResult::kQueued);
}

TEST(BoundedDatagramQueueTest, RejectedEarlyFrameCannotResumeWithItsTail) {
  BoundedDatagramQueue queue(10, 1200);
  const Timestamp now = Timestamp::Millis(100);
  const DatagramFrameId frame{.ssrc = 1, .timestamp = 9000, .binding_id = "v"};
  ASSERT_EQ(
      queue.Enqueue({.bytes = {1}, .binding_id = "v", .frame_id = frame}, now),
      DatagramEnqueueResult::kQueued);
  queue.Clear();
  EXPECT_EQ(queue.Enqueue({.bytes = {2},
                           .binding_id = "v",
                           .frame_id = frame,
                           .frame_complete = true},
                          now),
            DatagramEnqueueResult::kDropped);
}

QueuedDatagram Datagram(uint8_t value,
                        DatagramPriority priority,
                        Timestamp enqueue_time,
                        TimeDelta lifetime,
                        std::optional<DatagramFrameId> frame_id = std::nullopt,
                        uint64_t flow_id = 0) {
  return {.bytes = {value},
          .flow_id = flow_id,
          .enqueue_time = enqueue_time,
          .deadline = enqueue_time + lifetime,
          .priority = priority,
          .frame_id = frame_id};
}

TEST(BoundedDatagramQueueTest, FairlySharesBytesAcrossEqualPriorityBindings) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  QueuedDatagram large_one = Datagram(1, DatagramPriority::kAudio, now,
                                      TimeDelta::Seconds(1), std::nullopt, 11);
  large_one.bytes.resize(8);
  QueuedDatagram large_two = Datagram(2, DatagramPriority::kAudio, now,
                                      TimeDelta::Seconds(1), std::nullopt, 11);
  large_two.bytes.resize(8);
  QueuedDatagram small_one = Datagram(3, DatagramPriority::kAudio, now,
                                      TimeDelta::Seconds(1), std::nullopt, 22);
  small_one.bytes.resize(4);
  QueuedDatagram small_two = Datagram(4, DatagramPriority::kAudio, now,
                                      TimeDelta::Seconds(1), std::nullopt, 22);
  small_two.bytes.resize(4);
  EXPECT_EQ(queue.Enqueue(std::move(large_one), now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(std::move(large_two), now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(std::move(small_one), now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(std::move(small_two), now),
            DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.Pop(now)->flow_id, 11u);
  EXPECT_EQ(queue.Pop(now)->flow_id, 22u);
  EXPECT_EQ(queue.Pop(now)->flow_id, 22u);
  EXPECT_EQ(queue.Pop(now)->flow_id, 11u);
}

TEST(BoundedDatagramQueueTest, EnforcesConnectionByteCapacity) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8,
                             /*byte_capacity=*/5);
  constexpr Timestamp now = Timestamp::Millis(100);
  QueuedDatagram first = Datagram(1, DatagramPriority::kAudio, now,
                                  TimeDelta::Seconds(1), std::nullopt, 11);
  first.bytes = {1, 2, 3};
  QueuedDatagram second = Datagram(4, DatagramPriority::kAudio, now,
                                   TimeDelta::Seconds(1), std::nullopt, 22);
  second.bytes = {4, 5, 6};
  EXPECT_EQ(queue.Enqueue(std::move(first), now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(std::move(second), now),
            DatagramEnqueueResult::kQueueFull);

  EXPECT_LE(queue.byte_size(), 5u);
  EXPECT_LE(queue.peak_byte_size(), 5u);
  EXPECT_EQ(queue.queue_full_drops(), 1u);
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

TEST(BoundedDatagramQueueTest, DoesNotReorderLaterKeyFrameAheadOfDelta) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  const auto frame = [](uint32_t timestamp) {
    return DatagramFrameId{
        .ssrc = 9, .timestamp = timestamp, .binding_id = "video"};
  };
  QueuedDatagram delta = Datagram(1, DatagramPriority::kVideoDelta, now,
                                  TimeDelta::Seconds(1), frame(90000));
  delta.binding_id = "video";
  delta.frame_complete = true;
  QueuedDatagram key = Datagram(2, DatagramPriority::kVideoKey, now,
                                TimeDelta::Seconds(1), frame(93000));
  key.binding_id = "video";
  key.frame_complete = true;
  key.is_key = key.contains_sps = key.contains_pps = true;
  ASSERT_EQ(queue.Enqueue(std::move(delta), now),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(queue.Enqueue(std::move(key), now), DatagramEnqueueResult::kQueued);

  ASSERT_TRUE(queue.Pop(now));
  EXPECT_EQ(queue.Pop(now)->bytes, (std::vector<uint8_t>{2}));
}

TEST(BoundedDatagramQueueTest, RtxForSubmittedVideoFrameCanBePopped) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  constexpr DatagramPacketId primary_id = {.ssrc = 7, .sequence_number = 42};
  QueuedDatagram primary = Datagram(1, DatagramPriority::kVideoDelta, start,
                                    TimeDelta::Millis(100), frame);
  primary.binding_id = "video";
  primary.packet_id = primary_id;
  primary.frame_complete = true;
  ASSERT_EQ(queue.Enqueue(std::move(primary), start),
            DatagramEnqueueResult::kQueued);
  std::optional<QueuedDatagram> submitted = queue.Pop(start);
  ASSERT_TRUE(submitted.has_value());
  queue.RecordSubmitted(*submitted, start);

  QueuedDatagram rtx =
      Datagram(2, DatagramPriority::kVideoDelta, start + TimeDelta::Millis(50),
               TimeDelta::Seconds(1));
  rtx.binding_id = "video";
  rtx.original_packet_id = primary_id;
  ASSERT_EQ(queue.Enqueue(std::move(rtx), start + TimeDelta::Millis(50)),
            DatagramEnqueueResult::kQueued);

  EXPECT_TRUE(queue.Pop(start + TimeDelta::Millis(50)).has_value());
}

TEST(BoundedDatagramQueueTest, ClearDropsAllPendingMedia) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  EXPECT_EQ(queue.Enqueue(Datagram(1, DatagramPriority::kVideoDelta, now,
                                   TimeDelta::Seconds(1)),
                          now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(Datagram(2, DatagramPriority::kVideoDelta, now,
                                   TimeDelta::Seconds(1)),
                          now),
            DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.Clear(), 2u);
  EXPECT_EQ(queue.size(), 0u);
}

TEST(BoundedDatagramQueueTest, RejectsOversizeWithoutBlockingFollowingPacket) {
  BoundedDatagramQueue queue(/*capacity=*/2, /*max_datagram_size=*/4);
  constexpr Timestamp now = Timestamp::Millis(100);

  QueuedDatagram oversize =
      Datagram(1, DatagramPriority::kVideoDelta, now, TimeDelta::Seconds(1));
  oversize.bytes = {1, 2, 3, 4, 5};
  QueuedDatagram valid =
      Datagram(9, DatagramPriority::kVideoDelta, now, TimeDelta::Seconds(1));
  valid.bytes = {9, 8, 7};
  EXPECT_EQ(queue.Enqueue(std::move(oversize), now),
            DatagramEnqueueResult::kOversize);
  EXPECT_EQ(queue.Enqueue(std::move(valid), now),
            DatagramEnqueueResult::kQueued);
  ASSERT_TRUE(queue.Pop().has_value());
  EXPECT_EQ(queue.Pop(), std::nullopt);
  EXPECT_EQ(queue.oversize_drops(), 1u);
}

TEST(BoundedDatagramQueueTest, PmtuShrinkPurgesQueuedOversizePackets) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  QueuedDatagram first =
      Datagram(1, DatagramPriority::kVideoDelta, now, TimeDelta::Seconds(1));
  first.bytes = {1, 2, 3};
  QueuedDatagram second =
      Datagram(4, DatagramPriority::kVideoDelta, now, TimeDelta::Seconds(1));
  second.bytes = {4, 5, 6, 7, 8};
  QueuedDatagram third =
      Datagram(9, DatagramPriority::kVideoDelta, now, TimeDelta::Seconds(1));
  third.bytes = {9, 10, 11, 12};
  EXPECT_EQ(queue.Enqueue(std::move(first), now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(std::move(second), now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(std::move(third), now),
            DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.SetMaxDatagramSize(4, now), 1u);
  ASSERT_TRUE(queue.Pop().has_value());
  EXPECT_EQ(queue.Pop()->bytes, (std::vector<uint8_t>{9, 10, 11, 12}));
  EXPECT_EQ(queue.pmtu_purge_drops(), 1u);
}

}  // namespace
}  // namespace webrtc::roq
