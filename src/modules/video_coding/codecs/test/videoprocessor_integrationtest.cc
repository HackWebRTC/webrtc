/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gtest/gtest.h"

#include <math.h>

#include "modules/video_coding/codecs/interface/video_codec_interface.h"
#include "modules/video_coding/codecs/test/packet_manipulator.h"
#include "modules/video_coding/codecs/test/videoprocessor.h"
#include "modules/video_coding/codecs/vp8/main/interface/vp8.h"
#include "modules/video_coding/codecs/vp8/main/interface/vp8_common_types.h"
#include "modules/video_coding/main/interface/video_coding.h"
#include "testsupport/fileutils.h"
#include "testsupport/frame_reader.h"
#include "testsupport/frame_writer.h"
#include "testsupport/metrics/video_metrics.h"
#include "testsupport/packet_reader.h"
#include "typedefs.h"

namespace webrtc {

struct CodecConfigPars {
  float packet_loss;
  int num_temporal_layers;
  int key_frame_interval;
  bool error_concealment_on;
  bool denoising_on;
};

// Sequence used is foreman (CIF): may be better to use VGA for resize test.
const int kCIFWidth = 352;
const int kCIFHeight = 288;
const int kFrameRate = 30;
const int kStartBitRateKbps = 300;
const int kNbrFramesShort = 100;  // Some tests are run for shorter sequence.
const int kNbrFramesLong = 299;
const int kPercTargetvsActualMismatch = 20;

// Integration test for video processor. Encodes+decodes a clip and
// writes it to the output directory. After completion, quality metrics
// (PSNR and SSIM) and rate control metrics are computed to verify that the
// quality and encoder response is acceptable. The rate control tests allow us
// to verify the behavior for changing bitrate, changing frame rate, frame
// dropping/spatial resize, and temporal layers. The limits for the rate
// control metrics are set to be fairly conservative, so failure should only
// happen when some significant regression or breakdown occurs.
class VideoProcessorIntegrationTest: public testing::Test {
 protected:
  VideoEncoder* encoder_;
  VideoDecoder* decoder_;
  webrtc::test::FrameReader* frame_reader_;
  webrtc::test::FrameWriter* frame_writer_;
  webrtc::test::PacketReader packet_reader_;
  webrtc::test::PacketManipulator* packet_manipulator_;
  webrtc::test::Stats stats_;
  webrtc::test::TestConfig config_;
  VideoCodec codec_settings_;
  webrtc::test::VideoProcessor* processor_;

  // Quantities defined/updated for every encoder rate update.
  // Some quantities defined per temporal layer (at most 3 layers in this test).
  int num_frames_per_update_[3];
  float sum_frame_size_mismatch_[3];
  float sum_encoded_frame_size_[3];
  float encoding_bitrate_[3];
  float per_frame_bandwidth_[3];
  float bit_rate_layer_[3];
  float frame_rate_layer_[3];
  int num_frames_total_;
  float sum_encoded_frame_size_total_;
  float encoding_bitrate_total_;
  float perc_encoding_rate_mismatch_;
  int num_frames_to_hit_target_;
  bool encoding_rate_within_target_;
  int bit_rate_;
  int frame_rate_;
  int layer_;

  // Codec and network settings.
  float packet_loss_;
  int num_temporal_layers_;
  int key_frame_interval_;
  bool error_concealment_on_;
  bool denoising_on_;


  VideoProcessorIntegrationTest() {}
  virtual ~VideoProcessorIntegrationTest() {}

