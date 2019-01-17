/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_MESSAGE_QUEUE_H_
#define MODULES_AUDIO_PROCESSING_AEC3_MESSAGE_QUEUE_H_

#include <atomic>
#include <utility>
#include <vector>

#include "rtc_base/checks.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

// Fixed-size circular queue similar to SwapQueue, but lock-free and no
// QueueItemVerifierFunction.
// The queue is designed for single-producer-single-consumer (accessed by one
// producer thread, calling Insert(), and one consumer thread, calling Remove().
template <typename T>
class MessageQueue {
 public:
  explicit MessageQueue(size_t size) : num_elements_(0), queue_(size) {
    producer_thread_checker_.DetachFromThread();
    consumer_thread_checker_.DetachFromThread();
  }
  MessageQueue(size_t size, const T& prototype)
      : num_elements_(0), queue_(size, prototype) {
    producer_thread_checker_.DetachFromThread();
    consumer_thread_checker_.DetachFromThread();
  }
  ~MessageQueue() = default;

  // Inserts a T at the back of the queue by swapping *input with a T from the
  // queue. This function should not be called concurrently. It can however be
  // called concurrently with Remove(). Returns true if the item was inserted or
  // false if not (the queue was full).
  bool Insert(T* input) {
    RTC_DCHECK_RUN_ON(&producer_thread_checker_);
    RTC_DCHECK(input);

    if (num_elements_ == queue_.size()) {
      return false;
    }

    std::swap(*input, queue_[next_write_index_]);

    ++next_write_index_;
    if (next_write_index_ == queue_.size()) {
      next_write_index_ = 0;
    }

    ++num_elements_;

    RTC_DCHECK_LT(next_write_index_, queue_.size());

    return true;
  }

  // Removes the frontmost T from the queue by swapping it with the T in
  // *output. This function should not be called concurrently. It can however be
  // called concurrently with Insert(). Returns true if an item could be removed
  // or false if not (the queue was empty).
  bool Remove(T* output) {
    RTC_DCHECK_RUN_ON(&consumer_thread_checker_);
    RTC_DCHECK(output);

    if (num_elements_ == 0) {
      return false;
    }

    std::swap(*output, queue_[next_read_index_]);

    ++next_read_index_;
    if (next_read_index_ == queue_.size()) {
      next_read_index_ = 0;
    }

    --num_elements_;

    RTC_DCHECK_LT(next_read_index_, queue_.size());

    return true;
  }

 private:
  uint32_t next_write_index_ = 0;
  uint32_t next_read_index_ = 0;
  rtc::ThreadChecker producer_thread_checker_;
  rtc::ThreadChecker consumer_thread_checker_;
  std::atomic<uint32_t> num_elements_;
  std::vector<T> queue_;
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_MESSAGE_QUEUE_H_
