/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef MODULES_ROQ_BOUNDED_DATAGRAM_QUEUE_H_
#define MODULES_ROQ_BOUNDED_DATAGRAM_QUEUE_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <optional>
#include <span>
#include <vector>

namespace webrtc::roq {

enum class DatagramEnqueueResult { kQueued, kQueueFull, kOversize };

struct QueuedDatagram {
  std::vector<uint8_t> bytes;
};

class BoundedDatagramQueue {
 public:
  BoundedDatagramQueue(size_t capacity, size_t max_datagram_size);

  DatagramEnqueueResult Enqueue(std::span<const uint8_t> bytes);
  DatagramEnqueueResult Enqueue(std::initializer_list<uint8_t> bytes) {
    return Enqueue(std::span<const uint8_t>(bytes.begin(), bytes.size()));
  }
  std::optional<QueuedDatagram> Pop();
  const QueuedDatagram* front() const {
    return queue_.empty() ? nullptr : &queue_.front();
  }
  size_t SetMaxDatagramSize(size_t max_datagram_size);

  size_t size() const { return queue_.size(); }
  size_t peak_size() const { return peak_size_; }
  size_t queue_full_drops() const { return queue_full_drops_; }
  size_t oversize_drops() const { return oversize_drops_; }
  size_t pmtu_purge_drops() const { return pmtu_purge_drops_; }

 private:
  const size_t capacity_;
  size_t max_datagram_size_;
  std::deque<QueuedDatagram> queue_;
  size_t peak_size_ = 0;
  size_t queue_full_drops_ = 0;
  size_t oversize_drops_ = 0;
  size_t pmtu_purge_drops_ = 0;
};

}  // namespace webrtc::roq

#endif  // MODULES_ROQ_BOUNDED_DATAGRAM_QUEUE_H_
