/* Copyright (c) 2026 The WebRTC project authors. */
#include "modules/roq/bounded_datagram_queue.h"

#include <algorithm>
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
DatagramEnqueueResult BoundedDatagramQueue::Enqueue(QueuedDatagram d,
                                                    Timestamp now) {
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
  if (queue_.empty())
    return std::nullopt;
  DatagramPriority highest = DatagramPriority::kPeriodicRtcp;
  bool found = false;
  for (const QueuedDatagram& datagram : queue_) {
    if (!found || datagram.priority > highest) {
      highest = datagram.priority;
      found = true;
    }
  }
  if (!found)
    return std::nullopt;

  std::vector<std::string> bindings;
  for (const QueuedDatagram& datagram : queue_) {
    if (datagram.priority != highest)
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
          return d.priority == highest && SchedulerBinding(d) == binding;
        });
    if (head != queue_.end() && head->bytes.size() > deficits_[binding])
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
  byte_size_ += d.bytes.size();
  queue_.push_front(std::move(d));
}
void BoundedDatagramQueue::RecordSubmitted(const QueuedDatagram& d, Timestamp) {
  ++binding_stats_[SchedulerBinding(d)].submitted_packets;
}
void BoundedDatagramQueue::RecordDropped(const QueuedDatagram& d, Timestamp) {
  ++binding_stats_[SchedulerBinding(d)].dropped_packets;
  ++dropped_by_priority_[static_cast<size_t>(d.priority)];
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
  for (const auto& d : queue_)
    RetireFrame(d);
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
    return true;
  });
  deficits_.erase(id);
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
