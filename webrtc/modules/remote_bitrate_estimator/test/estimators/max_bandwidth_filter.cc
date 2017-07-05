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
MaxBandwidthFilter::MaxBandwidthFilter() {}

MaxBandwidthFilter::~MaxBandwidthFilter() {}

bool MaxBandwidthFilter::FullBandwidthReached(float growth_target,
                                              int max_rounds_without_growth) {
  // Minimal bandwidth necessary to assume that better bandwidth can still be
  // found and full bandwidth is not reached.
  int64_t minimal_bandwidth = bandwidth_last_round_ * growth_target;
  if (max_bandwidth_estimate_ >= minimal_bandwidth) {
    bandwidth_last_round_ = max_bandwidth_estimate_;
    rounds_without_growth_ = 0;
    return false;
  }
  rounds_without_growth_++;
  if (rounds_without_growth_ >= max_rounds_without_growth)
    return true;
  return false;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
