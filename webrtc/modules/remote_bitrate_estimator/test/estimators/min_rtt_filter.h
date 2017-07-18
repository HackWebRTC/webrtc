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

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MIN_RTT_FILTER_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MIN_RTT_FILTER_H_

#include <cstdint>
#include <limits>

#include "webrtc/rtc_base/optional.h"

namespace webrtc {
namespace testing {
namespace bwe {
class MinRttFilter {
 public:
  MinRttFilter() {}
  ~MinRttFilter() {}

  rtc::Optional<int64_t> min_rtt_ms() { return min_rtt_ms_; }
  void add_rtt_sample(int64_t rtt_ms, int64_t now_ms) {
    if (!min_rtt_ms_ || rtt_ms <= *min_rtt_ms_) {
      min_rtt_ms_.emplace(rtt_ms);
      discovery_time_ms_ = now_ms;
    }
  }
  int64_t discovery_time() { return discovery_time_ms_; }

  // Checks whether or not last discovered min_rtt value is older than x
  // milliseconds.
  bool min_rtt_expired(int64_t now_ms, int64_t min_rtt_filter_window_size_ms) {
    return now_ms - discovery_time_ms_ >= min_rtt_filter_window_size_ms;
  }

 private:
  rtc::Optional<int64_t> min_rtt_ms_;
  int64_t discovery_time_ms_ = 0;
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MIN_RTT_FILTER_H_
