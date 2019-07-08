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

  struct CodecTypeSpecific {
    CodecTypeSpecific() {}
    CodecTypeSpecific(int qp_low, int qp_high, int fps)
        : qp_low(qp_low), qp_high(qp_high), fps(fps) {}

    bool operator==(const CodecTypeSpecific& o) const {
      return qp_low == o.qp_low && qp_high == o.qp_high && fps == o.fps;
    }

    absl::optional<int> GetQpLow() const;
    absl::optional<int> GetQpHigh() const;
    absl::optional<int> GetFps() const;
    int qp_low = 0;
    int qp_high = 0;
    int fps = 0;
  };

  struct Config {
    Config();
    Config(int pixels,
           int fps,
           CodecTypeSpecific vp8,
           CodecTypeSpecific vp9,
           CodecTypeSpecific h264,
           CodecTypeSpecific generic);

    bool operator==(const Config& o) const {
      return pixels == o.pixels && fps == o.fps && vp8 == o.vp8 &&
             vp9 == o.vp9 && h264 == o.h264 && generic == o.generic;
    }

    int pixels = 0;         // The video frame size.
    int fps = 0;            // The framerate and thresholds to be used if the
    CodecTypeSpecific vp8;  // frame size is less than or equal to |pixels|.
    CodecTypeSpecific vp9;
    CodecTypeSpecific h264;
    CodecTypeSpecific generic;
  };

  // Returns configurations from field trial on success (default on failure).
  std::vector<Config> GetConfigs() const;

  // Gets the min/max framerate from |configs_| based on |pixels|.
  int MinFps(VideoCodecType type, int pixels) const;
  int MaxFps(VideoCodecType type, int pixels) const;

  // Gets QpThresholds for the codec |type| based on |pixels|.
  absl::optional<VideoEncoder::QpThresholds> GetQpThresholds(
      VideoCodecType type,
      int pixels) const;

 private:
  absl::optional<Config> GetMinFpsConfig(int pixels) const;
  absl::optional<Config> GetMaxFpsConfig(int pixels) const;
  Config GetConfig(int pixels) const;

  std::vector<Config> configs_;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_BALANCED_DEGRADATION_SETTINGS_H_
