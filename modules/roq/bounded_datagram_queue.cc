/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/bounded_datagram_queue.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace webrtc::roq {

BoundedDatagramQueue::BoundedDatagramQueue(size_t capacity,
                                           size_t max_datagram_size,
                                           size_t byte_capacity)
    : capacity_(capacity),
      byte_capacity_(byte_capacity),
      max_datagram_size_(max_datagram_size) {}

namespace {

constexpr TimeDelta kRtxLifetime = TimeDelta::Millis(200);
constexpr TimeDelta kRepairWindow = TimeDelta::Seconds(1);
constexpr TimeDelta kFrameAssemblyLifetime = TimeDelta::Seconds(1);

std::string SchedulerBinding(const QueuedDatagram& datagram) {
  return datagram.binding_id.empty() ? std::to_string(datagram.flow_id)
                                     : datagram.binding_id;
}

bool SameVideoStream(const QueuedDatagram& datagram,
                     const DatagramFrameId& frame_id) {
  return datagram.frame_id &&
         datagram.frame_id->binding_id == frame_id.binding_id &&
         datagram.frame_id->ssrc == frame_id.ssrc;
}

}  // namespace

bool BoundedDatagramQueue::ReserveVideoFrame(QueuedDatagram frame,
                                             size_t packet_count,
                                             size_t byte_size,
                                             Timestamp now) {
  if (!frame.frame_id || packet_count == 0 || byte_size == 0 ||
      packet_count > capacity_ || byte_size > byte_capacity_) {
    if (frame.frame_id && packet_count > 0) {
      queue_full_drops_ += packet_count;
      dropped_by_priority_[static_cast<size_t>(frame.priority)] +=
          packet_count;
      binding_stats_[SchedulerBinding(frame)].dropped_packets += packet_count;
      MarkFrameDropped(*frame.frame_id, now);
      RecoveryState& state =
          recovery_[{frame.frame_id->binding_id, frame.frame_id->ssrc}];
      if (frame.request_key_frame)
        state.request_key_frame = frame.request_key_frame;
    }
    return false;
  }
  PurgeExpired(now);
  if (IsBroken(frame) &&
      frame.priority == DatagramPriority::kVideoDelta) {
    dependency_drops_ += packet_count;
    dropped_by_priority_[static_cast<size_t>(frame.priority)] += packet_count;
    DatagramBindingStats& stats = binding_stats_[SchedulerBinding(frame)];
    stats.dropped_packets += packet_count;
    stats.dependency_drops += packet_count;
    return false;
  }
  if (std::any_of(frame_reservations_.begin(), frame_reservations_.end(),
                  [&frame](const FrameReservation& reservation) {
                    return reservation.frame_id == *frame.frame_id;
                  })) {
    return true;
  }
  // Keep one quarter of each external-queue limit available for audio and
  // critical RTCP while a large video access unit is being admitted.
  const size_t video_packet_budget =
      capacity_ >= 4 ? capacity_ - (capacity_ + 3) / 4 : capacity_;
  const size_t video_byte_budget = byte_capacity_ >= 4
                                       ? byte_capacity_ -
                                             (byte_capacity_ + 3) / 4
                                       : byte_capacity_;
  if (queue_.size() + reserved_packets_ + packet_count > video_packet_budget ||
      byte_size_ + reserved_bytes_ + byte_size > video_byte_budget) {
    queue_full_drops_ += packet_count;
    dropped_by_priority_[static_cast<size_t>(frame.priority)] += packet_count;
    binding_stats_[SchedulerBinding(frame)].dropped_packets += packet_count;
    MarkFrameDropped(*frame.frame_id, now);
    RecoveryState& state =
        recovery_[{frame.frame_id->binding_id, frame.frame_id->ssrc}];
    if (frame.request_key_frame)
      state.request_key_frame = frame.request_key_frame;
    return false;
  }
  frame_reservations_.push_back(
      {.frame_id = *frame.frame_id,
       .remaining_packets = packet_count,
       .remaining_bytes = byte_size,
       .priority = frame.priority,
       .deadline = frame.deadline,
       .video_codec = frame.video_codec,
       .is_key = frame.is_key,
       .contains_vps = frame.contains_vps,
       .contains_sps = frame.contains_sps,
       .contains_pps = frame.contains_pps,
       .request_key_frame = std::move(frame.request_key_frame)});
  reserved_packets_ += packet_count;
  reserved_bytes_ += byte_size;
  return true;
}

DatagramEnqueueResult BoundedDatagramQueue::Enqueue(QueuedDatagram datagram,
                                                    Timestamp now) {
  const TimeDelta original_lifetime = datagram.deadline - datagram.enqueue_time;
  auto reservation = frame_reservations_.end();
  if (datagram.frame_id && !datagram.original_packet_id) {
    reservation = std::find_if(
        frame_reservations_.begin(), frame_reservations_.end(),
        [&datagram](const FrameReservation& candidate) {
          return candidate.frame_id == *datagram.frame_id;
        });
    if (reservation != frame_reservations_.end()) {
      datagram.admitted = true;
      datagram.committed = true;
      datagram.priority = reservation->priority;
      datagram.deadline = Timestamp::PlusInfinity();
      datagram.request_key_frame = reservation->request_key_frame;
    }
    if (IsFrameStarted(*datagram.frame_id))
      datagram.committed = true;
  }
  if (datagram.bytes.size() > max_datagram_size_) {
    ++oversize_drops_;
    RecordDrop(datagram.priority);
    ++binding_stats_[SchedulerBinding(datagram)].dropped_packets;
    if (datagram.frame_id) {
      MarkFrameDropped(*datagram.frame_id, now);
      if (datagram.request_key_frame) {
        recovery_[{datagram.frame_id->binding_id, datagram.frame_id->ssrc}]
            .request_key_frame = datagram.request_key_frame;
      }
    }
    return DatagramEnqueueResult::kOversize;
  }
  if (datagram.bytes.size() > byte_capacity_) {
    ++queue_full_drops_;
    RecordDrop(datagram.priority);
    ++binding_stats_[SchedulerBinding(datagram)].dropped_packets;
    if (datagram.frame_id) {
      MarkFrameDropped(*datagram.frame_id, now);
      if (datagram.request_key_frame) {
        recovery_[{datagram.frame_id->binding_id, datagram.frame_id->ssrc}]
            .request_key_frame = datagram.request_key_frame;
      }
    }
    return DatagramEnqueueResult::kQueueFull;
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
    if (original->repair_until <= now) {
      ++expired_drops_;
      RecordDrop(original->priority);
      return DatagramEnqueueResult::kInvalid;
    }
    datagram.deadline = std::min(now + kRtxLifetime, original->repair_until);
    datagram.priority = original->priority;
    datagram.frame_id = original->frame_id;
  }
  if (!datagram.committed && datagram.deadline <= now) {
    ++expired_drops_;
    RecordDrop(datagram.priority);
    ++binding_stats_[SchedulerBinding(datagram)].dropped_packets;
    if (datagram.frame_id) {
      MarkFrameDropped(*datagram.frame_id, now);
      if (datagram.request_key_frame) {
        recovery_[{datagram.frame_id->binding_id, datagram.frame_id->ssrc}]
            .request_key_frame = datagram.request_key_frame;
      }
    }
    return DatagramEnqueueResult::kInvalid;
  }
  if (!datagram.committed && IsBroken(datagram) &&
      datagram.priority == DatagramPriority::kVideoDelta) {
    ++dependency_drops_;
    RecordDrop(datagram.priority);
    DatagramBindingStats& stats = binding_stats_[SchedulerBinding(datagram)];
    ++stats.dropped_packets;
    ++stats.dependency_drops;
    return DatagramEnqueueResult::kDropped;
  }
  if (!datagram.admitted && datagram.frame_id &&
      !datagram.original_packet_id &&
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
        sent.priority = DatagramPriority::kVideoKey;
      }
    }
  } else if (!datagram.admitted && datagram.frame_id &&
             !datagram.original_packet_id) {
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
  if (datagram.frame_id) {
    bool is_key = datagram.is_key;
    bool contains_vps = datagram.contains_vps;
    bool contains_sps = datagram.contains_sps;
    bool contains_pps = datagram.contains_pps;
    if (reservation != frame_reservations_.end()) {
      reservation->is_key |= is_key;
      reservation->contains_vps |= contains_vps;
      reservation->contains_sps |= contains_sps;
      reservation->contains_pps |= contains_pps;
      is_key = reservation->is_key;
      contains_vps = reservation->contains_vps;
      contains_sps = reservation->contains_sps;
      contains_pps = reservation->contains_pps;
    }
    for (const QueuedDatagram& queued : queue_) {
      if (queued.frame_id == datagram.frame_id) {
        is_key |= queued.is_key;
        contains_vps |= queued.contains_vps;
        contains_sps |= queued.contains_sps;
        contains_pps |= queued.contains_pps;
      }
    }
    const bool independent =
        datagram.independent_frame ||
        (is_key && contains_sps && contains_pps &&
         (datagram.video_codec != VideoCodecType::kVideoCodecH265 ||
          contains_vps));
    for (QueuedDatagram& queued : queue_) {
      if (queued.frame_id == datagram.frame_id) {
        queued.is_key = is_key;
        queued.contains_vps = contains_vps;
        queued.contains_sps = contains_sps;
        queued.contains_pps = contains_pps;
        queued.independent_frame = independent;
      }
    }
    datagram.is_key = is_key;
    datagram.contains_vps = contains_vps;
    datagram.contains_sps = contains_sps;
    datagram.contains_pps = contains_pps;
    datagram.independent_frame = independent;
    if (datagram.frame_complete) {
      CompleteFrame(*datagram.frame_id);
      TimeDelta lifetime = original_lifetime;
      for (const QueuedDatagram& queued : queue_) {
        if (queued.frame_id == datagram.frame_id)
          lifetime = std::max(lifetime, queued.deadline - queued.enqueue_time);
      }
      const Timestamp frame_deadline = now + lifetime;
      for (QueuedDatagram& queued : queue_) {
        if (queued.frame_id == datagram.frame_id)
          queued.deadline = frame_deadline;
      }
      datagram.deadline = frame_deadline;
    }
  }
  while (!datagram.admitted && !datagram.committed &&
         (queue_.size() + reserved_packets_ >= capacity_ ||
          byte_size_ + reserved_bytes_ + datagram.bytes.size() >
              byte_capacity_)) {
    const auto lowest_it = std::min_element(
        queue_.begin(), queue_.end(),
        [](const QueuedDatagram& left, const QueuedDatagram& right) {
          if (left.committed != right.committed)
            return !left.committed;
          return left.priority < right.priority;
        });
    if (lowest_it == queue_.end() || lowest_it->committed) {
      ++queue_full_drops_;
      RecordDrop(datagram.priority);
      ++binding_stats_[SchedulerBinding(datagram)].dropped_packets;
      return DatagramEnqueueResult::kQueueFull;
    }
    const DatagramPriority lowest = lowest_it->priority;
    std::map<std::string, size_t> backlog_by_binding;
    for (const QueuedDatagram& queued : queue_) {
      if (!queued.committed && queued.priority == lowest)
        backlog_by_binding[SchedulerBinding(queued)] += queued.bytes.size();
    }
    const auto largest =
        std::max_element(backlog_by_binding.begin(), backlog_by_binding.end(),
                         [](const auto& left, const auto& right) {
                           return left.second < right.second;
                         });
    const auto victim = std::min_element(
        queue_.begin(), queue_.end(),
        [lowest, &largest](const QueuedDatagram& left,
                           const QueuedDatagram& right) {
          const bool left_candidate = !left.committed &&
                                      left.priority == lowest &&
                                      SchedulerBinding(left) == largest->first;
          const bool right_candidate =
              !right.committed && right.priority == lowest &&
              SchedulerBinding(right) == largest->first;
          if (left_candidate != right_candidate)
            return left_candidate;
          return left.enqueue_time < right.enqueue_time;
        });
    if (victim == queue_.end() || datagram.priority < victim->priority) {
      ++queue_full_drops_;
      RecordDrop(datagram.priority);
      ++binding_stats_[SchedulerBinding(datagram)].dropped_packets;
      if (datagram.frame_id) {
        MarkFrameDropped(*datagram.frame_id, now);
        RecoveryState& state =
            recovery_[{datagram.frame_id->binding_id, datagram.frame_id->ssrc}];
        if (datagram.request_key_frame)
          state.request_key_frame = datagram.request_key_frame;
      }
      return DatagramEnqueueResult::kQueueFull;
    }
    size_t removed = 1;
    if (victim->frame_id) {
      const DatagramFrameId victim_frame = *victim->frame_id;
      const size_t before = queue_.size();
      MarkFrameDropped(victim_frame, now);
      EraseFrame(victim_frame, true);
      removed = before - queue_.size();
      if (datagram.frame_id == victim_frame) {
        ++queue_full_drops_;
        RecordDrop(datagram.priority);
        ++binding_stats_[SchedulerBinding(datagram)].dropped_packets;
        RecoveryState& state =
            recovery_[{victim_frame.binding_id, victim_frame.ssrc}];
        if (datagram.request_key_frame)
          state.request_key_frame = datagram.request_key_frame;
        queue_full_drops_ += removed;
        return DatagramEnqueueResult::kQueueFull;
      }
    } else {
      RecordDrop(victim->priority);
      ++binding_stats_[SchedulerBinding(*victim)].dropped_packets;
      byte_size_ -= victim->bytes.size();
      queue_.erase(victim);
    }
    queue_full_drops_ += removed;
  }
  byte_size_ += datagram.bytes.size();
  ++binding_stats_[SchedulerBinding(datagram)].enqueued_packets;
  queue_.push_back(std::move(datagram));
  if (reservation != frame_reservations_.end()) {
    if (reservation->remaining_packets > 0) {
      --reservation->remaining_packets;
      --reserved_packets_;
    }
    const size_t consumed =
        std::min(reservation->remaining_bytes, queue_.back().bytes.size());
    reservation->remaining_bytes -= consumed;
    reserved_bytes_ -= consumed;
    if (reservation->remaining_packets == 0) {
      reserved_bytes_ -= reservation->remaining_bytes;
      frame_reservations_.erase(reservation);
    }
  }
  peak_size_ = std::max(peak_size_, queue_.size());
  peak_byte_size_ = std::max(peak_byte_size_, byte_size_);
  return DatagramEnqueueResult::kQueued;
}

