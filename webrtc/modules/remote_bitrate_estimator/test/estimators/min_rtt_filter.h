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

// Expiration time for min_rtt sample, which is set to 10 seconds according to
// BBR design doc.
const int64_t kMinRttFilterSizeMs = 10000;

class MinRttFilter {
 public:
  MinRttFilter() {}
  ~MinRttFilter() {}

  rtc::Optional<int64_t> min_rtt_ms() { return min_rtt_ms_; }
  void AddRttSample(int64_t rtt_ms, int64_t now_ms) {
    if (!min_rtt_ms_ || rtt_ms <= *min_rtt_ms_ || MinRttExpired(now_ms)) {
      min_rtt_ms_.emplace(rtt_ms);
      discovery_time_ms_ = now_ms;
    }
  }
  int64_t discovery_time() { return discovery_time_ms_; }

  // Checks whether or not last discovered min_rtt value is older than x
  // milliseconds.
  bool MinRttExpired(int64_t now_ms) {
    return now_ms - discovery_time_ms_ >= kMinRttFilterSizeMs;
  }

 private:
  rtc::Optional<int64_t> min_rtt_ms_;
  int64_t discovery_time_ms_ = 0;
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MIN_RTT_FILTER_H_