  void SetUpCodecConfig() {
    encoder_ = VP8Encoder::Create();
    decoder_ = VP8Decoder::Create();

    // CIF is currently used for all tests below.
    // Setup the TestConfig struct for processing of a clip in CIF resolution.
    config_.input_filename =
        webrtc::test::ResourcePath("foreman_cif", "yuv");
    config_.output_filename = webrtc::test::OutputPath() +
          "foreman_cif_short_video_codecs_test_framework_integrationtests.yuv";
    config_.frame_length_in_bytes = 3 * kCIFWidth * kCIFHeight / 2;
    config_.verbose = false;
    // Only allow encoder/decoder to use single core, for predictability.
    config_.use_single_core = true;
    // Key frame interval and packet loss are set for each test.
    config_.keyframe_interval = key_frame_interval_;
    config_.networking_config.packet_loss_probability = packet_loss_;

    // Get a codec configuration struct and configure it.
    VideoCodingModule::Codec(kVideoCodecVP8, &codec_settings_);
    config_.codec_settings = &codec_settings_;
    config_.codec_settings->startBitrate = kStartBitRateKbps;
    config_.codec_settings->width = kCIFWidth;
    config_.codec_settings->height = kCIFHeight;
    // These features may be set depending on the test.
    config_.codec_settings->codecSpecific.VP8.errorConcealmentOn =
        error_concealment_on_;
    config_.codec_settings->codecSpecific.VP8.denoisingOn =
        denoising_on_;
    config_.codec_settings->codecSpecific.VP8.numberOfTemporalLayers =
        num_temporal_layers_;

    frame_reader_ =
        new webrtc::test::FrameReaderImpl(config_.input_filename,
                                          config_.frame_length_in_bytes);
    frame_writer_ =
        new webrtc::test::FrameWriterImpl(config_.output_filename,
                                          config_.frame_length_in_bytes);
    ASSERT_TRUE(frame_reader_->Init());
    ASSERT_TRUE(frame_writer_->Init());

    packet_manipulator_ = new webrtc::test::PacketManipulatorImpl(
        &packet_reader_, config_.networking_config, config_.verbose);
    processor_ = new webrtc::test::VideoProcessorImpl(encoder_, decoder_,
                                                      frame_reader_,
                                                      frame_writer_,
                                                      packet_manipulator_,
                                                      config_, &stats_);
    ASSERT_TRUE(processor_->Init());
  }

  // Reset quantities after each encoder update, update the target
  // per-frame bandwidth.
  void ResetRateControlMetrics(int num_frames) {
    for (int i = 0; i < num_temporal_layers_; i++) {
      num_frames_per_update_[i] = 0;
      sum_frame_size_mismatch_[i] = 0.0f;
      sum_encoded_frame_size_[i] = 0.0f;
      encoding_bitrate_[i] = 0.0f;
      // Update layer per-frame-bandwidth.
      per_frame_bandwidth_[i] = static_cast<float>(bit_rate_layer_[i]) /
             static_cast<float>(frame_rate_layer_[i]);
    }
    num_frames_total_ = 0;
    sum_encoded_frame_size_total_ = 0.0f;
    encoding_bitrate_total_ = 0.0f;
    perc_encoding_rate_mismatch_ = 0.0f;
    num_frames_to_hit_target_ = num_frames;
    encoding_rate_within_target_ = false;
  }

  // For every encoded frame, update the rate control metrics.
  void UpdateRateControlMetrics(int frame_num,
                                int max_encoding_rate_mismatch) {
    int encoded_frame_size = processor_->EncodedFrameSize();
    float encoded_size_kbits = encoded_frame_size * 8.0f / 1000.0f;
    // Update layer data.
    // Ignore first frame (key frame), and any other key frames in the run,
    // for rate mismatch relative to per-frame bandwidth.
    // Note |frame_num| = 1 for the first frame in the run.
    if (frame_num > 1 && ((frame_num - 1) % key_frame_interval_ != 0 ||
        key_frame_interval_ < 0)) {
      sum_frame_size_mismatch_[layer_] += fabs(encoded_size_kbits -
                                               per_frame_bandwidth_[layer_]) /
                                               per_frame_bandwidth_[layer_];
    }
    sum_encoded_frame_size_[layer_] += encoded_size_kbits;
    // Encoding bitrate per layer: from the start of the update/run to the
    // current frame.
    encoding_bitrate_[layer_] = sum_encoded_frame_size_[layer_] *
        frame_rate_layer_[layer_] /
        num_frames_per_update_[layer_];
    // Total encoding rate: from the start of the update/run to current frame.
    sum_encoded_frame_size_total_ += encoded_size_kbits;
    encoding_bitrate_total_ = sum_encoded_frame_size_total_ * frame_rate_ /
        num_frames_total_;
    perc_encoding_rate_mismatch_ =  100 * fabs(encoding_bitrate_total_ -
                                               bit_rate_) / bit_rate_;
    if (perc_encoding_rate_mismatch_ < kPercTargetvsActualMismatch &&
        !encoding_rate_within_target_) {
      num_frames_to_hit_target_ = num_frames_total_;
      encoding_rate_within_target_ = true;
    }
  }

