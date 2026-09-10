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

TEST(BoundedDatagramQueueTest, SuppressesDeltaFramesAfterAFrameExpires) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId dropped = {.ssrc = 7, .timestamp = 90000};
  constexpr DatagramFrameId dependent = {.ssrc = 7, .timestamp = 93000};
  EXPECT_EQ(queue.Enqueue(Datagram(1, DatagramPriority::kVideoDelta, start,
                                   TimeDelta::Millis(10), dropped),
                          start),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Enqueue(Datagram(2, DatagramPriority::kVideoDelta, start,
                                   TimeDelta::Seconds(1), dependent),
                          start),
            DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.PurgeExpired(start + TimeDelta::Millis(11)), 2u);
  EXPECT_EQ(queue.Pop(start + TimeDelta::Millis(11)), std::nullopt);
}

TEST(BoundedDatagramQueueTest,
     DroppingSubmittedPacketPurgesItsFrameAndDependentFrames) {
  BoundedDatagramQueue queue(/*capacity=*/8, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  constexpr DatagramFrameId dropped = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  constexpr DatagramFrameId dependent = {
      .ssrc = 7, .timestamp = 93000, .binding_id = "video"};

  QueuedDatagram first = Datagram(1, DatagramPriority::kVideoDelta, now,
                                  TimeDelta::Seconds(1), dropped);
  first.binding_id = "video";
  QueuedDatagram last = Datagram(2, DatagramPriority::kVideoDelta, now,
                                 TimeDelta::Seconds(1), dropped);
  last.binding_id = "video";
  last.frame_complete = true;
  QueuedDatagram next = Datagram(3, DatagramPriority::kVideoDelta, now,
                                 TimeDelta::Seconds(1), dependent);
  next.binding_id = "video";
  next.frame_complete = true;
  ASSERT_EQ(queue.Enqueue(std::move(first), now),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(queue.Enqueue(std::move(last), now),
            DatagramEnqueueResult::kQueued);
  std::optional<QueuedDatagram> submitted = queue.Pop(now);
  ASSERT_TRUE(submitted);
  ASSERT_EQ(queue.Enqueue(std::move(next), now),
            DatagramEnqueueResult::kQueued);

  queue.RecordDropped(*submitted, now);

  EXPECT_EQ(queue.size(), 0u);
  EXPECT_EQ(queue.dependency_drops(), 1u);
  QueuedDatagram later = Datagram(
      4, DatagramPriority::kVideoDelta, now, TimeDelta::Seconds(1),
      DatagramFrameId{.ssrc = 7, .timestamp = 96000, .binding_id = "video"});
  later.binding_id = "video";
  later.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(later), now),
            DatagramEnqueueResult::kDropped);
}

TEST(BoundedDatagramQueueTest, NeverSendsOnlyTheMarkerAfterFrameEviction) {
  BoundedDatagramQueue queue(/*capacity=*/1, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  QueuedDatagram first = Datagram(1, DatagramPriority::kVideoDelta, now,
                                  TimeDelta::Seconds(1), frame);
  first.binding_id = "video";
  ASSERT_EQ(queue.Enqueue(std::move(first), now),
            DatagramEnqueueResult::kQueued);
  QueuedDatagram marker = Datagram(2, DatagramPriority::kVideoDelta, now,
                                   TimeDelta::Seconds(1), frame);
  marker.binding_id = "video";
  marker.frame_complete = true;

  EXPECT_EQ(queue.Enqueue(std::move(marker), now),
            DatagramEnqueueResult::kQueueFull);
  EXPECT_EQ(queue.Pop(now), std::nullopt);
}

TEST(BoundedDatagramQueueTest, OversizeVideoBreaksItsReferenceChain) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/1);
  constexpr Timestamp now = Timestamp::Millis(100);
  QueuedDatagram oversize = Datagram(
      1, DatagramPriority::kVideoDelta, now, TimeDelta::Seconds(1),
      DatagramFrameId{.ssrc = 7, .timestamp = 90000, .binding_id = "video"});
  oversize.binding_id = "video";
  oversize.bytes = {1, 2};
  EXPECT_EQ(queue.Enqueue(std::move(oversize), now),
            DatagramEnqueueResult::kOversize);
  QueuedDatagram dependent = Datagram(
      2, DatagramPriority::kVideoDelta, now, TimeDelta::Seconds(1),
      DatagramFrameId{.ssrc = 7, .timestamp = 93000, .binding_id = "video"});
  dependent.binding_id = "video";
  dependent.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(dependent), now),
            DatagramEnqueueResult::kDropped);
}

