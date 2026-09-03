/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef MODULES_ROQ_BOUNDED_DATAGRAM_QUEUE_H_
#define MODULES_ROQ_BOUNDED_DATAGRAM_QUEUE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <vector>

#include "api/roq/roq_session.h"
#include "api/units/timestamp.h"

namespace webrtc::roq {

enum class DatagramEnqueueResult { kQueued, kQueueFull, kOversize, kInvalid };

using DatagramPriority = RoqDatagramPriority;
using DatagramFrameId = RoqFrameId;
using DatagramPacketId = RoqPacketId;

struct QueuedDatagram {
  std::vector<uint8_t> bytes;
  uint64_t flow_id = 0;
  RoqMediaType media_type = RoqMediaType::kVideo;
  Timestamp enqueue_time = Timestamp::MinusInfinity();
  Timestamp deadline = Timestamp::PlusInfinity();
  DatagramPriority priority = DatagramPriority::kVideoDelta;
  std::optional<DatagramFrameId> frame_id;
  std::optional<DatagramPacketId> packet_id;
  std::optional<DatagramPacketId> original_packet_id;
  std::function<void()> on_submitted;
};

class BoundedDatagramQueue {
 public:
  BoundedDatagramQueue(size_t capacity, size_t max_datagram_size);

  DatagramEnqueueResult Enqueue(QueuedDatagram datagram, Timestamp now);
  std::optional<QueuedDatagram> Pop();
  std::optional<QueuedDatagram> Pop(Timestamp now);
  void RecordSubmitted(const QueuedDatagram& datagram);
  std::optional<Timestamp> FrameDeadline(const DatagramFrameId& frame_id) const;
  const QueuedDatagram* front() const {
    return queue_.empty() ? nullptr : &queue_.front();
  }
  size_t SetMaxDatagramSize(size_t max_datagram_size);
  size_t PurgeExpired(Timestamp now);
  size_t Clear();
  void RecordInvalid() { ++invalid_drops_; }

  size_t size() const { return queue_.size(); }
  size_t peak_size() const { return peak_size_; }
  size_t queue_full_drops() const { return queue_full_drops_; }
  size_t oversize_drops() const { return oversize_drops_; }
  size_t pmtu_purge_drops() const { return pmtu_purge_drops_; }
  size_t expired_drops() const { return expired_drops_; }
  size_t invalid_drops() const { return invalid_drops_; }
  TimeDelta max_queue_delay() const { return max_queue_delay_; }
  size_t dropped_by_priority(DatagramPriority priority) const {
    return dropped_by_priority_[static_cast<size_t>(priority)];
  }

 private:
  struct SentPacketDeadline {
    DatagramPacketId packet_id;
    Timestamp deadline;
    DatagramPriority priority;
    std::optional<DatagramFrameId> frame_id;
  };
  struct SentFrameDeadline {
    DatagramFrameId frame_id;
    Timestamp enqueue_time;
    Timestamp deadline;
    DatagramPriority priority;
  };

  size_t EraseFrame(const DatagramFrameId& frame_id);
  size_t EraseFrame(const DatagramFrameId& frame_id, bool record_drop);
  void RecordDrop(DatagramPriority priority);
  void PruneSentPacketDeadlines(Timestamp now);

  const size_t capacity_;
  size_t max_datagram_size_;
  std::deque<QueuedDatagram> queue_;
  std::deque<SentPacketDeadline> sent_packet_deadlines_;
  std::deque<SentFrameDeadline> sent_frame_deadlines_;
  size_t peak_size_ = 0;
  size_t queue_full_drops_ = 0;
  size_t oversize_drops_ = 0;
  size_t pmtu_purge_drops_ = 0;
  size_t expired_drops_ = 0;
  size_t invalid_drops_ = 0;
  TimeDelta max_queue_delay_ = TimeDelta::Zero();
  std::array<size_t, 5> dropped_by_priority_ = {};
};

}  // namespace webrtc::roq

#endif  // MODULES_ROQ_BOUNDED_DATAGRAM_QUEUE_H_
