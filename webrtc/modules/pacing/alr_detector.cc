/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/pacing/alr_detector.h"

#include <string>

#include "webrtc/system_wrappers/include/field_trial.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/format_macros.h"
#include "webrtc/rtc_base/logging.h"

namespace {
// Time period over which outgoing traffic is measured.
constexpr int kMeasurementPeriodMs = 500;

}  // namespace

namespace webrtc {

const char* AlrDetector::kScreenshareProbingBweExperimentName =
    "WebRTC-ProbingScreenshareBwe";

AlrDetector::AlrDetector()
    : alr_start_usage_percent_(kDefaultAlrStartUsagePercent),
      alr_end_usage_percent_(kDefaultAlrEndUsagePercent),
      rate_(kMeasurementPeriodMs, RateStatistics::kBpsScale),
      estimated_bitrate_bps_(0) {
  rtc::Optional<AlrExperimentSettings> experiment_settings =
      ParseAlrSettingsFromFieldTrial();
  if (experiment_settings) {
    alr_start_usage_percent_ = experiment_settings->alr_start_usage_percent;
    alr_end_usage_percent_ = experiment_settings->alr_end_usage_percent;
  }
}

AlrDetector::~AlrDetector() {}

void AlrDetector::OnBytesSent(size_t bytes_sent, int64_t now_ms) {
  RTC_DCHECK(estimated_bitrate_bps_);

  rate_.Update(bytes_sent, now_ms);
  rtc::Optional<uint32_t> rate = rate_.Rate(now_ms);
  if (!rate)
    return;

  int percentage = static_cast<int>(*rate) * 100 / estimated_bitrate_bps_;
  if (percentage < alr_start_usage_percent_ && !alr_started_time_ms_) {
    alr_started_time_ms_ = rtc::Optional<int64_t>(now_ms);
  } else if (percentage > alr_end_usage_percent_ && alr_started_time_ms_) {
    alr_started_time_ms_ = rtc::Optional<int64_t>();
  }
}

void AlrDetector::SetEstimatedBitrate(int bitrate_bps) {
  RTC_DCHECK(bitrate_bps);
  estimated_bitrate_bps_ = bitrate_bps;
}

rtc::Optional<int64_t> AlrDetector::GetApplicationLimitedRegionStartTime()
    const {
  return alr_started_time_ms_;
}

rtc::Optional<AlrDetector::AlrExperimentSettings>
AlrDetector::ParseAlrSettingsFromFieldTrial() {
  rtc::Optional<AlrExperimentSettings> ret;
  std::string group_name =
      field_trial::FindFullName(kScreenshareProbingBweExperimentName);

  const std::string kIgnoredSuffix = "_Dogfood";
  if (group_name.rfind(kIgnoredSuffix) ==
      group_name.length() - kIgnoredSuffix.length()) {
    group_name.resize(group_name.length() - kIgnoredSuffix.length());
  }

  if (group_name.empty())
    return ret;

  AlrExperimentSettings settings;
  if (sscanf(group_name.c_str(), "%f-%" PRId64 "-%d-%d",
             &settings.pacing_factor, &settings.max_paced_queue_time,
             &settings.alr_start_usage_percent,
             &settings.alr_end_usage_percent) == 4) {
    ret.emplace(settings);
    LOG(LS_INFO) << "Using screenshare ALR experiment settings: "
                    "pacing factor: "
                 << settings.pacing_factor << ", max pacer queue length: "
                 << settings.max_paced_queue_time
                 << ", ALR start usage percent: "
                 << settings.alr_start_usage_percent
                 << ", ALR end usage percent: "
                 << settings.alr_end_usage_percent;
  }

  return ret;
}

}  // namespace webrtc
