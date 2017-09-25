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

#include "test/field_trial.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

#if defined(WEBRTC_ANDROID)

namespace {
const int kForemanNumFrames = 300;
const std::nullptr_t kNoVisualizationParams = nullptr;
}  // namespace

class VideoProcessorIntegrationTestMediaCodec
    : public VideoProcessorIntegrationTest {
 protected:
  VideoProcessorIntegrationTestMediaCodec() {
    config_.filename = "foreman_cif";
    config_.input_filename = ResourcePath(config_.filename, "yuv");
    config_.output_filename =
        TempFilename(OutputPath(), "videoprocessor_integrationtest_mediacodec");
    config_.verbose = false;
    config_.hw_encoder = true;
    config_.hw_decoder = true;
  }
};

TEST_F(VideoProcessorIntegrationTestMediaCodec, DISABLED_ForemanCif500kbpsVp8) {
  SetCodecSettings(&config_, kVideoCodecVP8, 1, false, false, false, false,
                   false, 352, 288);

  RateProfile rate_profile;
  SetRateProfile(&rate_profile, 0, 500, 30, 0);  // Start below |low_kbps|.
  rate_profile.frame_index_rate_update[1] = kForemanNumFrames + 1;
  rate_profile.num_frames = kForemanNumFrames;

  // The thresholds below may have to be tweaked to let even poor MediaCodec
  // implementations pass. If this test fails on the bots, disable it and
  // ping brandtr@.
  std::vector<RateControlThresholds> rc_thresholds;
  AddRateControlThresholds(5, 95, 20, 10, 10, 0, 1, &rc_thresholds);

  QualityThresholds quality_thresholds(30.0, 15.0, 0.90, 0.40);

  ProcessFramesAndMaybeVerify(rate_profile, &rc_thresholds, &quality_thresholds,
                              kNoVisualizationParams);
}

TEST_F(VideoProcessorIntegrationTestMediaCodec,
       Foreman240p100kbpsVp8WithForcedSwFallback) {
  ScopedFieldTrials override_field_trials(
      "WebRTC-VP8-Forced-Fallback-Encoder/Enabled-150,175,10000,1/");

  config_.filename = "foreman_320x240";
  config_.input_filename = ResourcePath(config_.filename, "yuv");
  config_.sw_fallback_encoder = true;
  SetCodecSettings(&config_, kVideoCodecVP8, 1, false, false, false, false,
                   false, 320, 240);

  RateProfile rate_profile;
  SetRateProfile(&rate_profile, 0, 100, 10, 0);    // Start below |low_kbps|.
  SetRateProfile(&rate_profile, 1, 100, 10, 80);   // Fallback in this bucket.
  SetRateProfile(&rate_profile, 2, 200, 10, 200);  // Switch back here.
  rate_profile.frame_index_rate_update[3] = kForemanNumFrames + 1;
  rate_profile.num_frames = kForemanNumFrames;

  // The thresholds below may have to be tweaked to let even poor MediaCodec
  // implementations pass. If this test fails on the bots, disable it and
  // ping brandtr@.
  std::vector<RateControlThresholds> rc_thresholds;
  AddRateControlThresholds(0, 50, 75, 70, 10, 0, 1, &rc_thresholds);
  AddRateControlThresholds(0, 50, 25, 12, 60, 0, 1, &rc_thresholds);
  AddRateControlThresholds(0, 65, 15, 5, 5, 0, 1, &rc_thresholds);

  QualityThresholds quality_thresholds(33.0, 30.0, 0.90, 0.85);

  ProcessFramesAndMaybeVerify(rate_profile, &rc_thresholds, &quality_thresholds,
                              kNoVisualizationParams);
}

#endif  // defined(WEBRTC_ANDROID)

}  // namespace test
}  // namespace webrtc
