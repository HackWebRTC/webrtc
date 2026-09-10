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
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "api/roq/roq_session.h"
#include "api/units/timestamp.h"

namespace webrtc::roq {

enum class DatagramEnqueueResult {
  kQueued,
  kDropped,
  kQueueFull,
  kOversize,
  kInvalid
};

using DatagramPriority = RoqDatagramPriority;
using DatagramFrameId = RoqFrameId;
using DatagramPacketId = RoqPacketId;

struct QueuedDatagram {
  std::vector<uint8_t> bytes;
  std::string binding_id;
  uint64_t flow_id = 0;
  RoqMediaType media_type = RoqMediaType::kVideo;
  Timestamp enqueue_time = Timestamp::MinusInfinity();
  Timestamp deadline = Timestamp::PlusInfinity();
  DatagramPriority priority = DatagramPriority::kVideoDelta;
  std::optional<DatagramFrameId> frame_id;
  std::optional<DatagramPacketId> packet_id;
  std::optional<DatagramPacketId> original_packet_id;
  bool frame_complete = false;
  VideoCodecType video_codec = VideoCodecType::kVideoCodecGeneric;
  bool is_key = false;
  bool contains_vps = false;
  bool contains_sps = false;
  bool contains_pps = false;
  bool independent_frame = false;
  bool admitted = false;
  bool committed = false;
  std::function<void()> request_key_frame;
  std::function<void()> on_submitted;
};

struct DatagramBindingStats {
  size_t enqueued_packets = 0;
  size_t queued_packets = 0;
  size_t queued_bytes = 0;
  size_t submitted_packets = 0;
  size_t dropped_packets = 0;
  size_t dependency_drops = 0;
  size_t recovery_requests = 0;
};

class BoundedDatagramQueue {
 public:
  BoundedDatagramQueue(size_t capacity,
                       size_t max_datagram_size,
                       size_t byte_capacity = 512 * 1024);

  DatagramEnqueueResult Enqueue(QueuedDatagram datagram, Timestamp now);
  bool ReserveVideoFrame(QueuedDatagram frame,
                         size_t packet_count,
                         size_t byte_size,
                         Timestamp now);
  std::optional<QueuedDatagram> Pop();
  std::optional<QueuedDatagram> Pop(Timestamp now);
  void RecordSubmitted(const QueuedDatagram& datagram, Timestamp now);
  std::optional<Timestamp> FrameDeadline(const DatagramFrameId& frame_id) const;
  const QueuedDatagram* front() const {
    return queue_.empty() ? nullptr : &queue_.front();
  }
  size_t SetMaxDatagramSize(size_t max_datagram_size, Timestamp now);
  size_t PurgeExpired(Timestamp now);
  size_t Clear();
  size_t PurgeBinding(const std::string& binding_id);
  void RecordDropped(const QueuedDatagram& datagram, Timestamp now);
  std::vector<std::function<void()>> RecoveryCallbacks(Timestamp now,
                                                       TimeDelta rtt,
                                                       bool fresh);
  void RequireIndependentVideoFrame(const std::string& binding_id,
                                    uint32_t ssrc,
                                    Timestamp now);
  void RecordInvalid() { ++invalid_drops_; }

  size_t size() const { return queue_.size(); }
  size_t peak_size() const { return peak_size_; }
  size_t byte_size() const { return byte_size_; }
  size_t peak_byte_size() const { return peak_byte_size_; }
  size_t queue_full_drops() const { return queue_full_drops_; }
  size_t reserved_packets() const { return reserved_packets_; }
  size_t reserved_bytes() const { return reserved_bytes_; }
  size_t oversize_drops() const { return oversize_drops_; }
  size_t pmtu_purge_drops() const { return pmtu_purge_drops_; }
  size_t expired_drops() const { return expired_drops_; }
  size_t invalid_drops() const { return invalid_drops_; }
  size_t dependency_drops() const { return dependency_drops_; }
  size_t recovery_requests() const { return recovery_requests_; }
  TimeDelta max_queue_delay() const { return max_queue_delay_; }
  size_t dropped_by_priority(DatagramPriority priority) const {
    return dropped_by_priority_[static_cast<size_t>(priority)];
  }
  DatagramBindingStats BindingStats(const std::string& binding_id) const;