  // Verify expected behavior of rate control and print out data.
  void VerifyRateControl(int update_index,
                         int max_frame_size_mismatch,
                         int max_encoding_rate_mismatch,
                         int max_time_hit_target,
                         int max_num_dropped_frames,
                         int num_spatial_resizes) {
    int num_dropped_frames = processor_->NumberDroppedFrames();
    int num_resize_actions = processor_->NumberSpatialResizes();
    printf("For update #: %d,\n "
        " Target Bitrate: %d,\n"
        " Encoding bitrate: %f,\n"
        " Frame rate: %d \n",
        update_index, bit_rate_, encoding_bitrate_total_, frame_rate_);
    printf(" Number of frames to approach target rate = %d, \n"
           " Number of dropped frames = %d, \n"
           " Number of spatial resizes = %d, \n",
           num_frames_to_hit_target_, num_dropped_frames, num_resize_actions);
    EXPECT_LE(perc_encoding_rate_mismatch_, max_encoding_rate_mismatch);
    printf("\n");
    printf("Rates statistics for Layer data \n");
    for (int i = 0; i < num_temporal_layers_ ; i++) {
      printf("Layer #%d \n", i);
      int perc_frame_size_mismatch = 100 * sum_frame_size_mismatch_[i] /
        num_frames_per_update_[i];
      int perc_encoding_rate_mismatch = 100 * fabs(encoding_bitrate_[i] -
                                                   bit_rate_layer_[i]) /
                                                   bit_rate_layer_[i];
      printf(" Target Layer Bit rate: %f \n"
          " Layer frame rate: %f, \n"
          " Layer per frame bandwidth: %f, \n"
          " Layer Encoding bit rate: %f, \n"
          " Layer Percent frame size mismatch: %d,  \n"
          " Layer Percent encoding rate mismatch = %d, \n"
          " Number of frame processed per layer = %d \n",
          bit_rate_layer_[i], frame_rate_layer_[i], per_frame_bandwidth_[i],
          encoding_bitrate_[i], perc_frame_size_mismatch,
          perc_encoding_rate_mismatch, num_frames_per_update_[i]);
      EXPECT_LE(perc_frame_size_mismatch, max_frame_size_mismatch);
      EXPECT_LE(perc_encoding_rate_mismatch, max_encoding_rate_mismatch);
    }
    printf("\n");
    EXPECT_LE(num_frames_to_hit_target_, max_time_hit_target);
    EXPECT_LE(num_dropped_frames, max_num_dropped_frames);
    // Only if the spatial resizer is on in the codec wrapper do we expect to
    // get |num_spatial_resizes| resizes, otherwise we should not get any.
    EXPECT_TRUE(num_resize_actions == 0 ||
                num_resize_actions == num_spatial_resizes);
  }

  // Layer index corresponding to frame number, for up to 3 layers.
  void LayerIndexForFrame(int frame_number) {
    if (num_temporal_layers_ == 1) {
      layer_ = 0;
    } else if (num_temporal_layers_ == 2) {
        // layer 0:  0     2     4 ...
        // layer 1:     1     3
        if (frame_number % 2 == 0) {
          layer_ = 0;
        } else {
          layer_ = 1;
        }
    } else if (num_temporal_layers_ == 3) {
      // layer 0:  0            4            8 ...
      // layer 1:        2            6
      // layer 2:     1      3     5      7
      if (frame_number % 4 == 0) {
        layer_ = 0;
      } else if ((frame_number + 2) % 4 == 0) {
        layer_ = 1;
      } else if ((frame_number + 1) % 2 == 0) {
        layer_ = 2;
      }
    } else {
      assert(false);  // Only up to 3 layers.
    }
  }

