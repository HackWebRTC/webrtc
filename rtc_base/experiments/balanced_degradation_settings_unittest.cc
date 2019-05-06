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

#include "rtc_base/gunit.h"
#include "test/field_trial.h"
#include "test/gmock.h"

namespace webrtc {
namespace {

void VerifyIsDefault(
    const std::vector<BalancedDegradationSettings::Config>& config) {
  EXPECT_THAT(config, ::testing::ElementsAre(
                          BalancedDegradationSettings::Config{320 * 240, 7},
                          BalancedDegradationSettings::Config{480 * 270, 10},
                          BalancedDegradationSettings::Config{640 * 480, 15}));
}
}  // namespace

TEST(BalancedDegradationSettings, GetsDefaultConfigIfNoList) {
  webrtc::test::ScopedFieldTrials field_trials("");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsConfig) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25/");
  BalancedDegradationSettings settings;
  EXPECT_THAT(
      settings.GetConfigs(),
      ::testing::ElementsAre(BalancedDegradationSettings::Config{1000, 5},
                             BalancedDegradationSettings::Config{2000, 15},
                             BalancedDegradationSettings::Config{3000, 25}));
}

TEST(BalancedDegradationSettings, GetsDefaultConfigForZeroFpsValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:0|15|25/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfPixelsDecreases) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|999|3000,fps:5|15|25/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfFramerateDecreases) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|4|25/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsMinFps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(5, settings.MinFps(1));
  EXPECT_EQ(5, settings.MinFps(1000));
  EXPECT_EQ(15, settings.MinFps(1000 + 1));
  EXPECT_EQ(15, settings.MinFps(2000));
  EXPECT_EQ(25, settings.MinFps(2000 + 1));
  EXPECT_EQ(25, settings.MinFps(3000));
  EXPECT_EQ(std::numeric_limits<int>::max(), settings.MinFps(3000 + 1));
}

TEST(BalancedDegradationSettings, GetsMaxFps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(15, settings.MaxFps(1));
  EXPECT_EQ(15, settings.MaxFps(1000));
  EXPECT_EQ(25, settings.MaxFps(1000 + 1));
  EXPECT_EQ(25, settings.MaxFps(2000));
  EXPECT_EQ(std::numeric_limits<int>::max(), settings.MaxFps(2000 + 1));
}

}  // namespace webrtc