std::optional<QueuedDatagram> BoundedDatagramQueue::Pop(Timestamp now) {
  PurgeExpired(now);
  if (queue_.empty())
    return std::nullopt;
  DatagramPriority highest = DatagramPriority::kPeriodicRtcp;
  bool found = false;
  for (const QueuedDatagram& datagram : queue_) {
    if (!IsFrameComplete(datagram) || HasEarlierVideoFrame(datagram))
      continue;
    if (!found || datagram.priority > highest) {
      highest = datagram.priority;
      found = true;
    }
  }
  if (!found)
    return std::nullopt;

  std::vector<std::string> bindings;
  for (const QueuedDatagram& datagram : queue_) {
    if (datagram.priority != highest || !IsFrameComplete(datagram) ||
        HasEarlierVideoFrame(datagram))
      continue;
    const std::string binding = SchedulerBinding(datagram);
    if (std::find(bindings.begin(), bindings.end(), binding) == bindings.end())
      bindings.push_back(binding);
  }
  const size_t priority = static_cast<size_t>(highest);
  auto selected = queue_.end();
  std::string selected_binding;
  const std::string& current = current_binding_by_priority_[priority];
  if (!current.empty()) {
    selected = std::find_if(
        queue_.begin(), queue_.end(), [&](const QueuedDatagram& datagram) {
          return datagram.priority == highest &&
                 IsFrameComplete(datagram) &&
                 !HasEarlierVideoFrame(datagram) &&
                 SchedulerBinding(datagram) == current &&
                 datagram.bytes.size() <= deficits_[current];
        });
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
      selected = std::find_if(
          queue_.begin(), queue_.end(), [&](const QueuedDatagram& datagram) {
            return datagram.priority == highest &&
                   IsFrameComplete(datagram) &&
                   !HasEarlierVideoFrame(datagram) &&
                   SchedulerBinding(datagram) == binding &&
                   datagram.bytes.size() <= deficits_[binding];
          });
      if (selected != queue_.end()) {
        selected_binding = binding;
        break;
      }
    }
  }
  if (selected == queue_.end())
    return std::nullopt;
  if (selected->frame_id && !selected->original_packet_id) {
    if (!IsFrameStarted(*selected->frame_id))
      started_frames_.push_back(*selected->frame_id);
    for (QueuedDatagram& queued : queue_) {
      if (queued.frame_id == selected->frame_id &&
          !queued.original_packet_id) {
        queued.committed = true;
      }
    }
  }
  QueuedDatagram datagram = std::move(*selected);
  byte_size_ -= datagram.bytes.size();
  queue_.erase(selected);
  deficits_[selected_binding] -= datagram.bytes.size();
  const bool can_continue = std::any_of(
      queue_.begin(), queue_.end(), [&](const QueuedDatagram& queued) {
        return queued.priority == highest &&
               IsFrameComplete(queued) &&
               !HasEarlierVideoFrame(queued) &&
               SchedulerBinding(queued) == selected_binding &&
               queued.bytes.size() <= deficits_[selected_binding];
      });
  if (can_continue) {
    current_binding_by_priority_[priority] = selected_binding;
  } else {
    current_binding_by_priority_[priority].clear();
    last_binding_by_priority_[priority] = selected_binding;
  }
  if (datagram.frame_id &&
      std::none_of(queue_.begin(), queue_.end(),
                   [&datagram](const QueuedDatagram& queued) {
                     return queued.frame_id == datagram.frame_id;
                   }) &&
      std::none_of(frame_reservations_.begin(), frame_reservations_.end(),
                   [&datagram](const FrameReservation& reservation) {
                     return reservation.frame_id == *datagram.frame_id;
                   })) {
    std::erase(complete_frames_, *datagram.frame_id);
    std::erase(started_frames_, *datagram.frame_id);
  }
  max_queue_delay_ = std::max(max_queue_delay_, now - datagram.enqueue_time);
  return datagram;
}