 private:
  struct SentPacketDeadline {
    DatagramPacketId packet_id;
    Timestamp repair_until;
    DatagramPriority priority;
    std::optional<DatagramFrameId> frame_id;
  };
  struct SentFrameDeadline {
    DatagramFrameId frame_id;
    Timestamp enqueue_time;
    Timestamp deadline;
    DatagramPriority priority;
  };
  struct RecoveryState {
    Timestamp last_drop = Timestamp::MinusInfinity();
    Timestamp last_request = Timestamp::MinusInfinity();
    std::function<void()> request_key_frame;
  };
  struct FrameReservation {
    DatagramFrameId frame_id;
    size_t remaining_packets = 0;
    size_t remaining_bytes = 0;
    DatagramPriority priority = DatagramPriority::kVideoDelta;
    Timestamp deadline = Timestamp::PlusInfinity();
    VideoCodecType video_codec = VideoCodecType::kVideoCodecGeneric;
    bool is_key = false;
    bool contains_vps = false;
    bool contains_sps = false;
    bool contains_pps = false;
    std::function<void()> request_key_frame;
  };

  size_t EraseFrame(const DatagramFrameId& frame_id);
  size_t EraseFrame(const DatagramFrameId& frame_id, bool record_drop);
  void MarkFrameDropped(const DatagramFrameId& frame_id, Timestamp now);
  bool IsBroken(const QueuedDatagram& datagram) const;
  void CompleteFrame(const DatagramFrameId& frame_id);
  bool IsFrameComplete(const QueuedDatagram& datagram) const;
  bool IsFrameStarted(const DatagramFrameId& frame_id) const;
  bool HasEarlierVideoFrame(const QueuedDatagram& datagram) const;
  void RecordDrop(DatagramPriority priority);
  void PruneSentPacketDeadlines(Timestamp now);

  const size_t capacity_;
  const size_t byte_capacity_;
  size_t max_datagram_size_;
  std::deque<QueuedDatagram> queue_;
  std::array<std::string, 5> last_binding_by_priority_;
  std::array<std::string, 5> current_binding_by_priority_;
  std::map<std::string, size_t> deficits_;
  std::map<std::string, DatagramBindingStats> binding_stats_;
  std::vector<DatagramFrameId> complete_frames_;
  std::vector<DatagramFrameId> started_frames_;
  std::vector<FrameReservation> frame_reservations_;
  size_t reserved_packets_ = 0;
  size_t reserved_bytes_ = 0;
  std::set<std::pair<std::string, uint32_t>> broken_video_;
  std::map<std::pair<std::string, uint32_t>, RecoveryState> recovery_;
  std::deque<SentPacketDeadline> sent_packet_deadlines_;
  std::deque<SentFrameDeadline> sent_frame_deadlines_;
  size_t peak_size_ = 0;
  size_t byte_size_ = 0;
  size_t peak_byte_size_ = 0;
  size_t queue_full_drops_ = 0;
  size_t oversize_drops_ = 0;
  size_t pmtu_purge_drops_ = 0;
  size_t expired_drops_ = 0;
  size_t invalid_drops_ = 0;
  size_t dependency_drops_ = 0;
  size_t recovery_requests_ = 0;
  TimeDelta max_queue_delay_ = TimeDelta::Zero();
  std::array<size_t, 5> dropped_by_priority_ = {};
};

}  // namespace webrtc::roq

#endif  // MODULES_ROQ_BOUNDED_DATAGRAM_QUEUE_H_
