/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_

#include <math.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(WEBRTC_ANDROID)
#include "webrtc/modules/video_coding/codecs/test/android_test_initializer.h"
#include "webrtc/sdk/android/src/jni/androidmediadecoder_jni.h"
#include "webrtc/sdk/android/src/jni/androidmediaencoder_jni.h"
#elif defined(WEBRTC_IOS)
#include "webrtc/modules/video_coding/codecs/test/objc_codec_h264_test.h"
#endif

#include "webrtc/media/engine/internaldecoderfactory.h"
#include "webrtc/media/engine/internalencoderfactory.h"
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"
#include "webrtc/media/engine/webrtcvideoencoderfactory.h"
#include "webrtc/modules/video_coding/codecs/test/packet_manipulator.h"
#include "webrtc/modules/video_coding/codecs/test/videoprocessor.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8_common_types.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/utility/ivf_file_writer.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/file.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/ptr_util.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/frame_reader.h"
#include "webrtc/test/testsupport/frame_writer.h"
#include "webrtc/test/testsupport/metrics/video_metrics.h"
#include "webrtc/test/testsupport/packet_reader.h"
#include "webrtc/test/video_codec_settings.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {

const int kMaxNumRateUpdates = 3;
const int kMaxNumTemporalLayers = 3;

const int kPercTargetvsActualMismatch = 20;
const int kBaseKeyFrameInterval = 3000;

// Parameters from VP8 wrapper, which control target size of key frames.
const float kInitialBufferSize = 0.5f;
const float kOptimalBufferSize = 0.6f;
const float kScaleKeyFrameSize = 0.5f;

// Thresholds for the quality metrics. Defaults are maximally minimal.
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

// The sequence of bit rate and frame rate changes for the encoder, the frame
// number where the changes are made, and the total number of frames for the
// test.
struct RateProfile {
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
  VideoProcessorIntegrationTest() {
#if defined(WEBRTC_VIDEOPROCESSOR_INTEGRATIONTEST_HW_CODECS_ENABLED) && \
    defined(WEBRTC_ANDROID)
    InitializeAndroidObjects();
#endif
  }
  ~VideoProcessorIntegrationTest() override = default;

  void CreateEncoderAndDecoder() {
    if (config_.hw_codec) {
#if defined(WEBRTC_VIDEOPROCESSOR_INTEGRATIONTEST_HW_CODECS_ENABLED)
#if defined(WEBRTC_ANDROID)
      encoder_factory_.reset(new webrtc_jni::MediaCodecVideoEncoderFactory());
      decoder_factory_.reset(new webrtc_jni::MediaCodecVideoDecoderFactory());
#elif defined(WEBRTC_IOS)
      EXPECT_EQ(kVideoCodecH264, config_.codec_settings.codecType)
          << "iOS HW codecs only support H264.";
      encoder_factory_ = CreateObjCEncoderFactory();
      decoder_factory_ = CreateObjCDecoderFactory();
#else
      RTC_NOTREACHED() << "Only support HW codecs on Android and iOS.";
#endif
#endif  // WEBRTC_VIDEOPROCESSOR_INTEGRATIONTEST_HW_CODECS_ENABLED
    } else {
      // SW codecs.
      encoder_factory_.reset(new cricket::InternalEncoderFactory());
      decoder_factory_.reset(new cricket::InternalDecoderFactory());
    }

    switch (config_.codec_settings.codecType) {
      case kVideoCodecVP8:
        encoder_ = encoder_factory_->CreateVideoEncoder(
            cricket::VideoCodec(cricket::kVp8CodecName));
        decoder_ = decoder_factory_->CreateVideoDecoder(kVideoCodecVP8);
        break;
      case kVideoCodecVP9:
        encoder_ = encoder_factory_->CreateVideoEncoder(
            cricket::VideoCodec(cricket::kVp9CodecName));
        decoder_ = decoder_factory_->CreateVideoDecoder(kVideoCodecVP9);
        break;
      case kVideoCodecH264:
        // TODO(brandtr): Generalize so that we support multiple profiles here.
        encoder_ = encoder_factory_->CreateVideoEncoder(
            cricket::VideoCodec(cricket::kH264CodecName));
        decoder_ = decoder_factory_->CreateVideoDecoder(kVideoCodecH264);
        break;
      default:
        RTC_NOTREACHED();
        break;
    }

    EXPECT_TRUE(encoder_) << "Encoder not successfully created.";
    EXPECT_TRUE(decoder_) << "Decoder not successfully created.";
  }

