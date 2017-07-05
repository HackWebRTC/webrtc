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

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MAX_BANDWIDTH_FILTER_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MAX_BANDWIDTH_FILTER_H_

#include <cstdint>

namespace webrtc {
namespace testing {
namespace bwe {
class MaxBandwidthFilter {
 public:
  MaxBandwidthFilter();

  ~MaxBandwidthFilter();
  int64_t max_bandwidth_estimate() { return max_bandwidth_estimate_; }

  // Save bandwidth sample for the current round.
  // We save bandwidth samples for past 10 rounds to
  // provide better bandwidth estimate.

  void AddBandwidthSample(int64_t sample, int64_t round);

  // Check if bandwidth has grown by certain multiplier for past x rounds,
  // to decide whether or not full bandwidth was reached.
  bool FullBandwidthReached(float growth_target, int max_rounds_without_growth);

 private:
  int64_t bandwidth_last_round_;
  int64_t max_bandwidth_estimate_;
  int64_t rounds_without_growth_;
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MAX_BANDWIDTH_FILTER_H_
