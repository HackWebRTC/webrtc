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
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/video_codec_settings.h"

using ::testing::ElementsAre;

namespace webrtc {
namespace test {

namespace {
const size_t kNumTemporalLayers = 2;
}  // namespace

TEST(TestConfig, NumberOfCoresWithUseSingleCore) {
  TestConfig config;
  config.use_single_core = true;
  EXPECT_EQ(1u, config.NumberOfCores());
}

TEST(TestConfig, NumberOfCoresWithoutUseSingleCore) {
  TestConfig config;
  config.use_single_core = false;
  EXPECT_GE(config.NumberOfCores(), 1u);
}

TEST(TestConfig, NumberOfTemporalLayersIsOne) {
  TestConfig config;
  webrtc::test::CodecSettings(kVideoCodecH264, &config.codec_settings);
  EXPECT_EQ(1u, config.NumberOfTemporalLayers());
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

TEST(TestConfig, ForcedKeyFrameIntervalOff) {
  TestConfig config;
  config.keyframe_interval = 0;
  EXPECT_THAT(config.FrameTypeForFrame(0), ElementsAre(kVideoFrameDelta));
  EXPECT_THAT(config.FrameTypeForFrame(1), ElementsAre(kVideoFrameDelta));
  EXPECT_THAT(config.FrameTypeForFrame(2), ElementsAre(kVideoFrameDelta));
}

TEST(TestConfig, ForcedKeyFrameIntervalOn) {
  TestConfig config;
  config.keyframe_interval = 3;
  EXPECT_THAT(config.FrameTypeForFrame(0), ElementsAre(kVideoFrameKey));
  EXPECT_THAT(config.FrameTypeForFrame(1), ElementsAre(kVideoFrameDelta));
  EXPECT_THAT(config.FrameTypeForFrame(2), ElementsAre(kVideoFrameDelta));
  EXPECT_THAT(config.FrameTypeForFrame(3), ElementsAre(kVideoFrameKey));
  EXPECT_THAT(config.FrameTypeForFrame(4), ElementsAre(kVideoFrameDelta));
  EXPECT_THAT(config.FrameTypeForFrame(5), ElementsAre(kVideoFrameDelta));
}

TEST(TestConfig, FilenameWithParams) {
  TestConfig config;
  config.filename = "filename";
  webrtc::test::CodecSettings(kVideoCodecVP8, &config.codec_settings);
  config.hw_encoder = true;
  config.codec_settings.startBitrate = 400;
  EXPECT_EQ("filename_VP8_hw_400", config.FilenameWithParams());
}

}  // namespace test
}  // namespace webrtc
