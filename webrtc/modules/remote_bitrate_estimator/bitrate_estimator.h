/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_BITRATE_ESTIMATOR_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_BITRATE_ESTIMATOR_H_

#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class BitRateStats {
 public:
  BitRateStats();
  ~BitRateStats();

  void Init();
  void Update(uint32_t packet_size_bytes, int64_t now_ms);
  uint32_t BitRate(int64_t now_ms);

 private:
  void EraseOld(int64_t now_ms);

  // Numbers of bytes are kept in buckets (circular buffer), with one bucket
  // per millisecond.
  const int num_buckets_;
  scoped_array<uint32_t> buckets_;

  // Total number of bytes recorded in buckets.
  uint32_t accumulated_bytes_;

  // Oldest time recorded in buckets.
  int64_t oldest_time_;

  // Bucket index of oldest bytes recorded in buckets.
  int oldest_index_;

  // To convert number of bytes in bits/second.
  const float bps_coefficient_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_BITRATE_ESTIMATOR_H_