TEST(BoundedDatagramQueueTest, AlreadyExpiredVideoBreaksItsReferenceChain) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  QueuedDatagram expired = Datagram(
      1, DatagramPriority::kVideoDelta, now, TimeDelta::Zero(),
      DatagramFrameId{.ssrc = 7, .timestamp = 90000, .binding_id = "video"});
  expired.binding_id = "video";
  EXPECT_EQ(queue.Enqueue(std::move(expired), now),
            DatagramEnqueueResult::kInvalid);
  QueuedDatagram dependent = Datagram(
      2, DatagramPriority::kVideoDelta, now, TimeDelta::Seconds(1),
      DatagramFrameId{.ssrc = 7, .timestamp = 93000, .binding_id = "video"});
  dependent.binding_id = "video";
  dependent.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(dependent), now),
            DatagramEnqueueResult::kDropped);
}

TEST(BoundedDatagramQueueTest, OversizeVideoKeepsRecoveryCallback) {
  BoundedDatagramQueue queue(/*capacity=*/8, /*max_datagram_size=*/1);
  constexpr Timestamp start = Timestamp::Millis(100);
  int requests = 0;
  QueuedDatagram oversize = Datagram(
      1, DatagramPriority::kVideoDelta, start, TimeDelta::Seconds(1),
      DatagramFrameId{.ssrc = 7, .timestamp = 90000, .binding_id = "video"});
  oversize.binding_id = "video";
  oversize.bytes = {1, 2};
  oversize.request_key_frame = [&] { ++requests; };
  ASSERT_EQ(queue.Enqueue(std::move(oversize), start),
            DatagramEnqueueResult::kOversize);

  auto callbacks = queue.RecoveryCallbacks(start + TimeDelta::Millis(200),
                                           TimeDelta::Millis(50), true);
  ASSERT_EQ(callbacks.size(), 1u);
  callbacks.front()();
  EXPECT_EQ(requests, 1);
}

TEST(BoundedDatagramQueueTest, RequeueRestoresCompleteFrameEligibility) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  QueuedDatagram frame = Datagram(
      1, DatagramPriority::kVideoDelta, now, TimeDelta::Seconds(1),
      DatagramFrameId{.ssrc = 7, .timestamp = 90000, .binding_id = "video"});
  frame.binding_id = "video";
  frame.frame_complete = true;
  ASSERT_EQ(queue.Enqueue(std::move(frame), now),
            DatagramEnqueueResult::kQueued);
  std::optional<QueuedDatagram> blocked = queue.Pop(now);
  ASSERT_TRUE(blocked);

  ASSERT_EQ(queue.Enqueue(std::move(*blocked), now),
            DatagramEnqueueResult::kQueued);

  EXPECT_TRUE(queue.Pop(now).has_value());
}

TEST(BoundedDatagramQueueTest, HoldsVideoPacketUntilFrameMarkerArrives) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  QueuedDatagram packet = Datagram(
      1, DatagramPriority::kVideoDelta, now, TimeDelta::Millis(100),
      DatagramFrameId{.ssrc = 7,
                      .timestamp = 90000,
                      .binding_id = "video"});
  packet.binding_id = "video";
  ASSERT_EQ(queue.Enqueue(std::move(packet), now),
            DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.Pop(now), std::nullopt);
}

TEST(BoundedDatagramQueueTest, PurgeBindingRemovesRtxDeadlineState) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  const DatagramFrameId frame = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  QueuedDatagram primary = Datagram(1, DatagramPriority::kVideoDelta, now,
                                    TimeDelta::Seconds(1), frame);
  primary.binding_id = "video";
  primary.packet_id = {.ssrc = 7, .sequence_number = 10};
  primary.frame_complete = true;
  ASSERT_EQ(queue.Enqueue(std::move(primary), now),
            DatagramEnqueueResult::kQueued);
  std::optional<QueuedDatagram> sent = queue.Pop(now);
  ASSERT_TRUE(sent);
  queue.RecordSubmitted(*sent, now);
  queue.PurgeBinding("video");
  QueuedDatagram rtx =
      Datagram(2, DatagramPriority::kVideoDelta, now, TimeDelta::Seconds(1));
  rtx.binding_id = "video";
  rtx.original_packet_id = {.ssrc = 7, .sequence_number = 10};

  EXPECT_EQ(queue.Enqueue(std::move(rtx), now),
            DatagramEnqueueResult::kInvalid);
}

