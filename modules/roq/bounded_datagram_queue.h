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
#include <tuple>
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
  std::function<void()> on_submitted;
  uint32_t rtp_timestamp = 0;
  size_t primary_payload_bytes = 0;
  std::vector<uint8_t> periodic_rtcp_key;
  std::shared_ptr<RoqDatagramCompletion> completion;
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

// Diagnostic categories retain key-video identity even though primary video
// uses one FIFO scheduling priority. Counters describe this queue, not wire
// delivery; the optional quiche slot is counted separately.
struct DatagramClassStats {
  uint64_t enqueued_packets = 0;
  uint64_t enqueued_bytes = 0;
  uint64_t submitted_packets = 0;
  uint64_t submitted_bytes = 0;
  uint64_t dropped_packets = 0;
  uint64_t dropped_bytes = 0;
  uint64_t purged_packets = 0;
  size_t queued_packets = 0;
  size_t queued_bytes = 0;
  size_t slot_packets = 0;
  int64_t oldest_age_us = 0;
};
using DatagramQueueDiagnostics = std::array<DatagramClassStats, 6>;

class BoundedDatagramQueue {
 public:
  static constexpr TimeDelta kVideoHighWater = TimeDelta::Millis(500);
  static constexpr TimeDelta kVideoLowWater = TimeDelta::Millis(200);
  BoundedDatagramQueue(size_t capacity,
                       size_t max_datagram_size,
                       size_t byte_capacity = 512 * 1024);

  DatagramEnqueueResult Enqueue(QueuedDatagram datagram, Timestamp now);
  std::optional<QueuedDatagram> Pop();
  std::optional<QueuedDatagram> Pop(Timestamp now);
  void Requeue(QueuedDatagram datagram);
  void RecordSubmitted(const QueuedDatagram& datagram, Timestamp now);
  const QueuedDatagram* front() const {
    return queue_.empty() ? nullptr : &queue_.front();
  }
  size_t SetMaxDatagramSize(size_t max_datagram_size, Timestamp now);
  size_t Clear();
  // RTX service uses the recovery share; bursts remain in the bounded queue.
  // No packet is rejected based on original-send eligibility.
  void SetRecoveryRate(uint64_t bytes_per_second, Timestamp now);
  Timestamp NextRecoveryService(Timestamp now);
  size_t recovery_budget_drops() const { return recovery_budget_drops_; }
  size_t coalesced_reports() const { return coalesced_reports_; }
  size_t PurgeBinding(const std::string& binding_id);
  void RecordDropped(const QueuedDatagram& datagram, Timestamp now);
  void DiscardFrame(const QueuedDatagram& datagram, Timestamp now);
  void RecordInvalid() { ++invalid_drops_; }
  // slot is quiche's still-unpacketized DATAGRAM, not a pending UDP packet.
  // Returns true when that slot must be purged before quiche_conn_send.
  bool TrimVideo(Timestamp now, const QueuedDatagram* slot = nullptr);
  Timestamp NextVideoExpiry(const QueuedDatagram* slot = nullptr) const;
  TimeDelta OldestVideoAge(Timestamp now,
                           const QueuedDatagram* slot = nullptr) const;
  DatagramQueueDiagnostics Diagnostics(
      Timestamp now,
      const QueuedDatagram* slot = nullptr) const;

  size_t size() const { return queue_.size(); }
  size_t peak_size() const { return peak_size_; }
  size_t byte_size() const { return byte_size_; }
  size_t peak_byte_size() const { return peak_byte_size_; }
  size_t queue_full_drops() const { return queue_full_drops_; }
  size_t oversize_drops() const { return oversize_drops_; }
  size_t pmtu_purge_drops() const { return pmtu_purge_drops_; }
  size_t expired_drops() const { return expired_drops_; }
  size_t expired_video_frames() const { return expired_video_frames_; }
  TimeDelta max_video_queue_age() const { return max_video_queue_age_; }
  size_t invalid_drops() const { return invalid_drops_; }
  size_t dependency_drops() const { return 0; }
  size_t recovery_requests() const { return recovery_requests_; }
  TimeDelta max_queue_delay() const { return max_queue_delay_; }
  size_t dropped_by_priority(DatagramPriority priority) const {
    return dropped_by_priority_[static_cast<size_t>(priority)];
  }
  DatagramBindingStats BindingStats(const std::string& binding_id) const;

 private:
  using VideoStream = std::tuple<std::string, uint64_t, uint32_t>;
  using VideoFrame = std::tuple<VideoStream, uint32_t>;
  static bool IsPrimaryVideo(const QueuedDatagram& d);
  static size_t DiagnosticClass(const QueuedDatagram& d);
  DatagramQueueDiagnostics diagnostic_totals_ = {};
  static VideoStream Stream(const QueuedDatagram& d);
  static VideoFrame Frame(const QueuedDatagram& d);
  bool IsExpiredFrame(const QueuedDatagram& d) const;
  void ExpireVideo(const QueuedDatagram& d, Timestamp now);
  std::map<VideoFrame, Timestamp> expired_frames_;
  std::map<std::string, Timestamp> last_video_recovery_;
  size_t expired_drops_ = 0;
  size_t expired_video_frames_ = 0;
  TimeDelta max_video_queue_age_ = TimeDelta::Zero();
  void RefillRecovery(Timestamp now);
  bool RecoveryEligible(const QueuedDatagram& datagram) const;
  std::optional<uint64_t> recovery_rate_;
  Timestamp recovery_updated_ = Timestamp::MinusInfinity();
  double recovery_tokens_ = 0;
  size_t recovery_budget_drops_ = 0;
  size_t coalesced_reports_ = 0;
  const size_t capacity_;
  const size_t byte_capacity_;
  size_t max_datagram_size_;
  // At most one damaged access unit per binding; late fragments are discarded.
  std::map<std::string, DatagramFrameId> damaged_frames_;
  size_t recovery_requests_ = 0;
  void RetireFrame(const QueuedDatagram& datagram);
  std::deque<QueuedDatagram> queue_;
  std::array<std::string, 6> last_binding_by_priority_;
  std::array<std::string, 6> current_binding_by_priority_;
  std::map<std::string, size_t> deficits_;
  std::map<std::string, DatagramBindingStats> binding_stats_;
  size_t peak_size_ = 0;
  size_t byte_size_ = 0;
  size_t peak_byte_size_ = 0;
  size_t queue_full_drops_ = 0;
  size_t oversize_drops_ = 0;
  size_t pmtu_purge_drops_ = 0;
  size_t invalid_drops_ = 0;
  TimeDelta max_queue_delay_ = TimeDelta::Zero();
  std::array<size_t, 6> dropped_by_priority_ = {};
};

}  // namespace webrtc::roq

#endif  // MODULES_ROQ_BOUNDED_DATAGRAM_QUEUE_H_
