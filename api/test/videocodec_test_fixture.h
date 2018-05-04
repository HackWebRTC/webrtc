/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_VIDEOCODEC_TEST_FIXTURE_H_
#define API_TEST_VIDEOCODEC_TEST_FIXTURE_H_

#include <vector>

#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/video_coding/codecs/test/stats.h"

namespace webrtc {
namespace test {

// Rates for the encoder and the frame number when to change profile.
struct RateProfile {
  size_t target_kbps;
  size_t input_fps;
  size_t frame_index_rate_update;
};

struct RateControlThresholds {
  double max_avg_bitrate_mismatch_percent;
  double max_time_to_reach_target_bitrate_sec;
  // TODO(ssilkin): Use absolute threshold for framerate.
  double max_avg_framerate_mismatch_percent;
  double max_avg_buffer_level_sec;
  double max_max_key_frame_delay_sec;
  double max_max_delta_frame_delay_sec;
  size_t max_num_spatial_resizes;
  size_t max_num_key_frames;
};

struct QualityThresholds {
  double min_avg_psnr;
  double min_min_psnr;
  double min_avg_ssim;
  double min_min_ssim;
};

struct BitstreamThresholds {
  size_t max_max_nalu_size_bytes;
};

// Should video files be saved persistently to disk for post-run visualization?
struct VisualizationParams {
  bool save_encoded_ivf;
  bool save_decoded_y4m;
};

class VideoCodecTestFixture {
 public:
  virtual ~VideoCodecTestFixture() = default;

  virtual void RunTest(const std::vector<RateProfile>& rate_profiles,
                       const std::vector<RateControlThresholds>* rc_thresholds,
                       const std::vector<QualityThresholds>* quality_thresholds,
                       const BitstreamThresholds* bs_thresholds,
                       const VisualizationParams* visualization_params) = 0;
  virtual Stats GetStats() = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_VIDEOCODEC_TEST_FIXTURE_H_