  // Set the bitrate and frame rate per layer, for up to 3 layers.
  void SetLayerRates() {
    assert(num_temporal_layers_<= 3);
    for (int i = 0; i < num_temporal_layers_; i++) {
      float bit_rate_ratio =
          kVp8LayerRateAlloction[num_temporal_layers_ - 1][i];
      if (i > 0) {
        float bit_rate_delta_ratio = kVp8LayerRateAlloction
            [num_temporal_layers_ - 1][i] -
            kVp8LayerRateAlloction[num_temporal_layers_ - 1][i - 1];
        bit_rate_layer_[i] = bit_rate_ * bit_rate_delta_ratio;
      } else {
        bit_rate_layer_[i] = bit_rate_ * bit_rate_ratio;
      }
      frame_rate_layer_[i] = frame_rate_ / static_cast<float>(
          1 << (num_temporal_layers_ - 1));
    }
    if (num_temporal_layers_ == 3) {
      frame_rate_layer_[2] = frame_rate_ / 2.0f;
    }
  }

  void TearDown() {
    delete processor_;
    delete packet_manipulator_;
    delete frame_writer_;
    delete frame_reader_;
    delete decoder_;
    delete encoder_;
  }

  // Processes all frames in the clip and verifies the result.
  void ProcessFramesAndVerify(double minimum_avg_psnr,
                              double minimum_min_psnr,
                              double minimum_avg_ssim,
                              double minimum_min_ssim,
                              int* target_bit_rate,
                              int* input_frame_rate,
                              int* frame_index_rate_update,
                              int num_frames,
                              CodecConfigPars process,
                              int* max_frame_size_mismatch,
                              int* max_encoding_rate_mismatch,
                              int* max_time_hit_target,
                              int* max_num_dropped_frames,
                              int* num_spatial_resizes) {
    // Codec/config settings.
    packet_loss_ = process.packet_loss;
    key_frame_interval_ = process.key_frame_interval;
    num_temporal_layers_ = process.num_temporal_layers;
    error_concealment_on_ = process.error_concealment_on;
    denoising_on_ = process.denoising_on;
    SetUpCodecConfig();
    // Update the layers and the codec with the initial rates.
    bit_rate_ =  target_bit_rate[0];
    frame_rate_ = input_frame_rate[0];
    SetLayerRates();
    processor_->SetRates(bit_rate_, frame_rate_);
    // Process each frame, up to |num_frames|.
    int update_index = 0;
    ResetRateControlMetrics(frame_index_rate_update[update_index + 1]);
    int frame_number = 0;
    while (processor_->ProcessFrame(frame_number) &&
        frame_number < num_frames) {
      // Get the layer index for the frame |frame_number|.
      LayerIndexForFrame(frame_number);
      // Counter for whole sequence run.
      ++frame_number;
      // Counters for each rate update.
      ++num_frames_per_update_[layer_];
      ++num_frames_total_;
      UpdateRateControlMetrics(frame_number,
                               max_encoding_rate_mismatch[update_index]);
      // If we hit another/next update, verify stats for current state and
      // update layers and codec with new rates.
      if (frame_number == frame_index_rate_update[update_index + 1]) {
        VerifyRateControl(update_index,
                          max_frame_size_mismatch[update_index],
                          max_encoding_rate_mismatch[update_index],
                          max_time_hit_target[update_index],
                          max_num_dropped_frames[update_index],
                          num_spatial_resizes[update_index]);
        // Update layer rates and the codec with new rates.
        ++update_index;
        bit_rate_ =  target_bit_rate[update_index];
        frame_rate_ = input_frame_rate[update_index];
        SetLayerRates();
        ResetRateControlMetrics(frame_index_rate_update[update_index + 1]);
        processor_->SetRates(bit_rate_, frame_rate_);
      }
    }
    VerifyRateControl(update_index,
                      max_frame_size_mismatch[update_index],
                      max_encoding_rate_mismatch[update_index],
                      max_time_hit_target[update_index],
                      max_num_dropped_frames[update_index],
                      num_spatial_resizes[update_index]);
    EXPECT_EQ(num_frames, frame_number);
    EXPECT_EQ(num_frames + 1, static_cast<int>(stats_.stats_.size()));

    // Release encoder and decoder to make sure they have finished processing:
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Release());
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Release());
    // Close the files before we start using them for SSIM/PSNR calculations.
    frame_reader_->Close();
    frame_writer_->Close();