  void DestroyEncoderAndDecoder() {
    encoder_factory_->DestroyVideoEncoder(encoder_);
    decoder_factory_->DestroyVideoDecoder(decoder_);
  }

  void SetUpObjects(const VisualizationParams* visualization_params,
                    const int initial_bitrate_kbps,
                    const int initial_framerate_fps) {
    CreateEncoderAndDecoder();

    // Create file objects for quality analysis.
    analysis_frame_reader_.reset(new YuvFrameReaderImpl(
        config_.input_filename, config_.codec_settings.width,
        config_.codec_settings.height));
    analysis_frame_writer_.reset(new YuvFrameWriterImpl(
        config_.output_filename, config_.codec_settings.width,
        config_.codec_settings.height));
    EXPECT_TRUE(analysis_frame_reader_->Init());
    EXPECT_TRUE(analysis_frame_writer_->Init());

    if (visualization_params) {
      const std::string codec_name =
          CodecTypeToPayloadString(config_.codec_settings.codecType);
      const std::string implementation_type = config_.hw_codec ? "hw" : "sw";
      // clang-format off
      const std::string output_filename_base =
          OutputPath() + config_.filename + "-" +
          codec_name + "-" + implementation_type + "-" +
          std::to_string(initial_bitrate_kbps);
      // clang-format on
      if (visualization_params->save_encoded_ivf) {
        rtc::File post_encode_file =
            rtc::File::Create(output_filename_base + ".ivf");
        encoded_frame_writer_ =
            IvfFileWriter::Wrap(std::move(post_encode_file), 0);
      }
      if (visualization_params->save_decoded_y4m) {
        decoded_frame_writer_.reset(new Y4mFrameWriterImpl(
            output_filename_base + ".y4m", config_.codec_settings.width,
            config_.codec_settings.height, initial_framerate_fps));
        EXPECT_TRUE(decoded_frame_writer_->Init());
      }
    }

    packet_manipulator_.reset(new PacketManipulatorImpl(
        &packet_reader_, config_.networking_config, config_.verbose));
    processor_ = rtc::MakeUnique<VideoProcessor>(
        encoder_, decoder_, analysis_frame_reader_.get(),
        analysis_frame_writer_.get(), packet_manipulator_.get(), config_,
        &stats_, encoded_frame_writer_.get(), decoded_frame_writer_.get());
    processor_->Init();
  }

  // Reset quantities after each encoder update, update the target per-frame
  // bandwidth.
  void ResetRateControlMetrics(int num_frames_to_hit_target) {
    const int num_temporal_layers =
        NumberOfTemporalLayers(config_.codec_settings);
    for (int i = 0; i < num_temporal_layers; i++) {
      num_frames_per_update_[i] = 0;
      sum_frame_size_mismatch_[i] = 0.0f;
      sum_encoded_frame_size_[i] = 0.0f;
      encoding_bitrate_[i] = 0.0f;
      // Update layer per-frame-bandwidth.
      per_frame_bandwidth_[i] = static_cast<float>(bitrate_layer_[i]) /
                                static_cast<float>(framerate_layer_[i]);
    }
    // Set maximum size of key frames, following setting in the VP8 wrapper.
    float max_key_size = kScaleKeyFrameSize * kOptimalBufferSize * framerate_;
    // We don't know exact target size of the key frames (except for first one),
    // but the minimum in libvpx is ~|3 * per_frame_bandwidth| and maximum is
    // set by |max_key_size_ * per_frame_bandwidth|. Take middle point/average
    // as reference for mismatch. Note key frames always correspond to base
    // layer frame in this test.
    target_size_key_frame_ = 0.5 * (3 + max_key_size) * per_frame_bandwidth_[0];
    num_frames_total_ = 0;
    sum_encoded_frame_size_total_ = 0.0f;
    encoding_bitrate_total_ = 0.0f;
    perc_encoding_rate_mismatch_ = 0.0f;
    num_frames_to_hit_target_ = num_frames_to_hit_target;
    encoding_rate_within_target_ = false;
    sum_key_frame_size_mismatch_ = 0.0;
    num_key_frames_ = 0;
  }

