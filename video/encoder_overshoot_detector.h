/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ENCODER_OVERSHOOT_DETECTOR_H_
#define VIDEO_ENCODER_OVERSHOOT_DETECTOR_H_

#include <deque>

#include "absl/types/optional.h"
#include "api/units/data_rate.h"

namespace webrtc {

class EncoderOvershootDetector {
 public:
  explicit EncoderOvershootDetector(int64_t window_size_ms);
  ~EncoderOvershootDetector();

  void SetTargetRate(DataRate target_bitrate,
                     double target_framerate_fps,
                     int64_t time_ms);
  void OnEncodedFrame(size_t bytes, int64_t time_ms);
  absl::optional<double> GetUtilizationFactor(int64_t time_ms);
  void Reset();

 private:
  int64_t IdealFrameSizeBits() const;
  void LeakBits(int64_t time_ms);

  const int64_t window_size_ms_;
  int64_t time_last_update_ms_;
  struct BitrateUpdate {
    BitrateUpdate(double utilization_factor, int64_t update_time_ms)
        : utilization_factor(utilization_factor),
          update_time_ms(update_time_ms) {}
    double utilization_factor;
    int64_t update_time_ms;
  };
  std::deque<BitrateUpdate> utilization_factors_;
  double sum_utilization_factors_;
  DataRate target_bitrate_;
  double target_framerate_fps_;
  int64_t buffer_level_bits_;
};

}  // namespace webrtc

#endif  // VIDEO_ENCODER_OVERSHOOT_DETECTOR_H_