    // TODO(marpan): should compute these quality metrics per SetRates update.
    webrtc::test::QualityMetricsResult psnr_result, ssim_result;
    EXPECT_EQ(0, webrtc::test::I420MetricsFromFiles(
        config_.input_filename.c_str(),
        config_.output_filename.c_str(),
        config_.codec_settings->width,
        config_.codec_settings->height,
        &psnr_result,
        &ssim_result));
    printf("PSNR avg: %f, min: %f    SSIM avg: %f, min: %f\n",
           psnr_result.average, psnr_result.min,
           ssim_result.average, ssim_result.min);
    stats_.PrintSummary();
    EXPECT_GT(psnr_result.average, minimum_avg_psnr);
    EXPECT_GT(psnr_result.min, minimum_min_psnr);
    EXPECT_GT(ssim_result.average, minimum_avg_ssim);
    EXPECT_GT(ssim_result.min, minimum_min_ssim);
  }
};

// Run with no packet loss and fixed bitrate. Quality should be very high.
// One key frame (first frame only) in sequence. Setting |key_frame_interval|
// to -1 below means no periodic key frames in test.
TEST_F(VideoProcessorIntegrationTest, ProcessZeroPacketLoss) {
  // Bitrate and frame rate profile.
  int target_bit_rate[] = {500};   // kbps
  int input_frame_rate[] = {kFrameRate};
  int frame_index_rate_update[] = {0, kNbrFramesShort + 1};
  int num_frames = kNbrFramesShort;
  // Codec/network settings.
  CodecConfigPars process_settings;
  process_settings.packet_loss = 0.0f;
  process_settings.key_frame_interval = -1;
  process_settings.num_temporal_layers = 1;
  process_settings.error_concealment_on = true;
  process_settings.denoising_on = true;
  // Metrics for expected quality.
  double minimum_avg_psnr = 36;
  double minimum_min_psnr = 32;
  double minimum_avg_ssim = 0.9;
  double minimum_min_ssim = 0.9;
  // Metrics for rate control: rate mismatch metrics are defined as percentages.
  // |max_time_hit_target| is defined as number of frames, after a rate update
  // is made to the encoder, for the encoder to reach within
  // |kPercTargetvsActualMismatch| of new target rate.
  int max_num_dropped_frames[] = {0};
  int max_frame_size_mismatch[] = {20};
  int max_encoding_rate_mismatch[] = {15};
  int max_time_hit_target[] = {15};
  int num_spatial_resizes[] = {0};
  ProcessFramesAndVerify(minimum_avg_psnr, minimum_min_psnr,
                         minimum_avg_ssim, minimum_min_ssim,
                         target_bit_rate, input_frame_rate,
                         frame_index_rate_update, num_frames, process_settings,
                         max_frame_size_mismatch, max_encoding_rate_mismatch,
                         max_time_hit_target, max_num_dropped_frames,
                         num_spatial_resizes);
}

