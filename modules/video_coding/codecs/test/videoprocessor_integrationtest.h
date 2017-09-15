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

#include <memory>
#include <string>
#include <vector>

#include "common_types.h"
#include "media/engine/webrtcvideodecoderfactory.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "modules/video_coding/codecs/test/packet_manipulator.h"
#include "modules/video_coding/codecs/test/stats.h"
#include "modules/video_coding/codecs/test/videoprocessor.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "test/gtest.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/frame_writer.h"
#include "test/testsupport/packet_reader.h"

namespace webrtc {
namespace test {

// The sequence of bit rate and frame rate changes for the encoder, the frame
// number where the changes are made, and the total number of frames for the
// test.
struct RateProfile {
  static const int kMaxNumRateUpdates = 3;

  int target_bit_rate[kMaxNumRateUpdates];
  int input_frame_rate[kMaxNumRateUpdates];
  int frame_index_rate_update[kMaxNumRateUpdates + 1];
  int num_frames;
};

// Thresholds for the rate control metrics. The rate mismatch thresholds are
// defined as percentages. |max_time_hit_target| is defined as number of frames,
// after a rate update is made to the encoder, for the encoder to reach within
// |kPercTargetvsActualMismatch| of new target rate. The thresholds are defined
// for each rate update sequence.
struct RateControlThresholds {
  int max_num_dropped_frames;
  int max_key_frame_size_mismatch;
  int max_delta_frame_size_mismatch;
  int max_encoding_rate_mismatch;
  int max_time_hit_target;
  int num_spatial_resizes;
  int num_key_frames;
};

// Thresholds for the quality metrics.
struct QualityThresholds {
  QualityThresholds(double min_avg_psnr,
                    double min_min_psnr,
                    double min_avg_ssim,
                    double min_min_ssim)
      : min_avg_psnr(min_avg_psnr),
        min_min_psnr(min_min_psnr),
        min_avg_ssim(min_avg_ssim),
        min_min_ssim(min_min_ssim) {}
  double min_avg_psnr;
  double min_min_psnr;
  double min_avg_ssim;
  double min_min_ssim;
};

// Should video files be saved persistently to disk for post-run visualization?
struct VisualizationParams {
  bool save_encoded_ivf;
  bool save_decoded_y4m;
};

// Integration test for video processor. Encodes+decodes a clip and
// writes it to the output directory. After completion, quality metrics
// (PSNR and SSIM) and rate control metrics are computed and compared to given
// thresholds, to verify that the quality and encoder response is acceptable.
// The rate control tests allow us to verify the behavior for changing bit rate,
// changing frame rate, frame dropping/spatial resize, and temporal layers.
// The thresholds for the rate control metrics are set to be fairly
// conservative, so failure should only happen when some significant regression
// or breakdown occurs.
class VideoProcessorIntegrationTest : public testing::Test {
 protected:
  VideoProcessorIntegrationTest();
  ~VideoProcessorIntegrationTest() override;

  static void SetCodecSettings(TestConfig* config,
                               VideoCodecType codec_type,
                               int num_temporal_layers,
                               bool error_concealment_on,
                               bool denoising_on,
                               bool frame_dropper_on,
                               bool spatial_resize_on,
                               bool resilience_on,
                               int width,
                               int height);

  static void SetRateProfile(RateProfile* rate_profile,
                             int rate_update_index,
                             int bitrate_kbps,
                             int framerate_fps,
                             int frame_index_rate_update);

  static void AddRateControlThresholds(
      int max_num_dropped_frames,
      int max_key_frame_size_mismatch,
      int max_delta_frame_size_mismatch,
      int max_encoding_rate_mismatch,
      int max_time_hit_target,
      int num_spatial_resizes,
      int num_key_frames,
      std::vector<RateControlThresholds>* rc_thresholds);

  void ProcessFramesAndMaybeVerify(
      const RateProfile& rate_profile,
      const std::vector<RateControlThresholds>* rc_thresholds,
      const QualityThresholds* quality_thresholds,
      const VisualizationParams* visualization_params);

  // Config.
  TestConfig config_;

 private:
  static const int kMaxNumTemporalLayers = 3;

  void CreateEncoderAndDecoder();
  void DestroyEncoderAndDecoder();
  void SetUpAndInitObjects(rtc::TaskQueue* task_queue,
                           const int initial_bitrate_kbps,
                           const int initial_framerate_fps,
                           const VisualizationParams* visualization_params);
  void ReleaseAndCloseObjects(rtc::TaskQueue* task_queue);
  void UpdateRateControlMetrics(int frame_number);
  void PrintAndMaybeVerifyRateControlMetrics(
      int rate_update_index,
      const std::vector<RateControlThresholds>* rc_thresholds,
      const std::vector<int>& num_dropped_frames,
      const std::vector<int>& num_resize_actions);
  int TemporalLayerIndexForFrame(int frame_number) const;
  void ResetRateControlMetrics(int rate_update_index,
                               const RateProfile& rate_profile);

  // Codecs.
  std::unique_ptr<VideoEncoder> encoder_;
  std::unique_ptr<cricket::WebRtcVideoDecoderFactory> decoder_factory_;
  VideoDecoder* decoder_;

  // Helper objects.
  std::unique_ptr<FrameReader> analysis_frame_reader_;
  std::unique_ptr<FrameWriter> analysis_frame_writer_;
  std::unique_ptr<IvfFileWriter> encoded_frame_writer_;
  std::unique_ptr<FrameWriter> decoded_frame_writer_;
  PacketReader packet_reader_;
  std::unique_ptr<PacketManipulator> packet_manipulator_;
  Stats stats_;
  std::unique_ptr<VideoProcessor> processor_;

  // Quantities defined/updated for every encoder rate update.
  int num_frames_per_update_[kMaxNumTemporalLayers];
  float sum_frame_size_mismatch_[kMaxNumTemporalLayers];
  float sum_encoded_frame_size_[kMaxNumTemporalLayers];
  float encoding_bitrate_[kMaxNumTemporalLayers];
  float per_frame_bandwidth_[kMaxNumTemporalLayers];
  float bitrate_layer_[kMaxNumTemporalLayers];
  float framerate_layer_[kMaxNumTemporalLayers];
  int num_frames_total_;
  float sum_encoded_frame_size_total_;
  float encoding_bitrate_total_;
  float perc_encoding_rate_mismatch_;
  int num_frames_to_hit_target_;
  bool encoding_rate_within_target_;
  int bitrate_kbps_;
  int framerate_;
  float target_size_key_frame_initial_;
  float target_size_key_frame_;
  float sum_key_frame_size_mismatch_;
  int num_key_frames_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_
