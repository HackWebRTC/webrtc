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

TEST(BoundedDatagramQueueTest, DiagnosticsKeepKeyIdentityAndSeparateSlot) {
  BoundedDatagramQueue q(8, 1200);
  const auto start = Timestamp::Millis(100);
  QueuedDatagram key{.bytes = {1, 2, 3},
                     .binding_id = "v",
                     .priority = DatagramPriority::kVideoKey,
                     .is_key = true};
  ASSERT_EQ(q.Enqueue(key, start), DatagramEnqueueResult::kQueued);
  auto slot = q.Pop(start + TimeDelta::Millis(10));
  ASSERT_TRUE(slot);
  EXPECT_EQ(slot->priority, DatagramPriority::kVideoDelta);
  q.RecordSubmitted(*slot, start + TimeDelta::Millis(10));
  ASSERT_EQ(q.Enqueue({.bytes = {4, 5},
                       .binding_id = "sr",
                       .priority = DatagramPriority::kPeriodicRtcp},
                      start),
            DatagramEnqueueResult::kQueued);
  auto stats = q.Diagnostics(start + TimeDelta::Millis(20), &*slot);
  const auto key_class = static_cast<size_t>(DatagramPriority::kVideoKey);
  const auto sr_class = static_cast<size_t>(DatagramPriority::kPeriodicRtcp);
  EXPECT_EQ(stats[key_class].enqueued_packets, 1u);
  EXPECT_EQ(stats[key_class].submitted_bytes, 3u);
  EXPECT_EQ(stats[key_class].queued_packets, 0u);
  EXPECT_EQ(stats[key_class].slot_packets, 1u);
  EXPECT_EQ(stats[key_class].oldest_age_us, 20000);
  EXPECT_EQ(stats[sr_class].queued_bytes, 2u);
  q.PurgeBinding("sr");
  stats = q.Diagnostics(start + TimeDelta::Millis(30));
  EXPECT_EQ(stats[sr_class].purged_packets, 1u);
  EXPECT_EQ(stats[sr_class].queued_packets, 0u);
  EXPECT_EQ(stats[sr_class].oldest_age_us, 0);
}

