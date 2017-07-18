/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/max_bandwidth_filter.h"

namespace webrtc {
namespace testing {
namespace bwe {
MaxBandwidthFilter::MaxBandwidthFilter()
    : bandwidth_last_round_bytes_per_ms_(0),
      round_bandwidth_updated_(0),
      max_bandwidth_estimate_bytes_per_ms_(0),
      rounds_without_growth_(0) {}

MaxBandwidthFilter::~MaxBandwidthFilter() {}

// Rounds are units for packets rtt_time, after packet has been acknowledged,
// one round has passed from its send time.
void MaxBandwidthFilter::AddBandwidthSample(int64_t sample_bytes_per_ms,
                                            int64_t round,
                                            size_t filter_size_round) {
  if (round - round_bandwidth_updated_ >= filter_size_round ||
      sample_bytes_per_ms >= max_bandwidth_estimate_bytes_per_ms_) {
    max_bandwidth_estimate_bytes_per_ms_ = sample_bytes_per_ms;
    round_bandwidth_updated_ = round;
  }
}

bool MaxBandwidthFilter::FullBandwidthReached(float growth_target,
                                              int max_rounds_without_growth) {
  // Minimal bandwidth necessary to assume that better bandwidth can still be
  // found and full bandwidth is not reached.
  int64_t minimal_bandwidth =
      bandwidth_last_round_bytes_per_ms_ * growth_target;
  if (max_bandwidth_estimate_bytes_per_ms_ >= minimal_bandwidth) {
    bandwidth_last_round_bytes_per_ms_ = max_bandwidth_estimate_bytes_per_ms_;
    rounds_without_growth_ = 0;
    return false;
  }
  rounds_without_growth_++;
  return rounds_without_growth_ >= max_rounds_without_growth;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
