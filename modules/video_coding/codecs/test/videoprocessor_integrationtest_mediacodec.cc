/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videoprocessor_integrationtest.h"

#include <string>
#include <tuple>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "media/base/mediaconstants.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {
const int kForemanNumFrames = 300;
const int kForemanFramerateFps = 30;
}  // namespace

class VideoProcessorIntegrationTestMediaCodec
    : public VideoProcessorIntegrationTest {
 protected:
  VideoProcessorIntegrationTestMediaCodec() {
    config_.filename = "foreman_cif";
    config_.filepath = ResourcePath(config_.filename, "yuv");
    config_.num_frames = kForemanNumFrames;
    config_.hw_encoder = true;
    config_.hw_decoder = true;
  }
};

TEST_F(VideoProcessorIntegrationTestMediaCodec, ForemanCif500kbpsVp8) {
  config_.SetCodecSettings(cricket::kVp8CodecName, 1, 1, 1, false, false, false,
                           352, 288);

  std::vector<RateProfile> rate_profiles = {
      {500, kForemanFramerateFps, kForemanNumFrames}};

  // The thresholds below may have to be tweaked to let even poor MediaCodec
  // implementations pass. If this test fails on the bots, disable it and
  // ping brandtr@.
  std::vector<RateControlThresholds> rc_thresholds = {
      {10, 1, 1, 0.1, 0.2, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{36, 31, 0.92, 0.86}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

TEST_F(VideoProcessorIntegrationTestMediaCodec, ForemanCif500kbpsH264CBP) {
  config_.encoded_frame_checker = &h264_keyframe_checker_;
  config_.SetCodecSettings(cricket::kH264CodecName, 1, 1, 1, false, false,
                           false, 352, 288);

  std::vector<RateProfile> rate_profiles = {
      {500, kForemanFramerateFps, kForemanNumFrames}};

  // The thresholds below may have to be tweaked to let even poor MediaCodec
  // implementations pass. If this test fails on the bots, disable it and
  // ping brandtr@.
  std::vector<RateControlThresholds> rc_thresholds = {
      {10, 1, 1, 0.1, 0.2, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{36, 31, 0.92, 0.86}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

// TODO(brandtr): Enable this test when we have trybots/buildbots with
// HW encoders that support CHP.
TEST_F(VideoProcessorIntegrationTestMediaCodec,
       DISABLED_ForemanCif500kbpsH264CHP) {
  config_.h264_codec_settings.profile = H264::kProfileConstrainedHigh;
  config_.encoded_frame_checker = &h264_keyframe_checker_;
  config_.SetCodecSettings(cricket::kH264CodecName, 1, 1, 1, false, false,
                           false, 352, 288);

  std::vector<RateProfile> rate_profiles = {
      {500, kForemanFramerateFps, kForemanNumFrames}};

  // The thresholds below may have to be tweaked to let even poor MediaCodec
  // implementations pass. If this test fails on the bots, disable it and
  // ping brandtr@.
  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 0, 0.1, 0.2, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{37, 35, 0.93, 0.91}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

TEST_F(VideoProcessorIntegrationTestMediaCodec, ForemanMixedRes100kbpsVp8H264) {
  const int kNumFrames = 30;
  // TODO(brandtr): Add H.264 when we have fixed the encoder.
  const std::vector<std::string> codecs = {cricket::kVp8CodecName};
  const std::vector<std::tuple<int, int>> resolutions = {
      {128, 96}, {160, 120}, {176, 144}, {240, 136}, {320, 240}, {480, 272}};
  const std::vector<RateProfile> rate_profiles = {
      {100, kForemanFramerateFps, kNumFrames}};
  const std::vector<QualityThresholds> quality_thresholds = {
      {29, 26, 0.8, 0.75}};

  for (const auto& codec : codecs) {
    for (const auto& resolution : resolutions) {
      const int width = std::get<0>(resolution);
      const int height = std::get<1>(resolution);
      config_.filename = std::string("foreman_") + std::to_string(width) + "x" +
                         std::to_string(height);
      config_.filepath = ResourcePath(config_.filename, "yuv");
      config_.num_frames = kNumFrames;
      config_.SetCodecSettings(codec, 1, 1, 1, false, false, false,
                               width, height);

      ProcessFramesAndMaybeVerify(
          rate_profiles, nullptr /* rc_thresholds */, &quality_thresholds,
          nullptr /* bs_thresholds */, nullptr /* visualization_params */);
    }
  }
}

}  // namespace test
}  // namespace webrtc
