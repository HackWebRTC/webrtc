/* Copyright (c) 2026 The WebRTC project authors. */
#include "modules/roq/bounded_datagram_queue.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace webrtc::roq {
BoundedDatagramQueue::BoundedDatagramQueue(size_t capacity,
                                           size_t max_datagram_size,
                                           size_t byte_capacity)
    : capacity_(capacity),
      byte_capacity_(byte_capacity),
      max_datagram_size_(max_datagram_size) {}

namespace {
std::string SchedulerBinding(const QueuedDatagram& d) {
  return d.binding_id.empty() ? std::to_string(d.flow_id) : d.binding_id;
}
}  // namespace
bool BoundedDatagramQueue::IsPrimaryVideo(const QueuedDatagram& d) {
  return !d.original_packet_id &&
         (d.priority == DatagramPriority::kVideoDelta ||
          d.priority == DatagramPriority::kVideoKey);
}
size_t BoundedDatagramQueue::DiagnosticClass(const QueuedDatagram& d) {
  if (d.original_packet_id)
    return static_cast<size_t>(DatagramPriority::kRetransmission);
  if (IsPrimaryVideo(d) &&
      (d.is_key || d.contains_vps || d.contains_sps || d.contains_pps))
    return static_cast<size_t>(DatagramPriority::kVideoKey);
  return static_cast<size_t>(d.priority);
}
DatagramQueueDiagnostics BoundedDatagramQueue::Diagnostics(
    Timestamp now,
    const QueuedDatagram* slot) const {
  auto result = diagnostic_totals_;
  auto observe = [&](const QueuedDatagram& d, bool in_slot) {
    auto& stats = result[DiagnosticClass(d)];
    if (in_slot) {
      ++stats.slot_packets;
    } else {
      ++stats.queued_packets;
      stats.queued_bytes += d.bytes.size();
    }
    if (d.enqueue_time.IsFinite())
      stats.oldest_age_us =
          std::max(stats.oldest_age_us, (now - d.enqueue_time).us());
  };
  for (const auto& d : queue_)
    observe(d, false);
  if (slot)
    observe(*slot, true);
  return result;
}
BoundedDatagramQueue::VideoStream BoundedDatagramQueue::Stream(
    const QueuedDatagram& d) {
  return {SchedulerBinding(d), d.flow_id, d.frame_id ? d.frame_id->ssrc : 0};
}
BoundedDatagramQueue::VideoFrame BoundedDatagramQueue::Frame(
    const QueuedDatagram& d) {
  return {Stream(d), d.frame_id->timestamp};
}
bool BoundedDatagramQueue::IsExpiredFrame(const QueuedDatagram& d) const {
  return IsPrimaryVideo(d) && d.frame_id && expired_frames_.contains(Frame(d));
}
void BoundedDatagramQueue::ExpireVideo(const QueuedDatagram& d, Timestamp now) {
  ++expired_drops_;
  RecordDropped(d, now);
  if (d.frame_id && !expired_frames_.contains(Frame(d))) {
    // Bounded late-fragment tombstones, independent of subsequent timestamps.
    if (expired_frames_.size() >= 4096) {
      auto oldest = std::min_element(
          expired_frames_.begin(), expired_frames_.end(),
          [](const auto& a, const auto& b) { return a.second < b.second; });
      expired_frames_.erase(oldest);
    }
    expired_frames_.insert_or_assign(Frame(d), now + TimeDelta::Seconds(5));
    ++expired_video_frames_;
  }
  const auto binding = SchedulerBinding(d);
  auto last = last_video_recovery_.find(binding);
  if (last == last_video_recovery_.end() ||
      now - last->second >= TimeDelta::Seconds(1)) {
    last_video_recovery_.insert_or_assign(binding, now);
    ++recovery_requests_;
    ++binding_stats_[binding].recovery_requests;
  }
}
TimeDelta BoundedDatagramQueue::OldestVideoAge(
    Timestamp now,
    const QueuedDatagram* slot) const {
  TimeDelta age = TimeDelta::Zero();
  auto observe = [&](const QueuedDatagram& d) {
    if (IsPrimaryVideo(d) && d.enqueue_time.IsFinite())
      age = std::max(age, now - d.enqueue_time);
  };
  for (const auto& d : queue_)
    observe(d);
  if (slot)
    observe(*slot);
  return age;
}
Timestamp BoundedDatagramQueue::NextVideoExpiry(
    const QueuedDatagram* slot) const {
  Timestamp next = Timestamp::PlusInfinity();
  auto observe = [&](const QueuedDatagram& d) {
    if (IsPrimaryVideo(d) && d.enqueue_time.IsFinite())
      next = std::min(next, d.enqueue_time + kVideoHighWater);
    if (d.original_packet_id && d.deadline.IsFinite())
      next = std::min(next, d.deadline);
  };
  for (const auto& d : queue_)
    observe(d);
  if (slot)
    observe(*slot);
  return next;
}
bool BoundedDatagramQueue::TrimVideo(Timestamp now,
                                     const QueuedDatagram* slot) {
  std::erase_if(expired_frames_,
                [&](const auto& item) { return item.second <= now; });
  std::erase_if(last_video_recovery_, [&](const auto& item) {
    return now - item.second > TimeDelta::Seconds(5);
  });
  max_video_queue_age_ =
      std::max(max_video_queue_age_, OldestVideoAge(now, slot));
  std::set<VideoStream> congested;
  std::set<VideoFrame> frames;
  auto high = [&](const QueuedDatagram& d) {
    if (IsPrimaryVideo(d) && d.enqueue_time.IsFinite() &&
        now - d.enqueue_time >= kVideoHighWater)
      congested.insert(Stream(d));
  };
  for (const auto& d : queue_)
    high(d);
  if (slot)
    high(*slot);
  auto old = [&](const QueuedDatagram& d) {
    return IsPrimaryVideo(d) && d.enqueue_time.IsFinite() &&
           congested.contains(Stream(d)) &&
           now - d.enqueue_time >= kVideoLowWater;
  };
  auto select_frame = [&](const QueuedDatagram& d) {
    if (d.frame_id && old(d))
      frames.insert(Frame(d));
  };
  for (const auto& d : queue_)
    select_frame(d);
  if (slot)
    select_frame(*slot);
  auto drop = [&](const QueuedDatagram& d) {
    return old(d) || IsExpiredFrame(d) ||
           (IsPrimaryVideo(d) && d.frame_id && frames.contains(Frame(d)));
  };
  // Use RTP history's own repair lifetime, not the primary-video age gate.
  auto stale_repair = [&](const QueuedDatagram& d) {
    return d.original_packet_id && d.deadline.IsFinite() && now >= d.deadline;
  };
  const bool purge_slot = slot && (drop(*slot) || stale_repair(*slot));
  if (purge_slot) {
    if (stale_repair(*slot))
      RecordDropped(*slot, now);
    else
      ExpireVideo(*slot, now);
  }
  std::erase_if(queue_, [&](const QueuedDatagram& d) {
    if (!drop(d) && !stale_repair(d))
      return false;
    byte_size_ -= d.bytes.size();
    if (stale_repair(d))
      RecordDropped(d, now);
    else
      ExpireVideo(d, now);
    return true;
  });
  return purge_slot;
}
void BoundedDatagramQueue::RefillRecovery(Timestamp now) {
  if (!recovery_rate_)
    return;
  const double burst =
      std::max<double>(2 * max_datagram_size_, *recovery_rate_ / 10.0);
  if (recovery_updated_.IsFinite())
    recovery_tokens_ +=
        std::max(0.0, (now - recovery_updated_).seconds<double>()) *
        *recovery_rate_;
  recovery_tokens_ = std::min(recovery_tokens_, burst);
  recovery_updated_ = now;
}
bool BoundedDatagramQueue::RecoveryEligible(const QueuedDatagram& d) const {
  return !d.original_packet_id || !recovery_rate_ ||
         (*recovery_rate_ > 0 && recovery_tokens_ >= d.bytes.size() + 96);
}
Timestamp BoundedDatagramQueue::NextRecoveryService(Timestamp now) {
  RefillRecovery(now);
  Timestamp next = Timestamp::PlusInfinity();
  if (!recovery_rate_ || *recovery_rate_ == 0)
    return next;
  for (const auto& d : queue_) {
    if (!d.original_packet_id)
      continue;
    const double missing = d.bytes.size() + 96 - recovery_tokens_;
    // An eligible packet is waiting on QUIC, not on recovery credit.
    if (missing <= 0)
      return Timestamp::PlusInfinity();
    next = std::min(next,
                    now + TimeDelta::Micros(static_cast<int64_t>(
                              std::ceil(missing * 1000000 / *recovery_rate_))));
  }
  return next;
}
void BoundedDatagramQueue::SetRecoveryRate(uint64_t rate, Timestamp now) {
  if (recovery_rate_) {
    RefillRecovery(now);
  } else {
    recovery_tokens_ = 2 * max_datagram_size_;
  }
  recovery_rate_ = rate;
  RefillRecovery(now);
}
DatagramEnqueueResult BoundedDatagramQueue::Enqueue(QueuedDatagram d,
                                                    Timestamp now) {
  TrimVideo(now);
  if (d.original_packet_id && d.deadline.IsFinite() && now >= d.deadline) {
    RecordDropped(d, now);
    return DatagramEnqueueResult::kDropped;
  }
  if (d.original_packet_id &&
      std::any_of(queue_.begin(), queue_.end(), [&](const auto& queued) {
        return queued.binding_id == d.binding_id &&
               queued.flow_id == d.flow_id &&
               queued.original_packet_id == d.original_packet_id &&
               queued.rtp_timestamp == d.rtp_timestamp;
      })) {
    RecordDropped(d, now);
    return DatagramEnqueueResult::kDropped;
  }
  if (IsExpiredFrame(d)) {
    ++expired_drops_;
    RecordDropped(d, now);
    return DatagramEnqueueResult::kDropped;
  }
  if (d.frame_id) {
    const auto damaged = damaged_frames_.find(SchedulerBinding(d));
    if (damaged != damaged_frames_.end()) {
      if (damaged->second == *d.frame_id) {
        RecordDropped(d, now);
        return DatagramEnqueueResult::kDropped;
      }
      damaged_frames_.erase(damaged);
    }
  }
  if (d.bytes.empty()) {
    ++invalid_drops_;
    return DatagramEnqueueResult::kInvalid;
  }
  if (d.bytes.size() > max_datagram_size_) {
    ++oversize_drops_;
    if (d.frame_id) {
      RetireFrame(d);
      std::erase_if(queue_, [&](const QueuedDatagram& queued) {
        if (queued.frame_id != d.frame_id)
          return false;
        byte_size_ -= queued.bytes.size();
        RecordDropped(queued, now);
        return true;
      });
    }
    RecordDropped(d, now);
    return DatagramEnqueueResult::kOversize;
  }
  if (d.priority == DatagramPriority::kVideoKey)
    d.priority = DatagramPriority::kVideoDelta;
  if (d.original_packet_id)
    d.priority = DatagramPriority::kRetransmission;
  RefillRecovery(now);
  if (!d.periodic_rtcp_key.empty() &&
      d.priority == DatagramPriority::kPeriodicRtcp) {
    auto old =
        std::find_if(queue_.begin(), queue_.end(), [&](const auto& queued) {
          return queued.priority == DatagramPriority::kPeriodicRtcp &&
                 queued.binding_id == d.binding_id &&
                 queued.flow_id == d.flow_id &&
                 queued.periodic_rtcp_key == d.periodic_rtcp_key;
        });
    if (old != queue_.end()) {
      // Preserve waiting age so continuous replacement cannot postpone service.
      d.enqueue_time = old->enqueue_time;
      byte_size_ -= old->bytes.size();
      RecordDropped(*old, now);
      queue_.erase(old);
      ++coalesced_reports_;
    }
  }
  auto fits = [&](size_t packets, size_t bytes) {
    return packets < capacity_ && d.bytes.size() <= byte_capacity_ - bytes;
  };
  if (!fits(queue_.size(), byte_size_)) {
    size_t removable = 0, bytes = 0;
    if (d.priority >= DatagramPriority::kRetransmission) {
      for (const auto& queued : queue_) {
        if (queued.priority < d.priority) {
          ++removable;
          bytes += queued.bytes.size();
        }
      }
    }
    if (!fits(queue_.size() - removable, byte_size_ - bytes)) {
      ++queue_full_drops_;
      RecordDropped(d, now);
      return DatagramEnqueueResult::kQueueFull;
    }
    while (!fits(queue_.size(), byte_size_)) {
      auto victim = queue_.end();
      for (auto it = queue_.end(); it != queue_.begin();) {
        --it;
        if (it->priority < d.priority &&
            (victim == queue_.end() || it->priority < victim->priority))
          victim = it;
      }
      ++queue_full_drops_;
      byte_size_ -= victim->bytes.size();
      RecordDropped(*victim, now);
      queue_.erase(victim);
    }
  }
  if (!d.enqueue_time.IsFinite())
    d.enqueue_time = now;
  if (d.priority == DatagramPriority::kVideoKey)
    d.priority = DatagramPriority::kVideoDelta;
  ++binding_stats_[SchedulerBinding(d)].enqueued_packets;
  auto& diagnostic = diagnostic_totals_[DiagnosticClass(d)];
  ++diagnostic.enqueued_packets;
  diagnostic.enqueued_bytes += d.bytes.size();
  byte_size_ += d.bytes.size();
  queue_.push_back(std::move(d));
  peak_size_ = std::max(peak_size_, queue_.size());
  peak_byte_size_ = std::max(peak_byte_size_, byte_size_);
  return DatagramEnqueueResult::kQueued;
}
std::optional<QueuedDatagram> BoundedDatagramQueue::Pop() {
  return Pop(Timestamp::Zero());
}
std::optional<QueuedDatagram> BoundedDatagramQueue::Pop(Timestamp now) {
  TrimVideo(now);
  RefillRecovery(now);
  if (queue_.empty())
    return std::nullopt;
  DatagramPriority highest = DatagramPriority::kPeriodicRtcp;
  bool found = false;
  for (const QueuedDatagram& datagram : queue_) {
    if (!RecoveryEligible(datagram))
      continue;
    if (!found || datagram.priority > highest) {
      highest = datagram.priority;
      found = true;
    }
  }
  if (!found)
    return std::nullopt;
  // Low frequency reports get one opportunity after 100 ms, but never
  // preempt audio or critical feedback. Unlike strict priority, this cannot
  // starve indefinitely behind primary video or retransmissions.
  if (highest < DatagramPriority::kAudio &&
      std::any_of(
          queue_.begin(), queue_.end(),
          [&](const auto& d) {
            return d.priority == DatagramPriority::kPeriodicRtcp &&
                   now - d.enqueue_time >= TimeDelta::Millis(100);
          }))
    highest = DatagramPriority::kPeriodicRtcp;

  std::vector<std::string> bindings;
  for (const QueuedDatagram& datagram : queue_) {
    if (datagram.priority != highest || !RecoveryEligible(datagram))
      continue;
    const std::string binding = SchedulerBinding(datagram);
    if (std::find(bindings.begin(), bindings.end(), binding) == bindings.end())
      bindings.push_back(binding);
  }
  const size_t priority = static_cast<size_t>(highest);
  auto selected = queue_.end();
  std::string selected_binding;
  auto find_head = [&](const std::string& binding) {
    auto head = std::find_if(
        queue_.begin(), queue_.end(), [&](const QueuedDatagram& d) {
          return d.priority == highest && SchedulerBinding(d) == binding &&
                 RecoveryEligible(d);
        });
    if (head != queue_.end() &&
        (head->bytes.size() > deficits_[binding] || !RecoveryEligible(*head)))
      return queue_.end();
    return head;
  };
  const std::string& current = current_binding_by_priority_[priority];
  if (!current.empty()) {
    selected = find_head(current);
    if (selected != queue_.end())
      selected_binding = current;
  }
  if (selected == queue_.end()) {
    current_binding_by_priority_[priority].clear();
    size_t start = 0;
    if (!last_binding_by_priority_[priority].empty()) {
      const auto last = std::find(bindings.begin(), bindings.end(),
                                  last_binding_by_priority_[priority]);
      if (last != bindings.end()) {
        start = (static_cast<size_t>(last - bindings.begin()) + 1) %
                bindings.size();
      }
    }
    for (size_t offset = 0; offset < bindings.size(); ++offset) {
      const std::string& binding = bindings[(start + offset) % bindings.size()];
      deficits_[binding] += max_datagram_size_;
      selected = find_head(binding);
      if (selected != queue_.end()) {
        selected_binding = binding;
        break;
      }
    }
  }
  if (selected == queue_.end())
    return std::nullopt;
  QueuedDatagram datagram = std::move(*selected);
  if (datagram.original_packet_id && recovery_rate_)
    recovery_tokens_ -= datagram.bytes.size() + 96;
  byte_size_ -= datagram.bytes.size();
  queue_.erase(selected);
  deficits_[selected_binding] -= datagram.bytes.size();
  const bool can_continue = find_head(selected_binding) != queue_.end();
  if (can_continue) {
    current_binding_by_priority_[priority] = selected_binding;
  } else {
    current_binding_by_priority_[priority].clear();
    last_binding_by_priority_[priority] = selected_binding;
  }
  max_queue_delay_ = std::max(max_queue_delay_, now - datagram.enqueue_time);
  return datagram;
}

