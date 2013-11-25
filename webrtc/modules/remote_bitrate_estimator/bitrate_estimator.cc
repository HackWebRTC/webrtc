/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/bitrate_estimator.h"

namespace webrtc {

const int kBitrateAverageWindowMs = 500;

BitRateStats::BitRateStats()
    : num_buckets_(kBitrateAverageWindowMs + 1),  // N ms in (N+1) buckets.
      buckets_(new uint32_t[num_buckets_]()),
      accumulated_bytes_(0),
      oldest_time_(0),
      oldest_index_(0),
      bps_coefficient_(8.f * 1000.f / (num_buckets_ - 1)) {
}

BitRateStats::~BitRateStats() {
}

void BitRateStats::Init() {
  accumulated_bytes_ = 0;
  oldest_time_ = 0;
  oldest_index_ = 0;
  for (int i = 0; i < num_buckets_; i++) {
    buckets_[i] = 0;
  }
}

void BitRateStats::Update(uint32_t packet_size_bytes, int64_t now_ms) {
  if (now_ms < oldest_time_) {
    // Too old data is ignored.
    return;
  }

  EraseOld(now_ms);

  int now_offset = static_cast<int>(now_ms - oldest_time_);
  assert(now_offset < num_buckets_);
  int index = oldest_index_ + now_offset;
  if (index >= num_buckets_) {
    index -= num_buckets_;
  }
  buckets_[index] += packet_size_bytes;
  accumulated_bytes_ += packet_size_bytes;
}

uint32_t BitRateStats::BitRate(int64_t now_ms) {
  EraseOld(now_ms);
  return static_cast<uint32_t>(accumulated_bytes_ * bps_coefficient_ + 0.5f);
}

void BitRateStats::EraseOld(int64_t now_ms) {
  int64_t new_oldest_time = now_ms - num_buckets_ + 1;
  if (new_oldest_time <= oldest_time_) {
    return;
  }

  while (oldest_time_ < new_oldest_time) {
    uint32_t num_bytes_in_oldest_bucket = buckets_[oldest_index_];
    assert(accumulated_bytes_ >= num_bytes_in_oldest_bucket);
    accumulated_bytes_ -= num_bytes_in_oldest_bucket;
    buckets_[oldest_index_] = 0;
    if (++oldest_index_ >= num_buckets_) {
      oldest_index_ = 0;
    }
    ++oldest_time_;
    if (accumulated_bytes_ == 0) {
      // This guarantees we go through all the buckets at most once, even if
      // |new_oldest_time| is far greater than |oldest_time_|.
      break;
    }
  }
  oldest_time_ = new_oldest_time;
}
}  // namespace webrtc