TEST(BoundedDatagramQueueTest, EvictsLargestBacklogWithinPriority) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  const auto enqueue_frame = [&](const char* binding, uint32_t timestamp,
                                 uint8_t value, Timestamp enqueued) {
    QueuedDatagram datagram = Datagram(
        value, DatagramPriority::kVideoDelta, enqueued, TimeDelta::Seconds(1),
        DatagramFrameId{.ssrc = binding[0] == 'a' ? 7u : 8u,
                        .timestamp = timestamp,
                        .binding_id = binding});
    datagram.binding_id = binding;
    datagram.frame_complete = true;
    return queue.Enqueue(std::move(datagram), now);
  };
  ASSERT_EQ(enqueue_frame("b", 90000, 1, now - TimeDelta::Millis(2)),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(enqueue_frame("a", 90000, 2, now - TimeDelta::Millis(1)),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(enqueue_frame("a", 93000, 3, now), DatagramEnqueueResult::kQueued);

  ASSERT_EQ(queue.Enqueue(Datagram(4, DatagramPriority::kAudio, now,
                                   TimeDelta::Seconds(1)),
                          now),
            DatagramEnqueueResult::kQueued);
  ASSERT_TRUE(queue.Pop(now));
  const std::optional<QueuedDatagram> remaining = queue.Pop(now);
  ASSERT_TRUE(remaining);
  EXPECT_EQ(remaining->binding_id, "b");
  EXPECT_EQ(queue.Pop(now), std::nullopt);
}

TEST(BoundedDatagramQueueTest, StartsCompleteVideoAtTheFirstPacket) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  QueuedDatagram first = Datagram(1, DatagramPriority::kVideoDelta, now,
                                  TimeDelta::Seconds(1), frame);
  first.binding_id = "video";
  EXPECT_EQ(queue.Enqueue(std::move(first), now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Pop(now), std::nullopt);

  QueuedDatagram last = Datagram(2, DatagramPriority::kVideoDelta, now,
                                 TimeDelta::Seconds(1), frame);
  last.binding_id = "video";
  last.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(last), now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.Pop(now)->bytes, (std::vector<uint8_t>{1}));
  EXPECT_EQ(queue.Pop(now)->bytes, (std::vector<uint8_t>{2}));
}

TEST(BoundedDatagramQueueTest, StartsVideoDeadlineWhenFrameCompletes) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  QueuedDatagram first = Datagram(1, DatagramPriority::kVideoDelta, start,
                                  TimeDelta::Millis(100), frame);
  first.binding_id = "video";
  ASSERT_EQ(queue.Enqueue(std::move(first), start),
            DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.PurgeExpired(start + TimeDelta::Millis(110)), 0u);
  QueuedDatagram marker = Datagram(2, DatagramPriority::kVideoDelta,
                                   start + TimeDelta::Millis(110),
                                   TimeDelta::Millis(100), frame);
  marker.binding_id = "video";
  marker.frame_complete = true;
  ASSERT_EQ(queue.Enqueue(std::move(marker),
                          start + TimeDelta::Millis(110)),
            DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.PurgeExpired(start + TimeDelta::Millis(209)), 0u);
  EXPECT_EQ(queue.PurgeExpired(start + TimeDelta::Millis(211)), 2u);
}

TEST(BoundedDatagramQueueTest, IncompleteVideoExpiresOnlyAtAssemblyBoundary) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  QueuedDatagram packet = Datagram(
      1, DatagramPriority::kVideoDelta, start, TimeDelta::Millis(100),
      DatagramFrameId{.ssrc = 7,
                      .timestamp = 90000,
                      .binding_id = "video"});
  packet.binding_id = "video";
  ASSERT_EQ(queue.Enqueue(std::move(packet), start),
            DatagramEnqueueResult::kQueued);

  EXPECT_EQ(queue.PurgeExpired(start + TimeDelta::Millis(999)), 0u);
  EXPECT_EQ(queue.PurgeExpired(start + TimeDelta::Seconds(1)), 1u);
}