void BoundedDatagramQueue::RecordSubmitted(const QueuedDatagram& datagram,
                                           Timestamp now) {
  ++binding_stats_[SchedulerBinding(datagram)].submitted_packets;
  if (datagram.packet_id) {
    sent_packet_deadlines_.push_back({.packet_id = *datagram.packet_id,
                                      .repair_until = now + kRepairWindow,
                                      .priority = datagram.priority,
                                      .frame_id = datagram.frame_id});
  }
  if (datagram.frame_id && datagram.frame_complete &&
      datagram.independent_frame) {
    const auto key =
        std::make_pair(datagram.frame_id->binding_id, datagram.frame_id->ssrc);
    broken_video_.erase(key);
    recovery_.erase(key);
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

std::vector<std::function<void()>> BoundedDatagramQueue::RecoveryCallbacks(
    Timestamp now,
    TimeDelta rtt,
    bool fresh) {
  if (!fresh || capacity_ == 0 || byte_capacity_ == 0 ||
      queue_.size() * 4 >= capacity_ || byte_size_ * 4 >= byte_capacity_) {
    return {};
  }
  const TimeDelta settle = std::max(rtt * 2, TimeDelta::Millis(200));
  std::vector<std::function<void()>> callbacks;
  for (auto& [key, state] : recovery_) {
    (void)key;
    if (!state.request_key_frame || now - state.last_drop < settle ||
        (state.last_request.IsFinite() &&
         now - state.last_request < TimeDelta::Seconds(1))) {
      continue;
    }
    state.last_request = now;
    ++recovery_requests_;
    ++binding_stats_[key.first].recovery_requests;
    callbacks.push_back(state.request_key_frame);
  }
  return callbacks;
}

void BoundedDatagramQueue::RequireIndependentVideoFrame(
    const std::string& binding_id,
    uint32_t ssrc,
    Timestamp now) {
  const auto key = std::make_pair(binding_id, ssrc);
  broken_video_.insert(key);
  recovery_[key].last_drop = now;
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
    const bool assembly_expired =
        datagram.frame_id && !datagram.original_packet_id &&
        !IsFrameComplete(datagram) &&
        datagram.enqueue_time + kFrameAssemblyLifetime <= now;
    const bool admission_expired =
        datagram.frame_id && !datagram.original_packet_id &&
        IsFrameComplete(datagram) &&
        !IsFrameStarted(*datagram.frame_id) && datagram.deadline <= now;
    if ((assembly_expired || admission_expired) &&
        std::find(expired_frames.begin(), expired_frames.end(),
                  *datagram.frame_id) == expired_frames.end()) {
      expired_frames.push_back(*datagram.frame_id);
    }
  }
  size_t removed = 0;
  for (const DatagramFrameId& frame_id : expired_frames) {
    const size_t before = queue_.size();
    MarkFrameDropped(frame_id, now);
    EraseFrame(frame_id, true);
    removed += before - queue_.size();
  }
  const size_t old_size = queue_.size();
  std::erase_if(queue_, [this, now](const QueuedDatagram& datagram) {
    if (datagram.deadline > now || datagram.committed ||
        (datagram.frame_id && !IsFrameComplete(datagram)))
      return false;
    byte_size_ -= datagram.bytes.size();
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
  byte_size_ -= datagram.bytes.size();
  queue_.pop_front();
  return datagram;
}

size_t BoundedDatagramQueue::SetMaxDatagramSize(size_t max_datagram_size,
                                                Timestamp now) {
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
  for (const DatagramFrameId& frame_id : oversize_frames) {
    const size_t before = queue_.size();
    MarkFrameDropped(frame_id, now);
    EraseFrame(frame_id, true);
    purged += before - queue_.size();
  }
  const size_t old_size = queue_.size();
  std::erase_if(queue_,
                [this, max_datagram_size](const QueuedDatagram& datagram) {
                  if (datagram.bytes.size() <= max_datagram_size)
                    return false;
                  byte_size_ -= datagram.bytes.size();
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
  std::erase_if(
      queue_, [this, &frame_id, record_drop](const QueuedDatagram& datagram) {
        if (datagram.frame_id != frame_id)
          return false;
        byte_size_ -= datagram.bytes.size();
        if (record_drop) {
          RecordDrop(datagram.priority);
          ++binding_stats_[SchedulerBinding(datagram)].dropped_packets;
        }
        return true;
      });
  std::erase(complete_frames_, frame_id);
  std::erase(started_frames_, frame_id);
  std::erase_if(frame_reservations_,
                [this, &frame_id](const FrameReservation& reservation) {
                  if (reservation.frame_id != frame_id)
                    return false;
                  reserved_packets_ -= reservation.remaining_packets;
                  reserved_bytes_ -= reservation.remaining_bytes;
                  return true;
                });
  return old_size - queue_.size();
}

void BoundedDatagramQueue::RecordDrop(DatagramPriority priority) {
  ++dropped_by_priority_[static_cast<size_t>(priority)];
}

void BoundedDatagramQueue::PruneSentPacketDeadlines(Timestamp now) {
  std::erase_if(sent_packet_deadlines_, [now](const SentPacketDeadline& sent) {
    return sent.repair_until + TimeDelta::Seconds(1) <= now;
  });
  std::erase_if(sent_frame_deadlines_, [now](const SentFrameDeadline& sent) {
    return sent.deadline + TimeDelta::Seconds(1) <= now;
  });
}

size_t BoundedDatagramQueue::Clear() {
  const size_t dropped = queue_.size();
  queue_.clear();
  byte_size_ = 0;
  complete_frames_.clear();
  started_frames_.clear();
  frame_reservations_.clear();
  reserved_packets_ = 0;
  reserved_bytes_ = 0;
  broken_video_.clear();
  sent_packet_deadlines_.clear();
  sent_frame_deadlines_.clear();
  return dropped;
}

size_t BoundedDatagramQueue::PurgeBinding(const std::string& binding_id) {
  const size_t old_size = queue_.size();
  std::erase_if(queue_, [this, &binding_id](const QueuedDatagram& datagram) {
    if (datagram.binding_id != binding_id)
      return false;
    byte_size_ -= datagram.bytes.size();
    return true;
  });
  std::erase_if(broken_video_, [&binding_id](const auto& key) {
    return key.first == binding_id;
  });
  std::erase_if(recovery_, [&binding_id](const auto& item) {
    return item.first.first == binding_id;
  });
  std::erase_if(complete_frames_, [&binding_id](const DatagramFrameId& frame) {
    return frame.binding_id == binding_id;
  });
  std::erase_if(started_frames_, [&binding_id](const DatagramFrameId& frame) {
    return frame.binding_id == binding_id;
  });
  std::erase_if(frame_reservations_,
                [this, &binding_id](const FrameReservation& reservation) {
                  if (reservation.frame_id.binding_id != binding_id)
                    return false;
                  reserved_packets_ -= reservation.remaining_packets;
                  reserved_bytes_ -= reservation.remaining_bytes;
                  return true;
                });
  std::erase_if(
      sent_packet_deadlines_, [&binding_id](const SentPacketDeadline& sent) {
        return sent.frame_id && sent.frame_id->binding_id == binding_id;
      });
  std::erase_if(sent_frame_deadlines_,
                [&binding_id](const SentFrameDeadline& sent) {
                  return sent.frame_id.binding_id == binding_id;
                });
  deficits_.erase(binding_id);
  return old_size - queue_.size();
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

void BoundedDatagramQueue::RecordDropped(const QueuedDatagram& datagram,
                                         Timestamp now) {
  RecordDrop(datagram.priority);
  ++binding_stats_[SchedulerBinding(datagram)].dropped_packets;
  if (datagram.frame_id) {
    MarkFrameDropped(*datagram.frame_id, now);
    EraseFrame(*datagram.frame_id, true);
    std::erase(complete_frames_, *datagram.frame_id);
    RecoveryState& state =
        recovery_[{datagram.frame_id->binding_id, datagram.frame_id->ssrc}];
    if (datagram.request_key_frame)
      state.request_key_frame = datagram.request_key_frame;
  }
}

void BoundedDatagramQueue::MarkFrameDropped(const DatagramFrameId& frame_id,
                                            Timestamp now) {
  const auto key = std::make_pair(frame_id.binding_id, frame_id.ssrc);
  broken_video_.insert(key);
  RecoveryState& state = recovery_[key];
  state.last_drop = std::max(state.last_drop, now);
  std::erase(complete_frames_, frame_id);
  for (const QueuedDatagram& datagram : queue_) {
    if (datagram.frame_id == frame_id && datagram.request_key_frame) {
      state.request_key_frame = datagram.request_key_frame;
      break;
    }
  }
  std::erase_if(queue_, [this, &frame_id](const QueuedDatagram& datagram) {
    if (!SameVideoStream(datagram, frame_id) || datagram.frame_id == frame_id ||
        datagram.independent_frame)
      return false;
    byte_size_ -= datagram.bytes.size();
    ++dependency_drops_;
    RecordDrop(datagram.priority);
    DatagramBindingStats& stats = binding_stats_[SchedulerBinding(datagram)];
    ++stats.dropped_packets;
    ++stats.dependency_drops;
    RecoveryState& state = recovery_[{frame_id.binding_id, frame_id.ssrc}];
    if (datagram.request_key_frame)
      state.request_key_frame = datagram.request_key_frame;
    return true;
  });
}

bool BoundedDatagramQueue::IsBroken(const QueuedDatagram& datagram) const {
  return datagram.frame_id &&
         broken_video_.contains(
             {datagram.frame_id->binding_id, datagram.frame_id->ssrc});
}

void BoundedDatagramQueue::CompleteFrame(const DatagramFrameId& frame_id) {
  if (std::find(complete_frames_.begin(), complete_frames_.end(), frame_id) ==
      complete_frames_.end()) {
    complete_frames_.push_back(frame_id);
  }
}

bool BoundedDatagramQueue::IsFrameComplete(
    const QueuedDatagram& datagram) const {
  if (datagram.admitted || datagram.committed || datagram.original_packet_id ||
      !datagram.frame_id ||
      datagram.frame_id->binding_id.empty())
    return true;
  return std::find(complete_frames_.begin(), complete_frames_.end(),
                   *datagram.frame_id) != complete_frames_.end();
}

bool BoundedDatagramQueue::IsFrameStarted(
    const DatagramFrameId& frame_id) const {
  return std::find(started_frames_.begin(), started_frames_.end(), frame_id) !=
         started_frames_.end();
}

bool BoundedDatagramQueue::HasEarlierVideoFrame(
    const QueuedDatagram& datagram) const {
  if (!datagram.frame_id || datagram.original_packet_id)
    return false;
  for (const QueuedDatagram& queued : queue_) {
    if (&queued == &datagram)
      break;
    if (!queued.original_packet_id && queued.frame_id &&
        queued.frame_id->binding_id == datagram.frame_id->binding_id &&
        queued.frame_id->ssrc == datagram.frame_id->ssrc &&
        queued.frame_id->timestamp != datagram.frame_id->timestamp) {
      return true;
    }
  }
  return false;
}

}  // namespace webrtc::roq
