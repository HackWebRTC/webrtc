/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EXPERIMENTS_BALANCED_DEGRADATION_SETTINGS_H_
#define RTC_BASE_EXPERIMENTS_BALANCED_DEGRADATION_SETTINGS_H_

#include <vector>

#include "absl/types/optional.h"
#include "api/video_codecs/video_encoder.h"

namespace webrtc {

class BalancedDegradationSettings {
 public:
  BalancedDegradationSettings();
  ~BalancedDegradationSettings();

  struct QpThreshold {
    QpThreshold() {}
    QpThreshold(int low, int high) : low(low), high(high) {}

    bool operator==(const QpThreshold& o) const {
      return low == o.low && high == o.high;
    }

    absl::optional<int> GetLow() const;
    absl::optional<int> GetHigh() const;
    int low = 0;
    int high = 0;
  };

  struct Config {
    Config();
    Config(int pixels,
           int fps,
           QpThreshold vp8,
           QpThreshold vp9,
           QpThreshold h264,
           QpThreshold generic);

    bool operator==(const Config& o) const {
      return pixels == o.pixels && fps == o.fps && vp8 == o.vp8 &&
             vp9 == o.vp9 && h264 == o.h264 && generic == o.generic;
    }

    int pixels = 0;   // The video frame size.
    int fps = 0;      // The framerate and thresholds to be used if the frame
    QpThreshold vp8;  // size is less than or equal to |pixels|.
    QpThreshold vp9;
    QpThreshold h264;
    QpThreshold generic;
  };

  // Returns configurations from field trial on success (default on failure).
  std::vector<Config> GetConfigs() const;

  // Gets the min/max framerate from |configs_| based on |pixels|.
  int MinFps(int pixels) const;
  int MaxFps(int pixels) const;

  // Gets QpThresholds for the codec |type| based on |pixels|.
  absl::optional<VideoEncoder::QpThresholds> GetQpThresholds(
      VideoCodecType type,
      int pixels) const;

 private:
  Config GetConfig(int pixels) const;

  std::vector<Config> configs_;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_BALANCED_DEGRADATION_SETTINGS_H_