TEST(BoundedDatagramQueueTest, DoesNotExpireVideoAfterSubmissionStarts) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  QueuedDatagram first = Datagram(1, DatagramPriority::kVideoDelta, start,
                                  TimeDelta::Millis(100), frame);
  first.binding_id = "video";
  QueuedDatagram marker = Datagram(2, DatagramPriority::kVideoDelta, start,
                                   TimeDelta::Millis(100), frame);
  marker.binding_id = "video";
  marker.frame_complete = true;
  ASSERT_EQ(queue.Enqueue(std::move(first), start),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(queue.Enqueue(std::move(marker), start),
            DatagramEnqueueResult::kQueued);
  std::optional<QueuedDatagram> submitted = queue.Pop(start);
  ASSERT_TRUE(submitted);
  EXPECT_TRUE(submitted->committed);

  EXPECT_EQ(queue.PurgeExpired(start + TimeDelta::Millis(101)), 0u);
  std::optional<QueuedDatagram> remaining =
      queue.Pop(start + TimeDelta::Millis(101));
  ASSERT_TRUE(remaining);
  EXPECT_TRUE(remaining->committed);
}

TEST(BoundedDatagramQueueTest, AcceptsLatePacketAfterReservedFrameStarts) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8,
                             /*byte_capacity=*/16);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  QueuedDatagram reservation = Datagram(0, DatagramPriority::kVideoKey, start,
                                        TimeDelta::Millis(100), frame);
  reservation.binding_id = "video";
  ASSERT_TRUE(queue.ReserveVideoFrame(std::move(reservation), 2, 2, start));
  QueuedDatagram first = Datagram(1, DatagramPriority::kVideoKey, start,
                                  TimeDelta::Millis(100), frame);
  first.binding_id = "video";
  first.contains_sps = true;
  ASSERT_EQ(queue.Enqueue(std::move(first), start),
            DatagramEnqueueResult::kQueued);
  auto sent = queue.Pop(start);
  ASSERT_TRUE(sent);
  queue.RecordSubmitted(*sent, start);

  QueuedDatagram marker = Datagram(2, DatagramPriority::kVideoKey,
                                   start + TimeDelta::Millis(101),
                                   TimeDelta::Millis(100), frame);
  marker.binding_id = "video";
  marker.contains_pps = true;
  marker.is_key = true;
  marker.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(marker),
                          start + TimeDelta::Millis(101)),
            DatagramEnqueueResult::kQueued);
  EXPECT_TRUE(queue.Pop(start + TimeDelta::Millis(101)));
}

TEST(BoundedDatagramQueueTest, ReservedVideoCanStreamBeforeMarker) {
  BoundedDatagramQueue queue(/*capacity=*/2, /*max_datagram_size=*/8,
                             /*byte_capacity=*/16);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  QueuedDatagram reservation = Datagram(0, DatagramPriority::kVideoKey, start,
                                        TimeDelta::Millis(150), frame);
  reservation.binding_id = "video";
  reservation.independent_frame = true;
  ASSERT_TRUE(queue.ReserveVideoFrame(std::move(reservation), 2, 2, start));

  QueuedDatagram first = Datagram(1, DatagramPriority::kVideoDelta, start,
                                  TimeDelta::Millis(100), frame);
  first.binding_id = "video";
  first.committed = true;
  ASSERT_EQ(queue.Enqueue(std::move(first), start),
            DatagramEnqueueResult::kQueued);
  std::optional<QueuedDatagram> popped = queue.Pop(start);
  ASSERT_TRUE(popped);
  EXPECT_TRUE(popped->committed);
  EXPECT_EQ(popped->priority, DatagramPriority::kVideoKey);
}

TEST(BoundedDatagramQueueTest, ReservedKeyFrameRepairsBrokenChainOnMarker) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8,
                             /*byte_capacity=*/16);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  queue.RequireIndependentVideoFrame("video", 7, start);
  QueuedDatagram reservation = Datagram(0, DatagramPriority::kVideoKey, start,
                                        TimeDelta::Millis(150), frame);
  reservation.binding_id = "video";
  reservation.independent_frame = true;
  ASSERT_TRUE(queue.ReserveVideoFrame(std::move(reservation), 2, 2, start));
  QueuedDatagram first = Datagram(1, DatagramPriority::kVideoDelta, start,
                                  TimeDelta::Millis(100), frame);
  first.binding_id = "video";
  QueuedDatagram marker = Datagram(2, DatagramPriority::kVideoKey, start,
                                   TimeDelta::Millis(150), frame);
  marker.binding_id = "video";
  marker.is_key = true;
  marker.contains_sps = true;
  marker.contains_pps = true;
  marker.frame_complete = true;
  ASSERT_EQ(queue.Enqueue(std::move(first), start),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(queue.Enqueue(std::move(marker), start),
            DatagramEnqueueResult::kQueued);
  auto submitted_first = queue.Pop(start);
  auto submitted_marker = queue.Pop(start);
  ASSERT_TRUE(submitted_first);
  ASSERT_TRUE(submitted_marker);
  queue.RecordSubmitted(*submitted_first, start);
  queue.RecordSubmitted(*submitted_marker, start);

  QueuedDatagram delta = Datagram(3, DatagramPriority::kVideoDelta, start,
                                  TimeDelta::Seconds(1),
                                  DatagramFrameId{.ssrc = 7,
                                                  .timestamp = 93000,
                                                  .binding_id = "video"});
  delta.binding_id = "video";
  delta.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(delta), start),
            DatagramEnqueueResult::kQueued);
}