// Run with 5% packet loss and fixed bitrate. Quality should be a bit lower.
// One key frame (first frame only) in sequence.
TEST_F(VideoProcessorIntegrationTest, Process5PercentPacketLoss) {
  // Bitrate and frame rate profile, and codec settings.
  int target_bit_rate[] = {500};  // kbps
  int input_frame_rate[] = {kFrameRate};
  int frame_index_rate_update[] = {0, kNbrFramesShort + 1};
  int num_frames = kNbrFramesShort;
  // Codec/network settings.
  CodecConfigPars process_settings;
  process_settings.packet_loss = 0.05f;
  process_settings.key_frame_interval = -1;
  process_settings.num_temporal_layers = 1;
  process_settings.error_concealment_on = true;
  process_settings.denoising_on = true;
  // Metrics for expected quality.
  double minimum_avg_psnr = 21;
  double minimum_min_psnr = 16;
  double minimum_avg_ssim = 0.6;
  double minimum_min_ssim = 0.4;
  // Metrics for rate control: rate mismatch metrics are defined as percentages.
  // |max_time_hit_target| is defined as number of frames, after a rate update
  // is made to the encoder, for the encoder to reach within
  // |kPercTargetvsActualMismatch| of new target rate.
  int max_num_dropped_frames[] = {0};
  int max_frame_size_mismatch[] = {25};
  int max_encoding_rate_mismatch[] = {15};
  int max_time_hit_target[] = {15};
  int num_spatial_resizes[] = {0};
  ProcessFramesAndVerify(minimum_avg_psnr, minimum_min_psnr,
                         minimum_avg_ssim, minimum_min_ssim,
                         target_bit_rate, input_frame_rate,
                         frame_index_rate_update, num_frames, process_settings,
                         max_frame_size_mismatch, max_encoding_rate_mismatch,
                         max_time_hit_target, max_num_dropped_frames,
                         num_spatial_resizes);
}

// Run with 10% packet loss and fixed bitrate. Quality should be even lower.
// One key frame (first frame only) in sequence.
TEST_F(VideoProcessorIntegrationTest, Process10PercentPacketLoss) {
  // Bitrate and frame rate profile, and codec settings.
  int target_bit_rate[] = {500};  // kbps
  int input_frame_rate[] = {kFrameRate};
  int frame_index_rate_update[] = {0, kNbrFramesShort + 1};
  int num_frames = kNbrFramesShort;
  // Codec/network settings.
  CodecConfigPars process_settings;
  process_settings.packet_loss = 0.1f;
  process_settings.key_frame_interval = -1;
  process_settings.num_temporal_layers = 1;
  process_settings.error_concealment_on = true;
  process_settings.denoising_on = true;
  // Metrics for expected quality.
  double minimum_avg_psnr = 19;
  double minimum_min_psnr = 16;
  double minimum_avg_ssim = 0.5;
  double minimum_min_ssim = 0.35;
  // Metrics for rate control: rate mismatch metrics are defined as percentages.
  // |max_time_hit_target| is defined as number of frames, after a rate update
  // is made to the encoder, for the encoder to reach within
  // |kPercTargetvsActualMismatch| of new target rate.
  int max_num_dropped_frames[] = {0};
  int max_frame_size_mismatch[] = {25};
  int max_encoding_rate_mismatch[] = {15};
  int max_time_hit_target[] = {15};
  int num_spatial_resizes[] = {0};
  ProcessFramesAndVerify(minimum_avg_psnr, minimum_min_psnr,
                         minimum_avg_ssim, minimum_min_ssim,
                         target_bit_rate, input_frame_rate,
                         frame_index_rate_update, num_frames, process_settings,
                         max_frame_size_mismatch, max_encoding_rate_mismatch,
                         max_time_hit_target, max_num_dropped_frames,
                         num_spatial_resizes);
}

// Run with no packet loss, with varying bitrate (3 rate updates):
// low to high to medium. Check that quality and encoder response to the new
// target rate/per-frame bandwidth (for each rate update) is within limits.
// One key frame (first frame only) in sequence.
TEST_F(VideoProcessorIntegrationTest, ProcessNoLossChangeBitRate) {
  // Bitrate and frame rate profile, and codec settings.
  int target_bit_rate[] = {200, 800, 500};  // kbps
  int input_frame_rate[] = {30, 30, 30};
  int frame_index_rate_update[] = {0, 100, 200, kNbrFramesLong + 1};
  int num_frames = kNbrFramesLong;
  // Codec/network settings.
  CodecConfigPars process_settings;
  process_settings.packet_loss = 0.0f;
  process_settings.key_frame_interval = -1;
  process_settings.num_temporal_layers = 1;
  process_settings.error_concealment_on = true;
  process_settings.denoising_on = true;
  // Metrics for expected quality.
  double minimum_avg_psnr = 34;
  double minimum_min_psnr = 32;
  double minimum_avg_ssim = 0.85;
  double minimum_min_ssim = 0.8;
  // Metrics for rate control: rate mismatch metrics are defined as percentages.
  // |max_time_hit_target| is defined as number of frames, after a rate update
  // is made to the encoder, for the encoder to reach within
  // |kPercTargetvsActualMismatch| of new target rate.
  int max_num_dropped_frames[] = {0, 0, 0};
  int max_frame_size_mismatch[] = {20, 25, 25};
  int max_encoding_rate_mismatch[] = {10, 20, 15};
  int max_time_hit_target[] = {15, 10, 10};
  int num_spatial_resizes[] = {0, 0, 0};
  ProcessFramesAndVerify(minimum_avg_psnr, minimum_min_psnr,
                         minimum_avg_ssim, minimum_min_ssim,
                         target_bit_rate, input_frame_rate,
                         frame_index_rate_update, num_frames, process_settings,
                         max_frame_size_mismatch, max_encoding_rate_mismatch,
                         max_time_hit_target, max_num_dropped_frames,
                         num_spatial_resizes);
}