  // For every encoded frame, update the rate control metrics.
  void UpdateRateControlMetrics(int frame_number) {
    RTC_CHECK_GE(frame_number, 0);

    FrameType frame_type = stats_.stats_[frame_number].frame_type;
    float encoded_size_kbits =
        stats_.stats_[frame_number].encoded_frame_length_in_bytes * 8.0f /
        1000.0f;
    const int tl_idx = TemporalLayerIndexForFrame(frame_number);

    // Update layer data.
    // Update rate mismatch relative to per-frame bandwidth for delta frames.
    if (frame_type == kVideoFrameDelta) {
      // TODO(marpan): Should we count dropped (zero size) frames in mismatch?
      sum_frame_size_mismatch_[tl_idx] +=
          fabs(encoded_size_kbits - per_frame_bandwidth_[tl_idx]) /
          per_frame_bandwidth_[tl_idx];
    } else {
      float target_size = (frame_number == 0) ? target_size_key_frame_initial_
                                              : target_size_key_frame_;
      sum_key_frame_size_mismatch_ +=
          fabs(encoded_size_kbits - target_size) / target_size;
      num_key_frames_ += 1;
    }
    sum_encoded_frame_size_[tl_idx] += encoded_size_kbits;
    // Encoding bit rate per temporal layer: from the start of the update/run
    // to the current frame.
    encoding_bitrate_[tl_idx] = sum_encoded_frame_size_[tl_idx] *
                                framerate_layer_[tl_idx] /
                                num_frames_per_update_[tl_idx];
    // Total encoding rate: from the start of the update/run to current frame.
    sum_encoded_frame_size_total_ += encoded_size_kbits;
    encoding_bitrate_total_ =
        sum_encoded_frame_size_total_ * framerate_ / num_frames_total_;
    perc_encoding_rate_mismatch_ =
        100 * fabs(encoding_bitrate_total_ - bitrate_kbps_) / bitrate_kbps_;
    if (perc_encoding_rate_mismatch_ < kPercTargetvsActualMismatch &&
        !encoding_rate_within_target_) {
      num_frames_to_hit_target_ = num_frames_total_;
      encoding_rate_within_target_ = true;
    }
  }