TEST(BoundedDatagramQueueTest, ReservedVideoSurvivesSchedulingDelay) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8,
                             /*byte_capacity=*/16);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  QueuedDatagram reservation = Datagram(0, DatagramPriority::kVideoDelta,
                                        start, TimeDelta::Millis(100), frame);
  reservation.binding_id = "video";
  ASSERT_TRUE(queue.ReserveVideoFrame(std::move(reservation), 1, 1, start));

  QueuedDatagram packet = Datagram(1, DatagramPriority::kVideoDelta, start,
                                   TimeDelta::Millis(100), frame);
  packet.binding_id = "video";
  packet.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(packet),
                          start + TimeDelta::Millis(101)),
            DatagramEnqueueResult::kQueued);
  EXPECT_TRUE(queue.Pop(start + TimeDelta::Millis(101)));
  EXPECT_EQ(queue.expired_drops(), 0u);
}

TEST(BoundedDatagramQueueTest, ReservationRejectsWholeFrameBeforePacketsQueue) {
  BoundedDatagramQueue queue(/*capacity=*/1, /*max_datagram_size=*/8,
                             /*byte_capacity=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  QueuedDatagram frame = Datagram(0, DatagramPriority::kVideoDelta, start,
                                  TimeDelta::Millis(100),
                                  DatagramFrameId{.ssrc = 7,
                                                  .timestamp = 90000,
                                                  .binding_id = "video"});
  frame.binding_id = "video";

  EXPECT_FALSE(queue.ReserveVideoFrame(std::move(frame), 2, 2, start));
  EXPECT_EQ(queue.queue_full_drops(), 2u);
  EXPECT_EQ(queue.size(), 0u);
}

TEST(BoundedDatagramQueueTest, ReservedFrameCannotBeEvictedByLaterVideo) {
  BoundedDatagramQueue queue(/*capacity=*/4, /*max_datagram_size=*/8,
                             /*byte_capacity=*/16);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId committed = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  QueuedDatagram reservation = Datagram(0, DatagramPriority::kVideoDelta,
                                        start, TimeDelta::Millis(100),
                                        committed);
  reservation.binding_id = "video";
  ASSERT_TRUE(queue.ReserveVideoFrame(std::move(reservation), 3, 12, start));

  QueuedDatagram first = Datagram(1, DatagramPriority::kVideoDelta, start,
                                  TimeDelta::Millis(100), committed);
  first.bytes.resize(4);
  first.binding_id = "video";
  first.committed = true;
  ASSERT_EQ(queue.Enqueue(std::move(first), start),
            DatagramEnqueueResult::kQueued);

  QueuedDatagram later = Datagram(2, DatagramPriority::kVideoKey, start,
                                  TimeDelta::Millis(150),
                                  DatagramFrameId{.ssrc = 8,
                                                  .timestamp = 93000,
                                                  .binding_id = "other"});
  later.bytes.resize(8);
  later.binding_id = "other";
  later.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(later), start),
            DatagramEnqueueResult::kQueueFull);
  ASSERT_TRUE(queue.Pop(start));
  EXPECT_EQ(queue.Pop(start), std::nullopt);
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
            DatagramEnqueueResult::kQueued);

  EXPECT_LE(queue.byte_size(), 5u);
  EXPECT_LE(queue.peak_byte_size(), 5u);
  EXPECT_EQ(queue.queue_full_drops(), 1u);
}

