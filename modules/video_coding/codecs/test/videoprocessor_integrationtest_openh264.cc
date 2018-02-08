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

#include <vector>

#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

#if defined(WEBRTC_USE_H264)

namespace {
// Codec settings.
const bool kResilienceOn = true;
const int kCifWidth = 352;
const int kCifHeight = 288;
const int kNumFrames = 100;
}  // namespace

class VideoProcessorIntegrationTestOpenH264
    : public VideoProcessorIntegrationTest {
 protected:
  VideoProcessorIntegrationTestOpenH264() {
    config_.filename = "foreman_cif";
    config_.input_filename = ResourcePath(config_.filename, "yuv");
    config_.num_frames = kNumFrames;
    // Only allow encoder/decoder to use single core, for predictability.
    config_.use_single_core = true;
    config_.hw_encoder = false;
    config_.hw_decoder = false;
    config_.encoded_frame_checker = &h264_keyframe_checker_;
  }
};

TEST_F(VideoProcessorIntegrationTestOpenH264, ConstantHighBitrate) {
  config_.SetCodecSettings(kVideoCodecH264, 1, 1, 1, false, false, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {{500, 30, kNumFrames}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 0, 0.1, 0.2, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{37, 35, 0.93, 0.91}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

// H264: Enable SingleNalUnit packetization mode. Encoder should split
// large frames into multiple slices and limit length of NAL units.
TEST_F(VideoProcessorIntegrationTestOpenH264, SingleNalUnit) {
  config_.h264_codec_settings.packetization_mode =
      H264PacketizationMode::SingleNalUnit;
  config_.max_payload_size_bytes = 500;
  config_.SetCodecSettings(kVideoCodecH264, 1, 1, 1, false, false, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {{500, 30, kNumFrames}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 0, 0.1, 0.2, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{37, 35, 0.93, 0.91}};

  BitstreamThresholds bs_thresholds = {config_.max_payload_size_bytes};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, &bs_thresholds, nullptr);
}

#endif  // defined(WEBRTC_USE_H264)

}  // namespace test
}  // namespace webrtc