  // Verify expected behavior of rate control and print out data.
  void PrintAndMaybeVerifyRateControlMetrics(
      int rate_update_index,
      const std::vector<RateControlThresholds>* rc_thresholds) {
    int num_dropped_frames = processor_->NumberDroppedFrames();
    int num_resize_actions = processor_->NumberSpatialResizes();
    printf(
        "Rate update #%d:\n"
        " Target bitrate         : %d\n"
        " Encoded bitrate        : %f\n"
        " Frame rate             : %d\n",
        rate_update_index, bitrate_kbps_, encoding_bitrate_total_, framerate_);
    printf(
        " # processed frames     : %d\n"
        " # frames to convergence: %d\n"
        " # dropped frames       : %d\n"
        " # spatial resizes      : %d\n",
        num_frames_total_, num_frames_to_hit_target_, num_dropped_frames,
        num_resize_actions);

    const RateControlThresholds* rc_threshold = nullptr;
    if (rc_thresholds) {
      rc_threshold = &(*rc_thresholds)[rate_update_index];

      EXPECT_LE(perc_encoding_rate_mismatch_,
                rc_threshold->max_encoding_rate_mismatch);
    }
    if (num_key_frames_ > 0) {
      int perc_key_frame_size_mismatch =
          100 * sum_key_frame_size_mismatch_ / num_key_frames_;
      printf(
          " # key frames           : %d\n"
          " Key frame rate mismatch: %d\n",
          num_key_frames_, perc_key_frame_size_mismatch);
      if (rc_threshold) {
        EXPECT_LE(perc_key_frame_size_mismatch,
                  rc_threshold->max_key_frame_size_mismatch);
      }
    }

    const int num_temporal_layers =
        NumberOfTemporalLayers(config_.codec_settings);
    for (int i = 0; i < num_temporal_layers; i++) {
      int perc_frame_size_mismatch =
          100 * sum_frame_size_mismatch_[i] / num_frames_per_update_[i];
      int perc_encoding_rate_mismatch =
          100 * fabs(encoding_bitrate_[i] - bitrate_layer_[i]) /
          bitrate_layer_[i];
      printf(
          " Temporal layer #%d:\n"
          "  Target layer bitrate                : %f\n"
          "  Layer frame rate                    : %f\n"
          "  Layer per frame bandwidth           : %f\n"
          "  Layer encoding bitrate              : %f\n"
          "  Layer percent frame size mismatch   : %d\n"
          "  Layer percent encoding rate mismatch: %d\n"
          "  # frames processed per layer        : %d\n",
          i, bitrate_layer_[i], framerate_layer_[i], per_frame_bandwidth_[i],
          encoding_bitrate_[i], perc_frame_size_mismatch,
          perc_encoding_rate_mismatch, num_frames_per_update_[i]);
      if (rc_threshold) {
        EXPECT_LE(perc_frame_size_mismatch,
                  rc_threshold->max_delta_frame_size_mismatch);
        EXPECT_LE(perc_encoding_rate_mismatch,
                  rc_threshold->max_encoding_rate_mismatch);
      }
    }
    printf("\n");

    if (rc_threshold) {
      EXPECT_LE(num_frames_to_hit_target_, rc_threshold->max_time_hit_target);
      EXPECT_LE(num_dropped_frames, rc_threshold->max_num_dropped_frames);
      EXPECT_EQ(rc_threshold->num_spatial_resizes, num_resize_actions);
      EXPECT_EQ(rc_threshold->num_key_frames, num_key_frames_);
    }
  }

  static void VerifyQuality(const QualityMetricsResult& psnr_result,
                            const QualityMetricsResult& ssim_result,
                            const QualityThresholds& quality_thresholds) {
    EXPECT_GT(psnr_result.average, quality_thresholds.min_avg_psnr);
    EXPECT_GT(psnr_result.min, quality_thresholds.min_min_psnr);
    EXPECT_GT(ssim_result.average, quality_thresholds.min_avg_ssim);
    EXPECT_GT(ssim_result.min, quality_thresholds.min_min_ssim);
  }

  void VerifyQpParser(int frame_number) {
    if (!config_.hw_codec &&
        (config_.codec_settings.codecType == kVideoCodecVP8 ||
         config_.codec_settings.codecType == kVideoCodecVP9)) {
      EXPECT_EQ(processor_->GetQpFromEncoder(frame_number),
                processor_->GetQpFromBitstream(frame_number));
    }
  }

  static int NumberOfTemporalLayers(const VideoCodec& codec_settings) {
    if (codec_settings.codecType == kVideoCodecVP8) {
      return codec_settings.VP8().numberOfTemporalLayers;
    } else if (codec_settings.codecType == kVideoCodecVP9) {
      return codec_settings.VP9().numberOfTemporalLayers;
    } else {
      return 1;
    }
  }