// Run with no packet loss, with an update (decrease) in frame rate.
// Lower frame rate means higher per-frame-bandwidth, so easier to encode.
// At the bitrate in this test, this means better rate control after the
// update(s) to lower frame rate. So expect less frame drops, and max values
// for the rate control metrics can be lower. One key frame (first frame only).
// Note: quality after update should be higher but we currently compute quality
// metrics avergaed over whole sequence run.
TEST_F(VideoProcessorIntegrationTest, ProcessNoLossChangeFrameRateFrameDrop) {
  config_.networking_config.packet_loss_probability = 0;
  // Bitrate and frame rate profile, and codec settings.
  int target_bit_rate[] = {80, 80, 80};  // kbps
  int input_frame_rate[] = {30, 15, 10};
  int frame_index_rate_update[] = {0, 100, 200, kNbrFramesLong + 1};
  int num_frames = kNbrFramesLong;
  // Codec/network settings.
  CodecConfigPars process_settings;
  process_settings.packet_loss = 0.0f;
  process_settings.key_frame_interval = -1;
  process_settings.num_temporal_layers = 1;
  process_settings.error_concealment_on = true;
  process_settings.denoising_on = true;
  // Metrics for expected quality.
  double minimum_avg_psnr = 31;
  double minimum_min_psnr = 24;
  double minimum_avg_ssim = 0.8;
  double minimum_min_ssim = 0.7;
  // Metrics for rate control: rate mismatch metrics are defined as percentages.
  // |max_time_hit_target| is defined as number of frames, after a rate update
  // is made to the encoder, for the encoder to reach within
  // |kPercTargetvsActualMismatch| of new target rate.
  int max_num_dropped_frames[] = {25, 10, 0};
  int max_frame_size_mismatch[] = {60, 25, 20};
  int max_encoding_rate_mismatch[] = {40, 10, 10};
  // At the low per-frame bandwidth for this scene, encoder can't hit target
  // rate before first update.
  int max_time_hit_target[] = {100, 40, 10};
  int num_spatial_resizes[] = {0, 0, 0};
  ProcessFramesAndVerify(minimum_avg_psnr, minimum_min_psnr,
                         minimum_avg_ssim, minimum_min_ssim,
                         target_bit_rate, input_frame_rate,
                         frame_index_rate_update, num_frames, process_settings,
                         max_frame_size_mismatch, max_encoding_rate_mismatch,
                         max_time_hit_target, max_num_dropped_frames,
                         num_spatial_resizes);
}

