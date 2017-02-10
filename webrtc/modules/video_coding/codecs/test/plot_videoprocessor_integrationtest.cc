/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/test/videoprocessor_integrationtest.h"

namespace webrtc {
namespace test {
namespace {
// Codec settings.
const int kBitrates[] = {30, 50, 100, 200, 300, 500, 1000};
const int kFps[] = {30};
const bool kErrorConcealmentOn = false;
const bool kDenoisingOn = false;
const bool kFrameDropperOn = true;
const bool kSpatialResizeOn = false;
const VideoCodecType kVideoCodecType = kVideoCodecVP8;

// Packet loss probability [0.0, 1.0].
const float kPacketLoss = 0.0f;

const bool kVerboseLogging = true;
}  // namespace

// Tests for plotting statistics from logs.
class PlotVideoProcessorIntegrationTest
    : public VideoProcessorIntegrationTest,
      public ::testing::WithParamInterface<::testing::tuple<int, int>> {
 protected:
  PlotVideoProcessorIntegrationTest()
      : bitrate_(::testing::get<0>(GetParam())),
        framerate_(::testing::get<1>(GetParam())) {}

  virtual ~PlotVideoProcessorIntegrationTest() {}

  void RunTest(int bitrate,
               int framerate,
               int width,
               int height,
               const std::string& filename) {
    // Bitrate and frame rate profile.
    RateProfile rate_profile;
    SetRateProfilePars(&rate_profile,
                       0,  // update_index
                       bitrate, framerate,
                       0);  // frame_index_rate_update
    rate_profile.frame_index_rate_update[1] = kNbrFramesLong + 1;
    rate_profile.num_frames = kNbrFramesLong;
    // Codec/network settings.
    CodecConfigPars process_settings;
    SetCodecParameters(&process_settings, kVideoCodecType, kPacketLoss,
                       -1,  // key_frame_interval
                       1,   // num_temporal_layers
                       kErrorConcealmentOn, kDenoisingOn, kFrameDropperOn,
                       kSpatialResizeOn, width, height, filename,
                       kVerboseLogging);
    // Metrics for expected quality (PSNR avg, PSNR min, SSIM avg, SSIM min).
    QualityMetrics quality_metrics;
    SetQualityMetrics(&quality_metrics, 15.0, 10.0, 0.2, 0.1);
    // Metrics for rate control.
    RateControlMetrics rc_metrics[1];
    SetRateControlMetrics(rc_metrics,
                          0,    // update_index
                          300,  // max_num_dropped_frames,
                          400,  // max_key_frame_size_mismatch
                          200,  // max_delta_frame_size_mismatch
                          100,  // max_encoding_rate_mismatch
                          300,  // max_time_hit_target
                          0,    // num_spatial_resizes
                          1);   // num_key_frames
    ProcessFramesAndVerify(quality_metrics, rate_profile, process_settings,
                           rc_metrics);
  }
  const int bitrate_;
  const int framerate_;
};

INSTANTIATE_TEST_CASE_P(CodecSettings,
                        PlotVideoProcessorIntegrationTest,
                        ::testing::Combine(::testing::ValuesIn(kBitrates),
                                           ::testing::ValuesIn(kFps)));

TEST_P(PlotVideoProcessorIntegrationTest, ProcessSQCif) {
  RunTest(bitrate_, framerate_, 128, 96, "foreman_128x96");
}

TEST_P(PlotVideoProcessorIntegrationTest, ProcessQQVga) {
  RunTest(bitrate_, framerate_, 160, 120, "foreman_160x120");
}

TEST_P(PlotVideoProcessorIntegrationTest, ProcessQCif) {
  RunTest(bitrate_, framerate_, 176, 144, "foreman_176x144");
}

TEST_P(PlotVideoProcessorIntegrationTest, ProcessQVga) {
  RunTest(bitrate_, framerate_, 320, 240, "foreman_320x240");
}

TEST_P(PlotVideoProcessorIntegrationTest, ProcessCif) {
  RunTest(bitrate_, framerate_, 352, 288, "foreman_cif");
}

}  // namespace test
}  // namespace webrtc