TEST(BoundedDatagramQueueTest, OnlyCompleteIndependentFrameRepairsBrokenChain) {
  BoundedDatagramQueue queue(/*capacity=*/8, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  const auto frame = [](uint32_t timestamp) {
    return DatagramFrameId{
        .ssrc = 7, .timestamp = timestamp, .binding_id = "video"};
  };
  QueuedDatagram dropped = Datagram(1, DatagramPriority::kVideoDelta, start,
                                    TimeDelta::Millis(10), frame(90000));
  dropped.binding_id = "video";
  dropped.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(dropped), start),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.PurgeExpired(start + TimeDelta::Millis(11)), 1u);

  QueuedDatagram idr = Datagram(2, DatagramPriority::kVideoKey, start,
                                TimeDelta::Seconds(1), frame(93000));
  idr.binding_id = "video";
  idr.is_key = true;
  idr.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(idr), start),
            DatagramEnqueueResult::kQueued);
  QueuedDatagram suppressed = Datagram(3, DatagramPriority::kVideoDelta, start,
                                       TimeDelta::Seconds(1), frame(96000));
  suppressed.binding_id = "video";
  suppressed.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(suppressed), start),
            DatagramEnqueueResult::kDropped);
  std::optional<QueuedDatagram> incomplete = queue.Pop(start);
  ASSERT_TRUE(incomplete);
  queue.RecordSubmitted(*incomplete, start);

  QueuedDatagram recovery = Datagram(4, DatagramPriority::kVideoKey, start,
                                     TimeDelta::Seconds(1), frame(99000));
  recovery.binding_id = "video";
  recovery.is_key = true;
  recovery.contains_sps = true;
  recovery.contains_pps = true;
  recovery.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(recovery), start),
            DatagramEnqueueResult::kQueued);
  std::optional<QueuedDatagram> recovered = queue.Pop(start);
  ASSERT_TRUE(recovered);
  queue.RecordSubmitted(*recovered, start);
  QueuedDatagram delta = Datagram(5, DatagramPriority::kVideoDelta, start,
                                  TimeDelta::Seconds(1), frame(102000));
  delta.binding_id = "video";
  delta.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(delta), start),
            DatagramEnqueueResult::kQueued);
}

TEST(BoundedDatagramQueueTest,
     ExplicitRecoveryGateSuppressesDeltaUntilIndependentFrame) {
  BoundedDatagramQueue queue(/*capacity=*/8, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  const auto frame = [](uint32_t timestamp) {
    return DatagramFrameId{
        .ssrc = 7, .timestamp = timestamp, .binding_id = "video"};
  };

  queue.RequireIndependentVideoFrame("video", 7, now);
  QueuedDatagram delta = Datagram(1, DatagramPriority::kVideoDelta, now,
                                  TimeDelta::Seconds(1), frame(90000));
  delta.binding_id = "video";
  delta.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(delta), now),
            DatagramEnqueueResult::kDropped);

  QueuedDatagram recovery = Datagram(2, DatagramPriority::kVideoKey, now,
                                     TimeDelta::Seconds(1), frame(93000));
  recovery.binding_id = "video";
  recovery.frame_complete = true;
  recovery.is_key = true;
  recovery.contains_sps = true;
  recovery.contains_pps = true;
  ASSERT_EQ(queue.Enqueue(std::move(recovery), now),
            DatagramEnqueueResult::kQueued);
  auto submitted = queue.Pop(now);
  ASSERT_TRUE(submitted);
  queue.RecordSubmitted(*submitted, now);

  QueuedDatagram next = Datagram(3, DatagramPriority::kVideoDelta, now,
                                 TimeDelta::Seconds(1), frame(96000));
  next.binding_id = "video";
  next.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(next), now),
            DatagramEnqueueResult::kQueued);
}