// Run with no packet loss, at low bitrate, then increase rate somewhat.
// Key frame is thrown in every 120 frames. Can expect some frame drops after
// key frame, even at high rate. If resizer is on, expect spatial resize down
// at first key frame, and back up at second key frame. Expected values for
// quality and rate control in this test are such that the test should pass
// with resizing on or off. Error_concealment is off in this test since there
// is a memory leak with resizing and error concealment.
TEST_F(VideoProcessorIntegrationTest, ProcessNoLossSpatialResizeFrameDrop) {
  config_.networking_config.packet_loss_probability = 0;
  // Bitrate and frame rate profile, and codec settings.
  int target_bit_rate[] = {80, 200, 200};  // kbps
  int input_frame_rate[] = {30, 30, 30};
  int frame_index_rate_update[] = {0, 120, 240, kNbrFramesLong + 1};
  int num_frames = kNbrFramesLong;
  // Codec/network settings.
  CodecConfigPars process_settings;
  process_settings.packet_loss = 0.0f;
  process_settings.key_frame_interval = 120;
  process_settings.num_temporal_layers = 1;
  process_settings.error_concealment_on = false;
  process_settings.denoising_on = true;
  // Metrics for expected quality.: lower quality on average from up-sampling
  // the down-sampled portion of the run, in case resizer is on.
  double minimum_avg_psnr = 29;
  double minimum_min_psnr = 20;
  double minimum_avg_ssim = 0.75;
  double minimum_min_ssim = 0.6;
  // Metrics for rate control: rate mismatch metrics are defined as percentages.
  // |max_time_hit_target| is defined as number of frames, after a rate update
  // is made to the encoder, for the encoder to reach within
  // |kPercTargetvsActualMismatch| of new target rate.
  int max_num_dropped_frames[] = {25, 15, 0};
  int max_frame_size_mismatch[] = {60, 30, 30};
  int max_encoding_rate_mismatch[] = {30, 20, 15};
  // At this low rate for this scene, can't hit target rate before first update.
  int max_time_hit_target[] = {120, 15, 25};
  int num_spatial_resizes[] = {0, 1, 1};
  ProcessFramesAndVerify(minimum_avg_psnr, minimum_min_psnr,
                         minimum_avg_ssim, minimum_min_ssim,
                         target_bit_rate, input_frame_rate,
                         frame_index_rate_update, num_frames, process_settings,
                         max_frame_size_mismatch, max_encoding_rate_mismatch,
                         max_time_hit_target, max_num_dropped_frames,
                         num_spatial_resizes);
}

// Run with no packet loss, with 3 temporal layers, with a rate update in the
// middle of the sequence. The max values for the frame size mismatch and
// encoding rate mismatch are applied to each layer.
// No dropped frames in this test, and the denoiser is off for temporal layers.
// One key frame (first frame only) in sequence, so no spatial resizing.
TEST_F(VideoProcessorIntegrationTest, ProcessNoLossTemporalLayers) {
  config_.networking_config.packet_loss_probability = 0;
  // Bitrate and frame rate profile, and codec settings.
  int target_bit_rate[] = {200, 400};  // kbps
  int input_frame_rate[] = {30, 30};
  int frame_index_rate_update[] = {0, 150, kNbrFramesLong + 1};
  int num_frames = kNbrFramesLong;
  // Codec/network settings.
  CodecConfigPars process_settings;
  process_settings.packet_loss = 0.0f;
  process_settings.key_frame_interval = -1;
  process_settings.num_temporal_layers = 3;
  process_settings.error_concealment_on = true;
  process_settings.denoising_on = false;
  // Metrics for expected quality.
  double minimum_avg_psnr = 33;
  double minimum_min_psnr = 30;
  double minimum_avg_ssim = 0.85;
  double minimum_min_ssim = 0.80;
  // Metrics for rate control: rate mismatch metrics are defined as percentages.
  // |max_time_hit_target| is defined as number of frames, after a rate update
  // is made to the encoder, for the encoder to reach within
  // |kPercTargetvsActualMismatch| of new target rate.
  int max_num_dropped_frames[] = {0, 0};
  int max_frame_size_mismatch[] = {30, 30};
  int max_encoding_rate_mismatch[] = {10, 12};
  int max_time_hit_target[] = {15, 15};
  int num_spatial_resizes[] = {0, 0};
  ProcessFramesAndVerify(minimum_avg_psnr, minimum_min_psnr,
                         minimum_avg_ssim, minimum_min_ssim,
                         target_bit_rate, input_frame_rate,
                         frame_index_rate_update, num_frames, process_settings,
                         max_frame_size_mismatch, max_encoding_rate_mismatch,
                         max_time_hit_target, max_num_dropped_frames,
                         num_spatial_resizes);
}
}  // namespace webrtc
