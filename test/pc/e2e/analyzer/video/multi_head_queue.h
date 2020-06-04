/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_MULTI_HEAD_QUEUE_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_MULTI_HEAD_QUEUE_H_

#include <deque>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// A queue that allows more than one reader. Readers are independent, and all
// readers will see all elements; an inserted element stays in the queue until
// all readers have extracted it. Elements are copied and copying is assumed to
// be cheap.
template <typename T>
class MultiHeadQueue {
 public:
  // Creates queue with exactly |readers_count| readers.
  explicit MultiHeadQueue(int readers_count) {
    for (int i = 0; i < readers_count; ++i) {
      queues_.push_back(std::deque<T>());
    }
  }

  // Add value to the end of the queue. Complexity O(readers_count).
  void PushBack(T value) {
    for (auto& queue : queues_) {
      queue.push_back(value);
    }
  }

  // Extract element from specified head. Complexity O(readers_count).
  absl::optional<T> PopFront(int index) {
    RTC_CHECK_LT(index, queues_.size());
    if (queues_[index].empty()) {
      return absl::nullopt;
    }
    T out = queues_[index].front();
    queues_[index].pop_front();
    return out;
  }

  // Returns element at specified head. Complexity O(readers_count).
  absl::optional<T> Front(int index) const {
    RTC_CHECK_LT(index, queues_.size());
    if (queues_[index].empty()) {
      return absl::nullopt;
    }
    return queues_[index].front();
  }

  // Returns true if for all readers there are no elements in the queue or
  // false otherwise. Complexity O(readers_count).
  bool IsEmpty() const {
    for (auto& queue : queues_) {
      if (!queue.empty()) {
        return false;
      }
    }
    return true;
  }

  // Returns size of the longest queue between all readers.
  // Complexity O(readers_count).
  size_t size() const {
    size_t size = 0;
    for (auto& queue : queues_) {
      if (queue.size() > size) {
        size = queue.size();
      }
    }
    return size;
  }

 private:
  std::vector<std::deque<T>> queues_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_MULTI_HEAD_QUEUE_H_