// Called under the session lock held since Pop. Producers cannot use the
// vacated capacity while quiche is deciding whether to accept this packet.
void BoundedDatagramQueue::Requeue(QueuedDatagram d) {
  if (d.original_packet_id && recovery_rate_)
    recovery_tokens_ += d.bytes.size() + 96;
  byte_size_ += d.bytes.size();
  queue_.push_front(std::move(d));
}
void BoundedDatagramQueue::RecordSubmitted(const QueuedDatagram& d, Timestamp) {
  ++binding_stats_[SchedulerBinding(d)].submitted_packets;
  auto& diagnostic = diagnostic_totals_[DiagnosticClass(d)];
  ++diagnostic.submitted_packets;
  diagnostic.submitted_bytes += d.bytes.size();
}
void BoundedDatagramQueue::RecordDropped(const QueuedDatagram& d, Timestamp) {
  if (d.completion)
    d.completion->Complete(false);
  ++binding_stats_[SchedulerBinding(d)].dropped_packets;
  ++dropped_by_priority_[static_cast<size_t>(d.priority)];
  auto& diagnostic = diagnostic_totals_[DiagnosticClass(d)];
  ++diagnostic.dropped_packets;
  diagnostic.dropped_bytes += d.bytes.size();
}
size_t BoundedDatagramQueue::SetMaxDatagramSize(size_t size, Timestamp now) {
  max_datagram_size_ = size;
  std::vector<DatagramFrameId> damaged;
  for (const auto& d : queue_) {
    if (d.bytes.size() > size && d.frame_id) {
      damaged.push_back(*d.frame_id);
      RetireFrame(d);
    }
  }
  const size_t before = queue_.size();
  std::erase_if(queue_, [&](const QueuedDatagram& d) {
    if (d.bytes.size() <= size &&
        (!d.frame_id || std::find(damaged.begin(), damaged.end(),
                                  *d.frame_id) == damaged.end()))
      return false;
    byte_size_ -= d.bytes.size();
    RecordDropped(d, now);
    return true;
  });
  pmtu_purge_drops_ += before - queue_.size();
  return before - queue_.size();
}
size_t BoundedDatagramQueue::Clear() {
  const size_t count = queue_.size();
  for (const auto& d : queue_) {
    RetireFrame(d);
    ++diagnostic_totals_[DiagnosticClass(d)].purged_packets;
    if (d.completion)
      d.completion->Complete(false);
  }
  queue_.clear();
  byte_size_ = 0;
  deficits_.clear();
  last_binding_by_priority_.fill({});
  current_binding_by_priority_.fill({});
  return count;
}
void BoundedDatagramQueue::RetireFrame(const QueuedDatagram& d) {
  if (!d.frame_id)
    return;
  const auto key = SchedulerBinding(d);
  const auto previous = damaged_frames_.find(key);
  if (previous == damaged_frames_.end() || previous->second != *d.frame_id) {
    ++recovery_requests_;
    ++binding_stats_[key].recovery_requests;
  }
  damaged_frames_[key] = *d.frame_id;
}
void BoundedDatagramQueue::DiscardFrame(const QueuedDatagram& d,
                                        Timestamp now) {
  if (!d.frame_id) {
    RecordDropped(d, now);
    return;
  }
  RetireFrame(d);
  std::erase_if(queue_, [&](const QueuedDatagram& queued) {
    if (queued.frame_id != d.frame_id)
      return false;
    byte_size_ -= queued.bytes.size();
    RecordDropped(queued, now);
    return true;
  });
  RecordDropped(d, now);
}
size_t BoundedDatagramQueue::PurgeBinding(const std::string& id) {
  const size_t before = queue_.size();
  std::erase_if(queue_, [&](const QueuedDatagram& d) {
    if (d.binding_id != id)
      return false;
    byte_size_ -= d.bytes.size();
    ++diagnostic_totals_[DiagnosticClass(d)].purged_packets;
    if (d.completion)
      d.completion->Complete(false);
    return true;
  });
  deficits_.erase(id);
  last_video_recovery_.erase(id);
  std::erase_if(expired_frames_, [&](const auto& item) {
    return std::get<0>(std::get<0>(item.first)) == id;
  });
  damaged_frames_.erase(id);
  return before - queue_.size();
}
DatagramBindingStats BoundedDatagramQueue::BindingStats(
    const std::string& binding_id) const {
  DatagramBindingStats result;
  if (const auto it = binding_stats_.find(binding_id);
      it != binding_stats_.end()) {
    result = it->second;
    result.queued_packets = 0;
    result.queued_bytes = 0;
  }
  for (const QueuedDatagram& datagram : queue_) {
    if (SchedulerBinding(datagram) == binding_id) {
      ++result.queued_packets;
      result.queued_bytes += datagram.bytes.size();
    }
  }
  return result;
}

}  // namespace webrtc::roq
