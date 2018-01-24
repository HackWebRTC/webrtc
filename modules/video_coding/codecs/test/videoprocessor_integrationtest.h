/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "common_video/h264/h264_common.h"
#include "media/engine/webrtcvideodecoderfactory.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "modules/video_coding/codecs/test/stats.h"
#include "modules/video_coding/codecs/test/test_config.h"
#include "modules/video_coding/codecs/test/videoprocessor.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "test/gtest.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/frame_writer.h"

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

// Integration test for video processor. It does rate control and frame quality
// analysis using frame statistics collected by video processor and logs the
// results. If thresholds are specified it checks that corresponding metrics
// are in desirable range.
class VideoProcessorIntegrationTest : public testing::Test {
 protected:
  // Verifies that all H.264 keyframes contain SPS/PPS/IDR NALUs.
  class H264KeyframeChecker : public TestConfig::EncodedFrameChecker {
   public:
    void CheckEncodedFrame(webrtc::VideoCodecType codec,
                           const EncodedImage& encoded_frame) const override;
  };

  VideoProcessorIntegrationTest();
  ~VideoProcessorIntegrationTest() override;

  void ProcessFramesAndMaybeVerify(
      const std::vector<RateProfile>& rate_profiles,
      const std::vector<RateControlThresholds>* rc_thresholds,
      const std::vector<QualityThresholds>* quality_thresholds,
      const BitstreamThresholds* bs_thresholds,
      const VisualizationParams* visualization_params);

  // Config.
  TestConfig config_;

  // Can be used by all H.264 tests.
  const H264KeyframeChecker h264_keyframe_checker_;

 private:
  class CpuProcessTime;

  void CreateEncoderAndDecoder();
  void DestroyEncoderAndDecoder();
  void SetUpAndInitObjects(rtc::TaskQueue* task_queue,
                           const int initial_bitrate_kbps,
                           const int initial_framerate_fps,
                           const VisualizationParams* visualization_params);
  void ReleaseAndCloseObjects(rtc::TaskQueue* task_queue);

  void ProcessAllFrames(rtc::TaskQueue* task_queue,
                        const std::vector<RateProfile>& rate_profiles);
  void AnalyzeAllFrames(
      const std::vector<RateProfile>& rate_profiles,
      const std::vector<RateControlThresholds>* rc_thresholds,
      const std::vector<QualityThresholds>* quality_thresholds,
      const BitstreamThresholds* bs_thresholds);

  std::vector<FrameStatistic> ExtractLayerStats(
      size_t target_spatial_layer_number,
      size_t target_temporal_layer_number,
      size_t first_frame_number,
      size_t last_frame_number,
      bool combine_layers);

  void AnalyzeAndPrintStats(const std::vector<FrameStatistic>& stats,
                            float target_bitrate_kbps,
                            float target_framerate_fps,
                            float input_duration_sec,
                            const RateControlThresholds* rc_thresholds,
                            const QualityThresholds* quality_thresholds,
                            const BitstreamThresholds* bs_thresholds);
  void PrintFrameLevelStats(const std::vector<FrameStatistic>& stats) const;

  void PrintSettings() const;

  // Codecs.
  std::unique_ptr<VideoEncoder> encoder_;
  std::vector<std::unique_ptr<VideoDecoder>> decoders_;

  // Helper objects.
  std::unique_ptr<FrameReader> source_frame_reader_;
  std::vector<std::unique_ptr<IvfFileWriter>> encoded_frame_writers_;
  std::vector<std::unique_ptr<FrameWriter>> decoded_frame_writers_;
  std::vector<Stats> stats_;
  std::unique_ptr<VideoProcessor> processor_;
  std::unique_ptr<CpuProcessTime> cpu_process_time_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_
