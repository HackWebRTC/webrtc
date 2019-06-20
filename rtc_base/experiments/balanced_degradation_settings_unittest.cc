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
                          BalancedDegradationSettings::Config{
                              320 * 240, 7, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
                          BalancedDegradationSettings::Config{
                              480 * 270, 10, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
                          BalancedDegradationSettings::Config{
                              640 * 480, 15, {0, 0}, {0, 0}, {0, 0}, {0, 0}}));
}
}  // namespace

TEST(BalancedDegradationSettings, GetsDefaultConfigIfNoList) {
  webrtc::test::ScopedFieldTrials field_trials("");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecVP8, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecVP9, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecH264, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecGeneric, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecMultiplex, 1));
}

TEST(BalancedDegradationSettings, GetsConfig) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:11|22|33,fps:5|15|25,other:4|5|6/");
  BalancedDegradationSettings settings;
  EXPECT_THAT(settings.GetConfigs(),
              ::testing::ElementsAre(
                  BalancedDegradationSettings::Config{
                      11, 5, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
                  BalancedDegradationSettings::Config{
                      22, 15, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
                  BalancedDegradationSettings::Config{
                      33, 25, {0, 0}, {0, 0}, {0, 0}, {0, 0}}));
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

TEST(BalancedDegradationSettings, QpThresholdsNotSetByDefault) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25/");
  BalancedDegradationSettings settings;
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecVP8, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecVP9, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecH264, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecGeneric, 1));
}

TEST(BalancedDegradationSettings, GetsConfigWithQpThresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_qp_low:89|90|88,"
      "vp8_qp_high:90|91|92,vp9_qp_low:27|28|29,vp9_qp_high:120|130|140,"
      "h264_qp_low:12|13|14,h264_qp_high:20|30|40,generic_qp_low:7|6|5,"
      "generic_qp_high:22|23|24/");
  BalancedDegradationSettings settings;
  EXPECT_THAT(settings.GetConfigs(),
              ::testing::ElementsAre(
                  BalancedDegradationSettings::Config{
                      1000, 5, {89, 90}, {27, 120}, {12, 20}, {7, 22}},
                  BalancedDegradationSettings::Config{
                      2000, 15, {90, 91}, {28, 130}, {13, 30}, {6, 23}},
                  BalancedDegradationSettings::Config{
                      3000, 25, {88, 92}, {29, 140}, {14, 40}, {5, 24}}));
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfOnlyHasLowThreshold) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_qp_low:89|90|88/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfOnlyHasHighThreshold) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_qp_high:90|91|92/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfLowEqualsHigh) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "vp8_qp_low:89|90|88,vp8_qp_high:90|91|88/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfLowGreaterThanHigh) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "vp8_qp_low:89|90|88,vp8_qp_high:90|91|87/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigForZeroQpValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "vp8_qp_low:89|0|88,vp8_qp_high:90|91|92/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsVp8QpThresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "vp8_qp_low:89|90|88,vp8_qp_high:90|91|92/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(89, settings.GetQpThresholds(kVideoCodecVP8, 1)->low);
  EXPECT_EQ(90, settings.GetQpThresholds(kVideoCodecVP8, 1)->high);
  EXPECT_EQ(90, settings.GetQpThresholds(kVideoCodecVP8, 1000)->high);
  EXPECT_EQ(91, settings.GetQpThresholds(kVideoCodecVP8, 1001)->high);
  EXPECT_EQ(91, settings.GetQpThresholds(kVideoCodecVP8, 2000)->high);
  EXPECT_EQ(92, settings.GetQpThresholds(kVideoCodecVP8, 2001)->high);
  EXPECT_EQ(92, settings.GetQpThresholds(kVideoCodecVP8, 3000)->high);
  EXPECT_EQ(92, settings.GetQpThresholds(kVideoCodecVP8, 3001)->high);
}

TEST(BalancedDegradationSettings, GetsVp9QpThresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "vp9_qp_low:55|56|57,vp9_qp_high:155|156|157/");
  BalancedDegradationSettings settings;
  const auto thresholds = settings.GetQpThresholds(kVideoCodecVP9, 1000);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(55, thresholds->low);
  EXPECT_EQ(155, thresholds->high);
}

TEST(BalancedDegradationSettings, GetsH264QpThresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "h264_qp_low:21|22|23,h264_qp_high:41|43|42/");
  BalancedDegradationSettings settings;
  const auto thresholds = settings.GetQpThresholds(kVideoCodecH264, 2000);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(22, thresholds->low);
  EXPECT_EQ(43, thresholds->high);
}

TEST(BalancedDegradationSettings, GetsGenericQpThresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "generic_qp_low:2|3|4,generic_qp_high:22|23|24/");
  BalancedDegradationSettings settings;
  const auto thresholds = settings.GetQpThresholds(kVideoCodecGeneric, 3000);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(4, thresholds->low);
  EXPECT_EQ(24, thresholds->high);
}

}  // namespace webrtc