TEST(BoundedDatagramQueueTest, DiagnosticsCountExpiredKeyAndPreserveRtx) {
  BoundedDatagramQueue q(8, 1200);
  const auto start = Timestamp::Millis(100);
  ASSERT_EQ(q.Enqueue({.bytes = {1, 2, 3},
                       .binding_id = "v",
                       .priority = DatagramPriority::kVideoKey,
                       .is_key = true},
                      start),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(q.Enqueue({.bytes = {4, 5},
                       .binding_id = "v",
                       .original_packet_id = DatagramPacketId{1, 2}},
                      start),
            DatagramEnqueueResult::kQueued);
  q.TrimVideo(start + TimeDelta::Millis(600));
  const auto stats = q.Diagnostics(start + TimeDelta::Millis(600));
  const auto key = static_cast<size_t>(DatagramPriority::kVideoKey);
  const auto rtx = static_cast<size_t>(DatagramPriority::kRetransmission);
  EXPECT_EQ(stats[key].dropped_packets, 1u);
  EXPECT_EQ(stats[key].dropped_bytes, 3u);
  EXPECT_EQ(stats[rtx].queued_packets, 1u);
  EXPECT_EQ(stats[rtx].oldest_age_us, 600000);
}

TEST(BoundedDatagramQueueTest, CriticalTrafficDisplacesNewestVideoWhenFull) {
  for (const auto priority :
       {DatagramPriority::kAudio, DatagramPriority::kRtcp}) {
    BoundedDatagramQueue q(2, 4, 8);
    const auto now = Timestamp::Millis(100);
    ASSERT_EQ(q.Enqueue({.bytes = {1, 1, 1, 1}, .binding_id = "v"}, now),
              DatagramEnqueueResult::kQueued);
    ASSERT_EQ(q.Enqueue({.bytes = {2, 2, 2, 2}, .binding_id = "v"}, now),
              DatagramEnqueueResult::kQueued);
    EXPECT_EQ(
        q.Enqueue(
            {.bytes = {9, 9, 9, 9}, .binding_id = "f", .priority = priority},
            now),
        DatagramEnqueueResult::kQueued);
    EXPECT_EQ(q.Pop(now)->bytes, (std::vector<uint8_t>{9, 9, 9, 9}));
    EXPECT_EQ(q.Pop(now)->bytes, (std::vector<uint8_t>{1, 1, 1, 1}));
    EXPECT_FALSE(q.Pop(now));
    EXPECT_EQ(q.BindingStats("v").dropped_packets, 1u);
  }
}
TEST(BoundedDatagramQueueTest, RTXPrecedesQueuedNewVideo) {
  BoundedDatagramQueue q(4, 10);
  const auto now = Timestamp::Millis(100);
  ASSERT_EQ(q.Enqueue({.bytes = {1}, .binding_id = "v"}, now),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(q.Enqueue({.bytes = {2},
                       .binding_id = "v",
                       .original_packet_id = RoqPacketId{7, 1}},
                      now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(q.Pop(now)->bytes, (std::vector<uint8_t>{2}));
  EXPECT_EQ(q.Pop(now)->bytes, (std::vector<uint8_t>{1}));
}

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

TEST(BoundedDatagramQueueTest, QueueAgeTrimsWholeFramesAndRejectsLateTails) {
  BoundedDatagramQueue q(16, 1200);
  const auto start = Timestamp::Millis(100);
  auto video = [&](uint8_t value, uint32_t ts, int ms) {
    auto d = Datagram(value, DatagramPriority::kVideoDelta,
                      start + TimeDelta::Millis(ms), TimeDelta::Seconds(10),
                      DatagramFrameId{7, ts, "v"}, 10);
    d.binding_id = "v";
    return d;
  };
  ASSERT_EQ(q.Enqueue(video(1, 9000, 0), start),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(q.Enqueue(video(2, 12000, 200), start + TimeDelta::Millis(200)),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(q.Enqueue(video(3, 9000, 400), start + TimeDelta::Millis(400)),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(q.Enqueue(video(4, 15000, 450), start + TimeDelta::Millis(450)),
            DatagramEnqueueResult::kQueued);
  auto sent = q.Pop(start + TimeDelta::Millis(500));
  ASSERT_TRUE(sent);
  EXPECT_EQ(sent->bytes, (std::vector<uint8_t>{4}));
  EXPECT_FALSE(q.Pop(start + TimeDelta::Millis(500)));
  EXPECT_EQ(q.expired_drops(), 3u);
  EXPECT_EQ(q.Enqueue(video(5, 9000, 510), start + TimeDelta::Millis(510)),
            DatagramEnqueueResult::kDropped);
  EXPECT_EQ(q.Enqueue(video(6, 18000, 520), start + TimeDelta::Millis(520)),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(q.Pop(start + TimeDelta::Millis(520))->bytes,
            (std::vector<uint8_t>{6}));
}

TEST(BoundedDatagramQueueTest, QueueAgeProtectsAudioAndRtcp) {
  BoundedDatagramQueue q(8, 1200);
  const auto start = Timestamp::Millis(100);
  for (auto priority : {DatagramPriority::kAudio, DatagramPriority::kRtcp,
                        DatagramPriority::kPeriodicRtcp}) {
    ASSERT_EQ(
        q.Enqueue(Datagram(1, priority, start, TimeDelta::Millis(1)), start),
        DatagramEnqueueResult::kQueued);
  }
  ASSERT_EQ(q.Enqueue(Datagram(2, DatagramPriority::kVideoDelta, start,
                               TimeDelta::Seconds(10)),
                      start),
            DatagramEnqueueResult::kQueued);
  for (int i = 0; i < 3; ++i) {
    auto packet = q.Pop(start + TimeDelta::Seconds(2));
    ASSERT_TRUE(packet);
    EXPECT_NE(packet->priority, DatagramPriority::kVideoDelta);
  }
  EXPECT_FALSE(q.Pop(start + TimeDelta::Seconds(2)));
  EXPECT_EQ(q.expired_drops(), 1u);
}

TEST(BoundedDatagramQueueTest, SlotAgeRetiresFrameWithoutCrossingFlowOrSsrc) {
  BoundedDatagramQueue q(16, 1200);
  const auto start = Timestamp::Millis(100);
  auto video = [&](uint8_t n, uint64_t flow, uint32_t ssrc, int ms) {
    auto d = Datagram(n, DatagramPriority::kVideoDelta,
                      start + TimeDelta::Millis(ms), TimeDelta::Seconds(10),
                      DatagramFrameId{ssrc, 9000, "v"}, flow);
    d.binding_id = "v";
    return d;
  };
  ASSERT_EQ(q.Enqueue(video(1, 10, 7, 0), start),
            DatagramEnqueueResult::kQueued);
  auto slot = q.Pop(start);
  ASSERT_TRUE(slot);
  ASSERT_EQ(q.Enqueue(video(2, 10, 7, 400), start + TimeDelta::Millis(400)),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(q.Enqueue(video(3, 11, 7, 250), start + TimeDelta::Millis(400)),
            DatagramEnqueueResult::kQueued);
  ASSERT_EQ(q.Enqueue(video(4, 10, 8, 250), start + TimeDelta::Millis(400)),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(q.NextVideoExpiry(&*slot), start + TimeDelta::Millis(500));
  EXPECT_TRUE(q.TrimVideo(start + TimeDelta::Millis(500), &*slot));
  EXPECT_EQ(q.expired_drops(), 2u);
  EXPECT_EQ(q.expired_video_frames(), 1u);
  EXPECT_EQ(q.OldestVideoAge(start + TimeDelta::Millis(500)),
            TimeDelta::Millis(250));
  EXPECT_EQ(q.Pop(start + TimeDelta::Millis(500))->bytes,
            (std::vector<uint8_t>{3}));
  EXPECT_EQ(q.Pop(start + TimeDelta::Millis(500))->bytes,
            (std::vector<uint8_t>{4}));
  EXPECT_EQ(q.BindingStats("v").recovery_requests, 1u);
  // RTX eligibility remains owned by the RTP layer, not a retirement marker.
  auto rtx = video(5, 10, 7, 510);
  rtx.original_packet_id = DatagramPacketId{7, 1};
  EXPECT_EQ(q.Enqueue(std::move(rtx), start + TimeDelta::Millis(510)),
            DatagramEnqueueResult::kQueued);
  EXPECT_TRUE(q.Pop(start + TimeDelta::Millis(510)));
}

TEST(BoundedDatagramQueueTest, SustainedOverloadBoundsAgeAndRecoveryRate) {
  BoundedDatagramQueue q(512, 1200);
  const auto start = Timestamp::Millis(100);
  for (int ms = 0; ms <= 10000; ms += 10) {
    auto at = start + TimeDelta::Millis(ms);
    auto d =
        Datagram(1, DatagramPriority::kVideoDelta, at, TimeDelta::Seconds(10),
                 DatagramFrameId{7, static_cast<uint32_t>(ms), "v"}, 10);
    d.binding_id = "v";
    ASSERT_EQ(q.Enqueue(std::move(d), at), DatagramEnqueueResult::kQueued);
    EXPECT_LT(q.OldestVideoAge(at), BoundedDatagramQueue::kVideoHighWater);
    EXPECT_LE(q.size(), 51u);
  }
  EXPECT_GT(q.expired_video_frames(), 0u);
  EXPECT_LE(q.recovery_requests(), 10u);
  EXPECT_EQ(q.queue_full_drops(), 0u);
  auto at = start + TimeDelta::Seconds(11);
  q.TrimVideo(at);
  EXPECT_FALSE(q.Pop(at));
  EXPECT_TRUE(q.NextVideoExpiry().IsPlusInfinity());
}

TEST(BoundedDatagramQueueTest, RecoveryServiceWakeAndRequeuePreserveCredit) {
  BoundedDatagramQueue q(20, 1200);
  auto now = Timestamp::Millis(100);
  q.SetRecoveryRate(10000, now);
  auto packet = [&](uint16_t seq) {
    return QueuedDatagram{.bytes = std::vector<uint8_t>(1100),
                         .binding_id = "v", .flow_id = 1,
                         .original_packet_id = RoqPacketId{7, seq}};
  };
  for (uint16_t i = 1; i <= 3; ++i)
    EXPECT_EQ(q.Enqueue(packet(i), now), DatagramEnqueueResult::kQueued);
  auto first = q.Pop(now);
  ASSERT_TRUE(first);
  q.Requeue(std::move(*first));
  EXPECT_TRUE(q.Pop(now));
  EXPECT_TRUE(q.Pop(now));
  EXPECT_FALSE(q.Pop(now));
  auto wake = q.NextRecoveryService(now);
  EXPECT_GT(wake, now);
  EXPECT_LE(wake, now + TimeDelta::Millis(120));
  EXPECT_TRUE(q.Pop(wake));
  EXPECT_TRUE(q.NextRecoveryService(wake).IsPlusInfinity());
}

TEST(BoundedDatagramQueueTest, RecoveryBudgetDefersServiceAndDeduplicates) {
  BoundedDatagramQueue q(20, 1200);
  auto now = Timestamp::Millis(100);
  q.SetRecoveryRate(10000, now);
  auto packet = [&](uint16_t seq) {
    return QueuedDatagram{.bytes = std::vector<uint8_t>(1100),
                          .binding_id = "v",
                          .flow_id = 1,
                          .original_packet_id = RoqPacketId{7, seq}};
  };
  for (uint16_t i = 1; i <= 10; ++i)
    EXPECT_EQ(q.Enqueue(packet(i), now), DatagramEnqueueResult::kQueued);
  EXPECT_EQ(q.Enqueue(packet(1), now), DatagramEnqueueResult::kDropped);
  EXPECT_TRUE(q.Pop(now));
  EXPECT_TRUE(q.Pop(now));
  EXPECT_FALSE(q.Pop(now));
  EXPECT_EQ(q.size(), 8u);
  EXPECT_EQ(q.recovery_budget_drops(), 0u);
  for (int i = 0; i < 8; ++i) {
    now += TimeDelta::Millis(120);
    EXPECT_TRUE(q.Pop(now));
  }
  EXPECT_EQ(q.size(), 0u);
}

TEST(BoundedDatagramQueueTest, TerminalIsExactlyOnceAcrossCopiesAndPurge) {
  int sent = 0, cancelled = 0;
  auto completion = std::make_shared<RoqDatagramCompletion>(
      [&](bool ok, int64_t) { ok ? ++sent : ++cancelled; });
  BoundedDatagramQueue q(5, 1200);
  auto now = Timestamp::Millis(100);
  q.Enqueue({.bytes = {1}, .binding_id = "v", .completion = completion}, now);
  EXPECT_EQ(cancelled, 0);
  q.PurgeBinding("v");
  EXPECT_EQ(cancelled, 1);
  completion->Complete(true, 123);
  completion.reset();
  EXPECT_EQ(sent, 0);
  EXPECT_EQ(cancelled, 1);
}

TEST(BoundedDatagramQueueTest, LatestReportRetainsServiceAgeAndAudioPriority) {
  BoundedDatagramQueue q(5, 1200);
  auto now = Timestamp::Millis(100);
  auto report = [](uint8_t value) {
    return QueuedDatagram{.bytes = {value},
                          .binding_id = "v",
                          .flow_id = 2,
                          .priority = DatagramPriority::kPeriodicRtcp,
                          .periodic_rtcp_key = {201, 7}};
  };
  q.Enqueue(report(1), now);
  q.Enqueue(report(2), now + TimeDelta::Millis(100));
  q.Enqueue({.bytes = {3}, .priority = DatagramPriority::kVideoDelta}, now);
  q.Enqueue({.bytes = {4}, .priority = DatagramPriority::kAudio}, now);
  EXPECT_EQ(q.coalesced_reports(), 1u);
  EXPECT_EQ(q.size(), 3u);
  EXPECT_EQ(q.Pop(now + TimeDelta::Millis(101))->bytes[0], 4);
  EXPECT_EQ(q.Pop(now + TimeDelta::Millis(101))->bytes[0], 2);
  EXPECT_EQ(q.Pop(now + TimeDelta::Millis(101))->bytes[0], 3);
}

TEST(BoundedDatagramQueueTest,
     RepairLifetimeIsNativeDeadlineNotPrimaryVideoAge) {
  BoundedDatagramQueue q(4, 1200);
  int cancelled = 0;
  auto completion =
      std::make_shared<RoqDatagramCompletion>([&](bool sent, int64_t) {
        EXPECT_FALSE(sent);
        ++cancelled;
      });
  const auto now = Timestamp::Seconds(1);
  ASSERT_EQ(q.Enqueue({.bytes = {1},
                       .binding_id = "v",
                       .enqueue_time = now,
                       .deadline = now + TimeDelta::Seconds(3),
                       .original_packet_id = RoqPacketId{7, 9},
                       .completion = completion},
                      now),
            DatagramEnqueueResult::kQueued);
  EXPECT_EQ(q.NextVideoExpiry(), now + TimeDelta::Seconds(3));
  auto slot = q.Pop(now + TimeDelta::Seconds(2));
  ASSERT_TRUE(slot);
  EXPECT_EQ(cancelled, 0);
  EXPECT_TRUE(q.TrimVideo(now + TimeDelta::Seconds(3), &*slot));
  EXPECT_EQ(cancelled, 1);
  slot.reset();
  completion.reset();
  EXPECT_EQ(cancelled, 1);
  EXPECT_EQ(q.expired_video_frames(), 0u);
}

}  // namespace
}  // namespace webrtc::roq