#if defined(RTC_ENABLE_H265)
TEST(BoundedDatagramQueueTest, H265RecoveryRequiresInBandVps) {
  BoundedDatagramQueue queue(/*capacity=*/8, /*max_datagram_size=*/8);
  constexpr Timestamp now = Timestamp::Millis(100);
  const auto frame = [](uint32_t timestamp) {
    return DatagramFrameId{
        .ssrc = 7, .timestamp = timestamp, .binding_id = "video"};
  };
  queue.RequireIndependentVideoFrame("video", 7, now);

  QueuedDatagram without_vps = Datagram(1, DatagramPriority::kVideoKey, now,
                                        TimeDelta::Seconds(1), frame(90000));
  without_vps.binding_id = "video";
  without_vps.video_codec = kVideoCodecH265;
  without_vps.frame_complete = true;
  without_vps.is_key = true;
  without_vps.contains_sps = true;
  without_vps.contains_pps = true;
  EXPECT_EQ(queue.Enqueue(std::move(without_vps), now),
            DatagramEnqueueResult::kQueued);
  auto incomplete = queue.Pop(now);
  ASSERT_TRUE(incomplete);
  queue.RecordSubmitted(*incomplete, now);
  QueuedDatagram suppressed = Datagram(3, DatagramPriority::kVideoDelta, now,
                                       TimeDelta::Seconds(1), frame(92000));
  suppressed.binding_id = "video";
  suppressed.video_codec = kVideoCodecH265;
  suppressed.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(suppressed), now),
            DatagramEnqueueResult::kDropped);

  QueuedDatagram recovery = Datagram(4, DatagramPriority::kVideoKey, now,
                                     TimeDelta::Seconds(1), frame(93000));
  recovery.binding_id = "video";
  recovery.video_codec = kVideoCodecH265;
  recovery.frame_complete = true;
  recovery.is_key = true;
  recovery.contains_vps = true;
  recovery.contains_sps = true;
  recovery.contains_pps = true;
  EXPECT_EQ(queue.Enqueue(std::move(recovery), now),
            DatagramEnqueueResult::kQueued);
}
#endif