  // Temporal layer index corresponding to frame number, for up to 3 layers.
  int TemporalLayerIndexForFrame(int frame_number) {
    const int num_temporal_layers =
        NumberOfTemporalLayers(config_.codec_settings);
    int tl_idx = -1;
    switch (num_temporal_layers) {
      case 1:
        tl_idx = 0;
        break;
      case 2:
        // temporal layer 0:  0     2     4 ...
        // temporal layer 1:     1     3
        tl_idx = (frame_number % 2 == 0) ? 0 : 1;
        break;
      case 3:
        // temporal layer 0:  0            4            8 ...
        // temporal layer 1:        2            6
        // temporal layer 2:     1      3     5      7
        if (frame_number % 4 == 0) {
          tl_idx = 0;
        } else if ((frame_number + 2) % 4 == 0) {
          tl_idx = 1;
        } else if ((frame_number + 1) % 2 == 0) {
          tl_idx = 2;
        }
        break;
      default:
        RTC_NOTREACHED();
        break;
    }
    return tl_idx;
  }

  // Set the bit rate and frame rate per temporal layer, for up to 3 layers.
  void SetTemporalLayerRates() {
    const int num_temporal_layers =
        NumberOfTemporalLayers(config_.codec_settings);
    RTC_DCHECK_LE(num_temporal_layers, kMaxNumTemporalLayers);
    for (int i = 0; i < num_temporal_layers; i++) {
      float bit_rate_ratio = kVp8LayerRateAlloction[num_temporal_layers - 1][i];
      if (i > 0) {
        float bit_rate_delta_ratio =
            kVp8LayerRateAlloction[num_temporal_layers - 1][i] -
            kVp8LayerRateAlloction[num_temporal_layers - 1][i - 1];
        bitrate_layer_[i] = bitrate_kbps_ * bit_rate_delta_ratio;
      } else {
        bitrate_layer_[i] = bitrate_kbps_ * bit_rate_ratio;
      }
      framerate_layer_[i] =
          framerate_ / static_cast<float>(1 << (num_temporal_layers - 1));
    }
    if (num_temporal_layers == 3) {
      framerate_layer_[2] = framerate_ / 2.0f;
    }
  }

