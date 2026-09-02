/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/bounded_datagram_queue.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace webrtc::roq {

BoundedDatagramQueue::BoundedDatagramQueue(size_t capacity,
                                           size_t max_datagram_size)
    : capacity_(capacity), max_datagram_size_(max_datagram_size) {}

DatagramEnqueueResult BoundedDatagramQueue::Enqueue(
    std::span<const uint8_t> bytes) {
  return EnqueueLegacy(bytes, Timestamp::Zero());
}

DatagramEnqueueResult BoundedDatagramQueue::EnqueueLegacy(
    std::span<const uint8_t> bytes,
    Timestamp now) {
  if (bytes.size() > max_datagram_size_) {
    ++oversize_drops_;
    return DatagramEnqueueResult::kOversize;
  }
  if (queue_.size() >= capacity_) {
    ++queue_full_drops_;
    return DatagramEnqueueResult::kQueueFull;
  }
  queue_.push_back({.bytes = std::vector<uint8_t>(bytes.begin(), bytes.end()),
                    .enqueue_time = now});
  peak_size_ = std::max(peak_size_, queue_.size());
  return DatagramEnqueueResult::kQueued;
}

DatagramEnqueueResult BoundedDatagramQueue::Enqueue(QueuedDatagram datagram,
                                                    Timestamp now) {
  if (datagram.bytes.size() > max_datagram_size_) {
    ++oversize_drops_;
    RecordDrop(datagram.priority);
    return DatagramEnqueueResult::kOversize;
  }
  PurgeExpired(now);
  PruneSentPacketDeadlines(now);
  if (datagram.original_packet_id) {
    const auto original = std::find_if(
        sent_packet_deadlines_.begin(), sent_packet_deadlines_.end(),
        [&datagram](const SentPacketDeadline& sent) {
          return sent.packet_id == *datagram.original_packet_id;
        });
    if (original == sent_packet_deadlines_.end()) {
      ++invalid_drops_;
      return DatagramEnqueueResult::kInvalid;
    }
    if (original->deadline <= now) {
      ++expired_drops_;
      RecordDrop(original->priority);
      return DatagramEnqueueResult::kInvalid;
    }
    datagram.deadline = original->deadline;
    datagram.priority = original->priority;
    datagram.frame_id = original->frame_id;
  }
  if (datagram.deadline <= now) {
    ++expired_drops_;
    RecordDrop(datagram.priority);
    return DatagramEnqueueResult::kInvalid;
  }
  if (datagram.frame_id && !datagram.original_packet_id &&
      datagram.priority == DatagramPriority::kVideoKey) {
    Timestamp frame_enqueue_time = datagram.enqueue_time;
    auto submitted =
        std::find_if(sent_frame_deadlines_.begin(), sent_frame_deadlines_.end(),
                     [&datagram](const SentFrameDeadline& sent) {
                       return sent.frame_id == *datagram.frame_id;
                     });
    if (submitted != sent_frame_deadlines_.end()) {
      if (submitted->deadline <= now) {
        ++expired_drops_;
        return DatagramEnqueueResult::kInvalid;
      }
      frame_enqueue_time =
          std::min(frame_enqueue_time, submitted->enqueue_time);
    }
    for (const QueuedDatagram& queued : queue_) {
      if (queued.frame_id == datagram.frame_id)
        frame_enqueue_time = std::min(frame_enqueue_time, queued.enqueue_time);
    }
    const Timestamp frame_deadline =
        frame_enqueue_time + (datagram.deadline - datagram.enqueue_time);
    for (QueuedDatagram& queued : queue_) {
      if (queued.frame_id == datagram.frame_id) {
        queued.priority = DatagramPriority::kVideoKey;
        queued.deadline = frame_deadline;
      }
    }
    datagram.deadline = frame_deadline;
    if (submitted != sent_frame_deadlines_.end()) {
      submitted->deadline = frame_deadline;
      submitted->priority = DatagramPriority::kVideoKey;
    }
    for (SentPacketDeadline& sent : sent_packet_deadlines_) {
      if (sent.frame_id == datagram.frame_id) {
        sent.deadline = frame_deadline;
        sent.priority = DatagramPriority::kVideoKey;
      }
    }
  } else if (datagram.frame_id) {
    const auto frame =
        std::find_if(queue_.begin(), queue_.end(),
                     [&datagram](const QueuedDatagram& queued) {
                       return queued.frame_id == datagram.frame_id;
                     });
    if (frame != queue_.end() &&
        frame->priority == DatagramPriority::kVideoKey) {
      datagram.priority = frame->priority;
      datagram.deadline = frame->deadline;
    } else {
      const auto submitted = std::find_if(
          sent_frame_deadlines_.begin(), sent_frame_deadlines_.end(),
          [&datagram](const SentFrameDeadline& sent) {
            return sent.frame_id == *datagram.frame_id;
          });
      if (submitted != sent_frame_deadlines_.end()) {
        if (submitted->deadline <= now) {
          ++expired_drops_;
          return DatagramEnqueueResult::kInvalid;
        }
        datagram.priority = submitted->priority;
        datagram.deadline = submitted->deadline;
      }
    }
  }
  if (queue_.size() >= capacity_) {
    const auto victim = std::min_element(
        queue_.begin(), queue_.end(),
        [](const QueuedDatagram& left, const QueuedDatagram& right) {
          if (left.priority != right.priority)
            return left.priority < right.priority;
          return left.enqueue_time < right.enqueue_time;
        });
    if (victim == queue_.end() || datagram.priority < victim->priority) {
      ++queue_full_drops_;
      RecordDrop(datagram.priority);
      return DatagramEnqueueResult::kQueueFull;
    }
    size_t removed = 1;
    if (victim->frame_id) {
      removed = EraseFrame(*victim->frame_id, true);
    } else {
      RecordDrop(victim->priority);
      queue_.erase(victim);
    }
    queue_full_drops_ += removed;
  }
  queue_.push_back(std::move(datagram));
  peak_size_ = std::max(peak_size_, queue_.size());
  return DatagramEnqueueResult::kQueued;
}

std::optional<QueuedDatagram> BoundedDatagramQueue::Pop(Timestamp now) {
  PurgeExpired(now);
  if (queue_.empty())
    return std::nullopt;
  const auto selected = std::max_element(
      queue_.begin(), queue_.end(),
      [](const QueuedDatagram& left, const QueuedDatagram& right) {
        if (left.priority != right.priority)
          return left.priority < right.priority;
        return left.enqueue_time > right.enqueue_time;
      });
  QueuedDatagram datagram = std::move(*selected);
  queue_.erase(selected);
  max_queue_delay_ = std::max(max_queue_delay_, now - datagram.enqueue_time);
  return datagram;
}

void BoundedDatagramQueue::RecordSubmitted(const QueuedDatagram& datagram) {
  if (datagram.packet_id) {
    sent_packet_deadlines_.push_back({.packet_id = *datagram.packet_id,
                                      .deadline = datagram.deadline,
                                      .priority = datagram.priority,
                                      .frame_id = datagram.frame_id});
  }
  if (datagram.frame_id) {
    const auto frame =
        std::find_if(sent_frame_deadlines_.begin(), sent_frame_deadlines_.end(),
                     [&datagram](const SentFrameDeadline& sent) {
                       return sent.frame_id == *datagram.frame_id;
                     });
    if (frame == sent_frame_deadlines_.end()) {
      sent_frame_deadlines_.push_back({.frame_id = *datagram.frame_id,
                                       .enqueue_time = datagram.enqueue_time,
                                       .deadline = datagram.deadline,
                                       .priority = datagram.priority});
    } else if (datagram.priority == DatagramPriority::kVideoKey) {
      frame->deadline = datagram.deadline;
      frame->priority = datagram.priority;
    }
  }
}

std::optional<Timestamp> BoundedDatagramQueue::FrameDeadline(
    const DatagramFrameId& frame_id) const {
  for (const QueuedDatagram& datagram : queue_) {
    if (datagram.frame_id == frame_id)
      return datagram.deadline;
  }
  for (const SentFrameDeadline& sent : sent_frame_deadlines_) {
    if (sent.frame_id == frame_id)
      return sent.deadline;
  }
  return std::nullopt;
}

size_t BoundedDatagramQueue::PurgeExpired(Timestamp now) {
  std::vector<DatagramFrameId> expired_frames;
  for (const QueuedDatagram& datagram : queue_) {
    if (datagram.deadline <= now && datagram.frame_id &&
        std::find(expired_frames.begin(), expired_frames.end(),
                  *datagram.frame_id) == expired_frames.end()) {
      expired_frames.push_back(*datagram.frame_id);
    }
  }
  size_t removed = 0;
  for (const DatagramFrameId& frame_id : expired_frames)
    removed += EraseFrame(frame_id, true);
  const size_t old_size = queue_.size();
  std::erase_if(queue_, [this, now](const QueuedDatagram& datagram) {
    if (datagram.deadline > now)
      return false;
    RecordDrop(datagram.priority);
    return true;
  });
  removed += old_size - queue_.size();
  expired_drops_ += removed;
  return removed;
}

std::optional<QueuedDatagram> BoundedDatagramQueue::Pop() {
  if (queue_.empty()) {
    return std::nullopt;
  }
  QueuedDatagram datagram = std::move(queue_.front());
  queue_.pop_front();
  return datagram;
}

size_t BoundedDatagramQueue::SetMaxDatagramSize(size_t max_datagram_size) {
  max_datagram_size_ = max_datagram_size;
  std::vector<DatagramFrameId> oversize_frames;
  for (const QueuedDatagram& datagram : queue_) {
    if (datagram.bytes.size() > max_datagram_size && datagram.frame_id &&
        std::find(oversize_frames.begin(), oversize_frames.end(),
                  *datagram.frame_id) == oversize_frames.end()) {
      oversize_frames.push_back(*datagram.frame_id);
    }
  }
  size_t purged = 0;
  for (const DatagramFrameId& frame_id : oversize_frames)
    purged += EraseFrame(frame_id, true);
  const size_t old_size = queue_.size();
  std::erase_if(queue_,
                [this, max_datagram_size](const QueuedDatagram& datagram) {
                  if (datagram.bytes.size() <= max_datagram_size)
                    return false;
                  RecordDrop(datagram.priority);
                  return true;
                });
  purged += old_size - queue_.size();
  pmtu_purge_drops_ += purged;
  return purged;
}

size_t BoundedDatagramQueue::EraseFrame(const DatagramFrameId& frame_id) {
  return EraseFrame(frame_id, false);
}

size_t BoundedDatagramQueue::EraseFrame(const DatagramFrameId& frame_id,
                                        bool record_drop) {
  const size_t old_size = queue_.size();
  std::erase_if(queue_,
                [this, &frame_id, record_drop](const QueuedDatagram& datagram) {
                  if (datagram.frame_id != frame_id)
                    return false;
                  if (record_drop)
                    RecordDrop(datagram.priority);
                  return true;
                });
  return old_size - queue_.size();
}

void BoundedDatagramQueue::RecordDrop(DatagramPriority priority) {
  ++dropped_by_priority_[static_cast<size_t>(priority)];
}

void BoundedDatagramQueue::PruneSentPacketDeadlines(Timestamp now) {
  std::erase_if(sent_packet_deadlines_, [now](const SentPacketDeadline& sent) {
    return sent.deadline + TimeDelta::Seconds(1) <= now;
  });
  std::erase_if(sent_frame_deadlines_, [now](const SentFrameDeadline& sent) {
    return sent.deadline + TimeDelta::Seconds(1) <= now;
  });
}

size_t BoundedDatagramQueue::Clear() {
  const size_t dropped = queue_.size();
  queue_.clear();
  sent_packet_deadlines_.clear();
  sent_frame_deadlines_.clear();
  return dropped;
}

}  // namespace webrtc::roq
