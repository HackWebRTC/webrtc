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

namespace webrtc {

class BalancedDegradationSettings {
 public:
  BalancedDegradationSettings();
  ~BalancedDegradationSettings();

  struct Config {
    Config();
    Config(int pixels, int fps);

    bool operator==(const Config& o) const {
      return pixels == o.pixels && fps == o.fps;
    }

    int pixels = 0;  // The video frame size.
    int fps = 0;     // The framerate to be used if the frame size is less than
                     // or equal to |pixels|.
  };

  // Returns configurations from field trial on success (default on failure).
  std::vector<Config> GetConfigs() const;

  // Gets the min/max framerate from |configs_| based on |pixels|.
  int MinFps(int pixels) const;
  int MaxFps(int pixels) const;

 private:
  std::vector<Config> configs_;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_BALANCED_DEGRADATION_SETTINGS_H_