TEST(BoundedDatagramQueueTest, RequestsKeyFrameAfterASettledRecoveryWindow) {
  BoundedDatagramQueue queue(/*capacity=*/8, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  const DatagramFrameId frame = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  int requests = 0;
  QueuedDatagram dropped = Datagram(1, DatagramPriority::kVideoDelta, start,
                                    TimeDelta::Millis(10), frame);
  dropped.binding_id = "video";
  dropped.frame_complete = true;
  dropped.request_key_frame = [&] { ++requests; };
  ASSERT_EQ(queue.Enqueue(std::move(dropped), start),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(queue.PurgeExpired(start + TimeDelta::Millis(11)), 1u);

  EXPECT_TRUE(queue
                  .RecoveryCallbacks(start + TimeDelta::Millis(199),
                                     TimeDelta::Millis(50), true)
                  .empty());
  auto callbacks = queue.RecoveryCallbacks(start + TimeDelta::Millis(211),
                                           TimeDelta::Millis(50), true);
  ASSERT_EQ(callbacks.size(), 1u);
  callbacks[0]();
  EXPECT_EQ(requests, 1);
  EXPECT_TRUE(queue
                  .RecoveryCallbacks(start + TimeDelta::Millis(500),
                                     TimeDelta::Millis(50), true)
                  .empty());
  EXPECT_EQ(queue
                .RecoveryCallbacks(start + TimeDelta::Millis(1212),
                                   TimeDelta::Millis(50), true)
                .size(),
            1u);
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
  ASSERT_EQ(queue.Enqueue(std::move(key), now),
            DatagramEnqueueResult::kQueued);

  ASSERT_TRUE(queue.Pop(now));
  EXPECT_EQ(queue.Pop(now)->bytes, (std::vector<uint8_t>{2}));
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
  queue.RecordSubmitted(*submitted, start);
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
  queue.RecordSubmitted(*submitted, start);

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
  EXPECT_EQ(rtx->deadline, start + TimeDelta::Millis(320));
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

  EXPECT_EQ(queue.SetMaxDatagramSize(4, now), 2u);
  EXPECT_EQ(queue.size(), 0u);
  EXPECT_EQ(queue.pmtu_purge_drops(), 2u);
}

TEST(BoundedDatagramQueueTest, RtxGetsFreshDeadlineWithinRepairWindow) {
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
  queue.RecordSubmitted(*submitted, start);

  EXPECT_EQ(queue.Enqueue({.bytes = {2},
                           .enqueue_time = start + TimeDelta::Millis(110),
                           .deadline = Timestamp::PlusInfinity(),
                           .priority = DatagramPriority::kVideoDelta,
                           .original_packet_id = primary},
                          start + TimeDelta::Millis(110)),
            DatagramEnqueueResult::kQueued);
  ASSERT_TRUE(queue.front());
  EXPECT_EQ(queue.front()->deadline, start + TimeDelta::Millis(310));
}

TEST(BoundedDatagramQueueTest, RepairWindowStartsWhenPrimaryIsSubmitted) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramPacketId primary = {.ssrc = 7, .sequence_number = 42};
  ASSERT_EQ(queue.Enqueue({.bytes = {1},
                           .enqueue_time = start,
                           .deadline = start + TimeDelta::Seconds(2),
                           .priority = DatagramPriority::kVideoDelta,
                           .packet_id = primary},
                          start),
            DatagramEnqueueResult::kQueued);
  auto sent = queue.Pop(start + TimeDelta::Millis(750));
  ASSERT_TRUE(sent);
  queue.RecordSubmitted(*sent, start + TimeDelta::Millis(750));

  EXPECT_EQ(queue.Enqueue({.bytes = {2},
                           .enqueue_time = start + TimeDelta::Millis(1500),
                           .deadline = Timestamp::PlusInfinity(),
                           .priority = DatagramPriority::kVideoDelta,
                           .original_packet_id = primary},
                          start + TimeDelta::Millis(1500)),
            DatagramEnqueueResult::kQueued);
}

TEST(BoundedDatagramQueueTest, RtxForSubmittedVideoFrameCanBePopped) {
  BoundedDatagramQueue queue(/*capacity=*/3, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  constexpr DatagramFrameId frame = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  constexpr DatagramPacketId primary_id = {
      .ssrc = 7, .sequence_number = 42};
  QueuedDatagram primary =
      Datagram(1, DatagramPriority::kVideoDelta, start,
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
      Datagram(2, DatagramPriority::kVideoDelta,
               start + TimeDelta::Millis(50), TimeDelta::Seconds(1));
  rtx.binding_id = "video";
  rtx.original_packet_id = primary_id;
  ASSERT_EQ(queue.Enqueue(std::move(rtx), start + TimeDelta::Millis(50)),
            DatagramEnqueueResult::kQueued);

  EXPECT_TRUE(queue.Pop(start + TimeDelta::Millis(50)).has_value());
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
  queue.RecordSubmitted(*submitted, start);

  EXPECT_EQ(queue.Enqueue({.bytes = {2},
                           .enqueue_time = start + TimeDelta::Millis(1001),
                           .deadline = Timestamp::PlusInfinity(),
                           .priority = DatagramPriority::kVideoDelta,
                           .original_packet_id = primary},
                          start + TimeDelta::Millis(1001)),
            DatagramEnqueueResult::kInvalid);
  EXPECT_EQ(queue.expired_drops(), 1u);
  EXPECT_EQ(queue.invalid_drops(), 0u);
}

TEST(BoundedDatagramQueueTest, ExpiredRtxDoesNotBreakPrimaryVideoChain) {
  BoundedDatagramQueue queue(/*capacity=*/8, /*max_datagram_size=*/8);
  constexpr Timestamp start = Timestamp::Millis(100);
  const DatagramFrameId frame = {
      .ssrc = 7, .timestamp = 90000, .binding_id = "video"};
  QueuedDatagram primary = Datagram(1, DatagramPriority::kVideoDelta, start,
                                    TimeDelta::Seconds(1), frame);
  primary.binding_id = "video";
  primary.packet_id = DatagramPacketId{.ssrc = 7, .sequence_number = 1};
  primary.frame_complete = true;
  ASSERT_EQ(queue.Enqueue(std::move(primary), start),
            DatagramEnqueueResult::kQueued);
  auto submitted = queue.Pop(start);
  ASSERT_TRUE(submitted);
  queue.RecordSubmitted(*submitted, start);

  QueuedDatagram rtx = Datagram(2, DatagramPriority::kVideoDelta,
                                start + TimeDelta::Millis(1),
                                TimeDelta::Millis(10));
  rtx.binding_id = "video";
  rtx.original_packet_id = DatagramPacketId{.ssrc = 7, .sequence_number = 1};
  ASSERT_EQ(queue.Enqueue(std::move(rtx), start + TimeDelta::Millis(1)),
            DatagramEnqueueResult::kQueued);
  ASSERT_TRUE(queue.front());
  EXPECT_TRUE(queue.front()->original_packet_id.has_value());
  EXPECT_EQ(queue.front()->deadline, start + TimeDelta::Millis(201));
  EXPECT_EQ(queue.PurgeExpired(start + TimeDelta::Millis(202)), 1u);

  QueuedDatagram next = Datagram(3, DatagramPriority::kVideoDelta, start,
                                 TimeDelta::Seconds(1),
                                 DatagramFrameId{.ssrc = 7,
                                                 .timestamp = 93000,
                                                 .binding_id = "video"});
  next.binding_id = "video";
  next.frame_complete = true;
  EXPECT_EQ(queue.Enqueue(std::move(next), start + TimeDelta::Millis(202)),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(queue.dependency_drops(), 0u);
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
