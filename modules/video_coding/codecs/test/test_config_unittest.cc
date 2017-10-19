/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/test_config.h"
#include "test/gtest.h"
#include "test/video_codec_settings.h"

namespace webrtc {
namespace test {

namespace {
const int kNumTemporalLayers = 2;
}  // namespace

TEST(TestConfig, NumberOfCoresWithUseSingleCore) {
  TestConfig config;
  config.use_single_core = true;
  EXPECT_EQ(1, config.NumberOfCores());
}

TEST(TestConfig, NumberOfCoresWithoutUseSingleCore) {
  TestConfig config;
  config.use_single_core = false;
  EXPECT_GE(config.NumberOfCores(), 1);
}

TEST(TestConfig, NumberOfTemporalLayersIsOne) {
  TestConfig config;
  webrtc::test::CodecSettings(kVideoCodecH264, &config.codec_settings);
  EXPECT_EQ(1, config.NumberOfTemporalLayers());
}

TEST(TestConfig, NumberOfTemporalLayers_Vp8) {
  TestConfig config;
  webrtc::test::CodecSettings(kVideoCodecVP8, &config.codec_settings);
  config.codec_settings.VP8()->numberOfTemporalLayers = kNumTemporalLayers;
  EXPECT_EQ(kNumTemporalLayers, config.NumberOfTemporalLayers());
}

TEST(TestConfig, NumberOfTemporalLayers_Vp9) {
  TestConfig config;
  webrtc::test::CodecSettings(kVideoCodecVP9, &config.codec_settings);
  config.codec_settings.VP9()->numberOfTemporalLayers = kNumTemporalLayers;
  EXPECT_EQ(kNumTemporalLayers, config.NumberOfTemporalLayers());
}

}  // namespace test
}  // namespace webrtc
