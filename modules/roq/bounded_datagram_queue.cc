/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/bounded_datagram_queue.h"

#include <algorithm>
#include <utility>

namespace webrtc::roq {

BoundedDatagramQueue::BoundedDatagramQueue(size_t capacity,
                                           size_t max_datagram_size)
    : capacity_(capacity), max_datagram_size_(max_datagram_size) {}

DatagramEnqueueResult BoundedDatagramQueue::Enqueue(
    std::span<const uint8_t> bytes) {
  if (bytes.size() > max_datagram_size_) {
    ++oversize_drops_;
    return DatagramEnqueueResult::kOversize;
  }
  if (queue_.size() >= capacity_) {
    ++queue_full_drops_;
    return DatagramEnqueueResult::kQueueFull;
  }
  queue_.push_back({std::vector<uint8_t>(bytes.begin(), bytes.end())});
  peak_size_ = std::max(peak_size_, queue_.size());
  return DatagramEnqueueResult::kQueued;
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
  const size_t old_size = queue_.size();
  std::erase_if(queue_, [max_datagram_size](const QueuedDatagram& datagram) {
    return datagram.bytes.size() > max_datagram_size;
  });
  const size_t purged = old_size - queue_.size();
  pmtu_purge_drops_ += purged;
  return purged;
}

}  // namespace webrtc::roq
