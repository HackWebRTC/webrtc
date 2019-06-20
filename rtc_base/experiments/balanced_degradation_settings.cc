/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/balanced_degradation_settings.h"
#include <limits>

#include "rtc_base/experiments/field_trial_list.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
constexpr char kFieldTrial[] = "WebRTC-Video-BalancedDegradationSettings";
constexpr int kMinFps = 1;
constexpr int kMaxFps = 100;

std::vector<BalancedDegradationSettings::Config> DefaultConfigs() {
  return {{320 * 240, 7, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
          {480 * 270, 10, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
          {640 * 480, 15, {0, 0}, {0, 0}, {0, 0}, {0, 0}}};
}

bool IsValidThreshold(
    const BalancedDegradationSettings::QpThreshold& threshold) {
  if (threshold.GetLow().has_value() != threshold.GetHigh().has_value()) {
    RTC_LOG(LS_WARNING) << "Neither or both values should be set.";
    return false;
  }
  if (threshold.GetLow().has_value() && threshold.GetHigh().has_value() &&
      threshold.GetLow().value() >= threshold.GetHigh().value()) {
    RTC_LOG(LS_WARNING) << "Invalid threshold value, low >= high threshold.";
    return false;
  }
  return true;
}

bool IsValid(const std::vector<BalancedDegradationSettings::Config>& configs) {
  if (configs.size() <= 1) {
    RTC_LOG(LS_WARNING) << "Unsupported size, value ignored.";
    return false;
  }
  for (const auto& config : configs) {
    if (config.fps < kMinFps || config.fps > kMaxFps) {
      RTC_LOG(LS_WARNING) << "Unsupported fps setting, value ignored.";
      return false;
    }
  }
  for (size_t i = 1; i < configs.size(); ++i) {
    if (configs[i].pixels < configs[i - 1].pixels ||
        configs[i].fps < configs[i - 1].fps) {
      RTC_LOG(LS_WARNING) << "Invalid fps/pixel value provided.";
      return false;
    }
    if (((configs[i].vp8.low > 0) != (configs[i - 1].vp8.low > 0)) ||
        ((configs[i].vp9.low > 0) != (configs[i - 1].vp9.low > 0)) ||
        ((configs[i].h264.low > 0) != (configs[i - 1].h264.low > 0)) ||
        ((configs[i].generic.low > 0) != (configs[i - 1].generic.low > 0)) ||
        ((configs[i].vp8.high > 0) != (configs[i - 1].vp8.high > 0)) ||
        ((configs[i].vp9.high > 0) != (configs[i - 1].vp9.high > 0)) ||
        ((configs[i].h264.high > 0) != (configs[i - 1].h264.high > 0)) ||
        ((configs[i].generic.high > 0) != (configs[i - 1].generic.high > 0))) {
      RTC_LOG(LS_WARNING) << "Invalid threshold value, all/none should be set.";
      return false;
    }
  }
  for (const auto& config : configs) {
    if (!IsValidThreshold(config.vp8) || !IsValidThreshold(config.vp9) ||
        !IsValidThreshold(config.h264) || !IsValidThreshold(config.generic)) {
      return false;
    }
  }
  return true;
}

std::vector<BalancedDegradationSettings::Config> GetValidOrDefault(
    const std::vector<BalancedDegradationSettings::Config>& configs) {
  if (IsValid(configs)) {
    return configs;
  }
  return DefaultConfigs();
}

absl::optional<VideoEncoder::QpThresholds> GetThresholds(
    VideoCodecType type,
    const BalancedDegradationSettings::Config& config) {
  absl::optional<int> low;
  absl::optional<int> high;

  switch (type) {
    case kVideoCodecVP8:
      low = config.vp8.GetLow();
      high = config.vp8.GetHigh();
      break;
    case kVideoCodecVP9:
      low = config.vp9.GetLow();
      high = config.vp9.GetHigh();
      break;
    case kVideoCodecH264:
      low = config.h264.GetLow();
      high = config.h264.GetHigh();
      break;
    case kVideoCodecGeneric:
      low = config.generic.GetLow();
      high = config.generic.GetHigh();
      break;
    default:
      break;
  }

  if (low && high) {
    RTC_LOG(LS_INFO) << "QP thresholds: low: " << *low << ", high: " << *high;
    return absl::optional<VideoEncoder::QpThresholds>(
        VideoEncoder::QpThresholds(*low, *high));
  }
  return absl::nullopt;
}
}  // namespace

absl::optional<int> BalancedDegradationSettings::QpThreshold::GetLow() const {
  return (low > 0) ? absl::optional<int>(low) : absl::nullopt;
}

absl::optional<int> BalancedDegradationSettings::QpThreshold::GetHigh() const {
  return (high > 0) ? absl::optional<int>(high) : absl::nullopt;
}

BalancedDegradationSettings::Config::Config() = default;

BalancedDegradationSettings::Config::Config(int pixels,
                                            int fps,
                                            QpThreshold vp8,
                                            QpThreshold vp9,
                                            QpThreshold h264,
                                            QpThreshold generic)
    : pixels(pixels),
      fps(fps),
      vp8(vp8),
      vp9(vp9),
      h264(h264),
      generic(generic) {}

BalancedDegradationSettings::BalancedDegradationSettings() {
  FieldTrialStructList<Config> configs(
      {FieldTrialStructMember("pixels", [](Config* c) { return &c->pixels; }),
       FieldTrialStructMember("fps", [](Config* c) { return &c->fps; }),
       FieldTrialStructMember("vp8_qp_low",
                              [](Config* c) { return &c->vp8.low; }),
       FieldTrialStructMember("vp8_qp_high",
                              [](Config* c) { return &c->vp8.high; }),
       FieldTrialStructMember("vp9_qp_low",
                              [](Config* c) { return &c->vp9.low; }),
       FieldTrialStructMember("vp9_qp_high",
                              [](Config* c) { return &c->vp9.high; }),
       FieldTrialStructMember("h264_qp_low",
                              [](Config* c) { return &c->h264.low; }),
       FieldTrialStructMember("h264_qp_high",
                              [](Config* c) { return &c->h264.high; }),
       FieldTrialStructMember("generic_qp_low",
                              [](Config* c) { return &c->generic.low; }),
       FieldTrialStructMember("generic_qp_high",
                              [](Config* c) { return &c->generic.high; })},
      {});

  ParseFieldTrial({&configs}, field_trial::FindFullName(kFieldTrial));

  configs_ = GetValidOrDefault(configs.Get());
  RTC_DCHECK_GT(configs_.size(), 1);
}

BalancedDegradationSettings::~BalancedDegradationSettings() {}

std::vector<BalancedDegradationSettings::Config>
BalancedDegradationSettings::GetConfigs() const {
  return configs_;
}

int BalancedDegradationSettings::MinFps(int pixels) const {
  for (const auto& config : configs_) {
    if (pixels <= config.pixels)
      return config.fps;
  }
  return std::numeric_limits<int>::max();
}

int BalancedDegradationSettings::MaxFps(int pixels) const {
  for (size_t i = 0; i < configs_.size() - 1; ++i) {
    if (pixels <= configs_[i].pixels)
      return configs_[i + 1].fps;
  }
  return std::numeric_limits<int>::max();
}

absl::optional<VideoEncoder::QpThresholds>
BalancedDegradationSettings::GetQpThresholds(VideoCodecType type,
                                             int pixels) const {
  return GetThresholds(type, GetConfig(pixels));
}

BalancedDegradationSettings::Config BalancedDegradationSettings::GetConfig(
    int pixels) const {
  for (const auto& config : configs_) {
    if (pixels <= config.pixels)
      return config;
  }
  return configs_.back();  // Use last above highest pixels.
}

}  // namespace webrtc
