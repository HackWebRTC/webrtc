/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_PACING_ALR_DETECTOR_H_
#define WEBRTC_MODULES_PACING_ALR_DETECTOR_H_

#include "webrtc/common_types.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/rtc_base/optional.h"
#include "webrtc/rtc_base/rate_statistics.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Application limited region detector is a class that utilizes signals of
// elapsed time and bytes sent to estimate whether network traffic is
// currently limited by the application's ability to generate traffic.
//
// AlrDetector provides a signal that can be utilized to adjust
// estimate bandwidth.
// Note: This class is not thread-safe.

class AlrDetector {
 public:
  AlrDetector();
  ~AlrDetector();

  void OnBytesSent(size_t bytes_sent, int64_t now_ms);

  // Set current estimated bandwidth.
  void SetEstimatedBitrate(int bitrate_bps);

  // Returns time in milliseconds when the current application-limited region
  // started or empty result if the sender is currently not application-limited.
  rtc::Optional<int64_t> GetApplicationLimitedRegionStartTime() const;

  struct AlrExperimentSettings {
    float pacing_factor = PacedSender::kDefaultPaceMultiplier;
    int64_t max_paced_queue_time = PacedSender::kMaxQueueLengthMs;
    int alr_start_usage_percent = kDefaultAlrStartUsagePercent;
    int alr_end_usage_percent = kDefaultAlrEndUsagePercent;
  };
  static rtc::Optional<AlrExperimentSettings> ParseAlrSettingsFromFieldTrial();

  // Sent traffic percentage as a function of network capacity used to determine
  // application-limited region. ALR region start when bandwidth usage drops
  // below kAlrStartUsagePercent and ends when it raises above
  // kAlrEndUsagePercent. NOTE: This is intentionally conservative at the moment
  // until BW adjustments of application limited region is fine tuned.
  static constexpr int kDefaultAlrStartUsagePercent = 60;
  static constexpr int kDefaultAlrEndUsagePercent = 70;
  static const char* kScreenshareProbingBweExperimentName;

 private:
  int alr_start_usage_percent_;
  int alr_end_usage_percent_;
  RateStatistics rate_;
  int estimated_bitrate_bps_;

  // Non-empty in ALR state.
  rtc::Optional<int64_t> alr_started_time_ms_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_PACING_ALR_DETECTOR_H_