  // Processes all frames in the clip and verifies the result.
  void ProcessFramesAndMaybeVerify(
      const RateProfile& rate_profile,
      const std::vector<RateControlThresholds>* rc_thresholds,
      const QualityThresholds* quality_thresholds,
      const VisualizationParams* visualization_params) {
    config_.codec_settings.startBitrate = rate_profile.target_bit_rate[0];
    SetUpObjects(visualization_params, rate_profile.target_bit_rate[0],
                 rate_profile.input_frame_rate[0]);

    // Set initial rates.
    bitrate_kbps_ = rate_profile.target_bit_rate[0];
    framerate_ = rate_profile.input_frame_rate[0];
    SetTemporalLayerRates();
    // Set the initial target size for key frame.
    target_size_key_frame_initial_ =
        0.5 * kInitialBufferSize * bitrate_layer_[0];
    processor_->SetRates(bitrate_kbps_, framerate_);

    // Process each frame, up to |num_frames|.
    int frame_number = 0;
    int update_index = 0;
    int num_frames = rate_profile.num_frames;
    ResetRateControlMetrics(
        rate_profile.frame_index_rate_update[update_index + 1]);

    if (config_.batch_mode) {
      // In batch mode, we calculate the metrics for all frames after all frames
      // have been sent for encoding.

      // TODO(brandtr): Refactor "frame number accounting" so we don't have to
      // call ProcessFrame num_frames+1 times here.
      for (frame_number = 0; frame_number <= num_frames; ++frame_number) {
        processor_->ProcessFrame(frame_number);
      }

      for (frame_number = 0; frame_number < num_frames; ++frame_number) {
        const int tl_idx = TemporalLayerIndexForFrame(frame_number);
        ++num_frames_per_update_[tl_idx];
        ++num_frames_total_;
        UpdateRateControlMetrics(frame_number);
      }
    } else {
      // In online mode, we calculate the metrics for a given frame right after
      // it has been sent for encoding.

      if (config_.hw_codec) {
        LOG(LS_WARNING) << "HW codecs should mostly be run in batch mode, "
                           "since they may be pipelining.";
      }

      while (frame_number < num_frames) {
        processor_->ProcessFrame(frame_number);
        VerifyQpParser(frame_number);
        const int tl_idx = TemporalLayerIndexForFrame(frame_number);
        ++num_frames_per_update_[tl_idx];
        ++num_frames_total_;
        UpdateRateControlMetrics(frame_number);

        ++frame_number;

        // If we hit another/next update, verify stats for current state and
        // update layers and codec with new rates.
        if (frame_number ==
            rate_profile.frame_index_rate_update[update_index + 1]) {
          PrintAndMaybeVerifyRateControlMetrics(update_index, rc_thresholds);

          // Update layer rates and the codec with new rates.
          ++update_index;
          bitrate_kbps_ = rate_profile.target_bit_rate[update_index];
          framerate_ = rate_profile.input_frame_rate[update_index];
          SetTemporalLayerRates();
          ResetRateControlMetrics(
              rate_profile.frame_index_rate_update[update_index + 1]);
          processor_->SetRates(bitrate_kbps_, framerate_);
        }
      }
      // TODO(brandtr): Refactor "frame number accounting" so we don't have to
      // call ProcessFrame one extra time here.
      processor_->ProcessFrame(frame_number);
    }

    // Verify rate control metrics for all frames (if in batch mode), or for all
    // frames since the last rate update (if not in batch mode).
    PrintAndMaybeVerifyRateControlMetrics(update_index, rc_thresholds);
    EXPECT_EQ(num_frames, frame_number);
    EXPECT_EQ(num_frames + 1, static_cast<int>(stats_.stats_.size()));

    // Release encoder and decoder to make sure they have finished processing.
    processor_->Release();
    DestroyEncoderAndDecoder();

    // Close the analysis files before we use them for SSIM/PSNR calculations.
    analysis_frame_reader_->Close();
    analysis_frame_writer_->Close();

    // Close visualization files.
    if (encoded_frame_writer_) {
      EXPECT_TRUE(encoded_frame_writer_->Close());
    }
    if (decoded_frame_writer_) {
      decoded_frame_writer_->Close();
    }

    // TODO(marpan): Should compute these quality metrics per SetRates update.
    QualityMetricsResult psnr_result, ssim_result;
    EXPECT_EQ(0, I420MetricsFromFiles(config_.input_filename.c_str(),
                                      config_.output_filename.c_str(),
                                      config_.codec_settings.width,
                                      config_.codec_settings.height,
                                      &psnr_result, &ssim_result));
    if (quality_thresholds) {
      VerifyQuality(psnr_result, ssim_result, *quality_thresholds);
    }
    stats_.PrintSummary();
    printf("PSNR avg: %f, min: %f\nSSIM avg: %f, min: %f\n",
           psnr_result.average, psnr_result.min, ssim_result.average,
           ssim_result.min);
    printf("\n");

    // Remove analysis file.
    if (remove(config_.output_filename.c_str()) < 0) {
      fprintf(stderr, "Failed to remove temporary file!\n");
    }
  }

  static void SetTestConfig(TestConfig* config,
                            bool hw_codec,
                            bool use_single_core,
                            float packet_loss_probability,
                            std::string filename,
                            bool verbose_logging,
                            bool batch_mode) {
    config->filename = filename;
    config->input_filename = ResourcePath(filename, "yuv");
    // Generate an output filename in a safe way.
    config->output_filename =
        TempFilename(OutputPath(), "videoprocessor_integrationtest");
    config->networking_config.packet_loss_probability = packet_loss_probability;
    config->use_single_core = use_single_core;
    config->verbose = verbose_logging;
    config->hw_codec = hw_codec;
    config->batch_mode = batch_mode;
  }

  static void SetCodecSettings(TestConfig* config,
                               VideoCodecType codec_type,
                               int num_temporal_layers,
                               bool error_concealment_on,
                               bool denoising_on,
                               bool frame_dropper_on,
                               bool spatial_resize_on,
                               bool resilience_on,
                               int width,
                               int height) {
    webrtc::test::CodecSettings(codec_type, &config->codec_settings);
    config->codec_settings.width = width;
    config->codec_settings.height = height;
    switch (config->codec_settings.codecType) {
      case kVideoCodecVP8:
        config->codec_settings.VP8()->resilience =
            resilience_on ? kResilientStream : kResilienceOff;
        config->codec_settings.VP8()->numberOfTemporalLayers =
            num_temporal_layers;
        config->codec_settings.VP8()->denoisingOn = denoising_on;
        config->codec_settings.VP8()->errorConcealmentOn = error_concealment_on;
        config->codec_settings.VP8()->automaticResizeOn = spatial_resize_on;
        config->codec_settings.VP8()->frameDroppingOn = frame_dropper_on;
        config->codec_settings.VP8()->keyFrameInterval = kBaseKeyFrameInterval;
        break;
      case kVideoCodecVP9:
        config->codec_settings.VP9()->resilienceOn = resilience_on;
        config->codec_settings.VP9()->numberOfTemporalLayers =
            num_temporal_layers;
        config->codec_settings.VP9()->denoisingOn = denoising_on;
        config->codec_settings.VP9()->frameDroppingOn = frame_dropper_on;
        config->codec_settings.VP9()->keyFrameInterval = kBaseKeyFrameInterval;
        config->codec_settings.VP9()->automaticResizeOn = spatial_resize_on;
        break;
      case kVideoCodecH264:
        config->codec_settings.H264()->frameDroppingOn = frame_dropper_on;
        config->codec_settings.H264()->keyFrameInterval = kBaseKeyFrameInterval;
        break;
      default:
        RTC_NOTREACHED();
        break;
    }

    config->frame_length_in_bytes =
        CalcBufferSize(VideoType::kI420, width, height);
  }

  static void SetRateProfile(RateProfile* rate_profile,
                             int rate_update_index,
                             int bitrate_kbps,
                             int framerate_fps,
                             int frame_index_rate_update) {
    rate_profile->target_bit_rate[rate_update_index] = bitrate_kbps;
    rate_profile->input_frame_rate[rate_update_index] = framerate_fps;
    rate_profile->frame_index_rate_update[rate_update_index] =
        frame_index_rate_update;
  }

  static void AddRateControlThresholds(
      int max_num_dropped_frames,
      int max_key_frame_size_mismatch,
      int max_delta_frame_size_mismatch,
      int max_encoding_rate_mismatch,
      int max_time_hit_target,
      int num_spatial_resizes,
      int num_key_frames,
      std::vector<RateControlThresholds>* rc_thresholds) {
    RTC_DCHECK(rc_thresholds);

    rc_thresholds->emplace_back();
    RateControlThresholds* rc_threshold = &rc_thresholds->back();
    rc_threshold->max_num_dropped_frames = max_num_dropped_frames;
    rc_threshold->max_key_frame_size_mismatch = max_key_frame_size_mismatch;
    rc_threshold->max_delta_frame_size_mismatch = max_delta_frame_size_mismatch;
    rc_threshold->max_encoding_rate_mismatch = max_encoding_rate_mismatch;
    rc_threshold->max_time_hit_target = max_time_hit_target;
    rc_threshold->num_spatial_resizes = num_spatial_resizes;
    rc_threshold->num_key_frames = num_key_frames;
  }

  // Config.
  TestConfig config_;

  // Codecs.
  std::unique_ptr<cricket::WebRtcVideoEncoderFactory> encoder_factory_;
  VideoEncoder* encoder_;
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

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_
