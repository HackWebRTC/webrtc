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

#include <algorithm>
#include <utility>

#if defined(WEBRTC_ANDROID)
#include "modules/video_coding/codecs/test/android_test_initializer.h"
#include "sdk/android/src/jni/androidmediadecoder_jni.h"
#include "sdk/android/src/jni/androidmediaencoder_jni.h"
#elif defined(WEBRTC_IOS)
#include "modules/video_coding/codecs/test/objc_codec_h264_test.h"
#endif

#include "media/engine/internaldecoderfactory.h"
#include "media/engine/internalencoderfactory.h"
#include "media/engine/videodecodersoftwarefallbackwrapper.h"
#include "media/engine/videoencodersoftwarefallbackwrapper.h"
#include "modules/video_coding/codecs/vp8/include/vp8_common_types.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_coding.h"
#include "rtc_base/checks.h"
#include "rtc_base/cpu_time.h"
#include "rtc_base/event.h"
#include "rtc_base/file.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "system_wrappers/include/sleep.h"
#include "test/testsupport/fileutils.h"
#include "test/testsupport/metrics/video_metrics.h"
#include "test/video_codec_settings.h"

namespace webrtc {
namespace test {

namespace {

const int kMaxBitrateMismatchPercent = 20;
const int kBaseKeyFrameInterval = 3000;

// Parameters from VP8 wrapper, which control target size of key frames.
const float kInitialBufferSize = 0.5f;
const float kOptimalBufferSize = 0.6f;
const float kScaleKeyFrameSize = 0.5f;

void VerifyQuality(const QualityMetricsResult& psnr_result,
                   const QualityMetricsResult& ssim_result,
                   const QualityThresholds& quality_thresholds) {
  EXPECT_GT(psnr_result.average, quality_thresholds.min_avg_psnr);
  EXPECT_GT(psnr_result.min, quality_thresholds.min_min_psnr);
  EXPECT_GT(ssim_result.average, quality_thresholds.min_avg_ssim);
  EXPECT_GT(ssim_result.min, quality_thresholds.min_min_ssim);
}

void PrintQualityMetrics(const QualityMetricsResult& psnr_result,
                         const QualityMetricsResult& ssim_result) {
  printf("PSNR avg: %f, min: %f\n", psnr_result.average, psnr_result.min);
  printf("SSIM avg: %f, min: %f\n", ssim_result.average, ssim_result.min);
  printf("\n");
}

int NumberOfTemporalLayers(const VideoCodec& codec_settings) {
  if (codec_settings.codecType == kVideoCodecVP8) {
    return codec_settings.VP8().numberOfTemporalLayers;
  } else if (codec_settings.codecType == kVideoCodecVP9) {
    return codec_settings.VP9().numberOfTemporalLayers;
  } else {
    return 1;
  }
}

bool RunEncodeInRealTime(const TestConfig& config) {
  if (config.measure_cpu) {
    return true;
  }
#if defined(WEBRTC_ANDROID)
  // In order to not overwhelm the OpenMAX buffers in the Android MediaCodec.
  return (config.hw_encoder || config.hw_decoder);
#else
  return false;
#endif
}
}  // namespace

class VideoProcessorIntegrationTest::CpuProcessTime final {
 public:
  explicit CpuProcessTime(const TestConfig& config) : config_(config) {}
  ~CpuProcessTime() {}

  void Start() {
    if (config_.measure_cpu) {
      cpu_time_ -= rtc::GetProcessCpuTimeNanos();
      wallclock_time_ -= rtc::SystemTimeNanos();
    }
  }
  void Stop() {
    if (config_.measure_cpu) {
      cpu_time_ += rtc::GetProcessCpuTimeNanos();
      wallclock_time_ += rtc::SystemTimeNanos();
    }
  }
  void Print() const {
    if (config_.measure_cpu) {
      printf("CPU usage %%: %f\n", GetUsagePercent() / config_.NumberOfCores());
    }
  }

 private:
  double GetUsagePercent() const {
    return static_cast<double>(cpu_time_) / wallclock_time_ * 100.0;
  }

  const TestConfig config_;
  int64_t cpu_time_ = 0;
  int64_t wallclock_time_ = 0;
};

VideoProcessorIntegrationTest::VideoProcessorIntegrationTest() {
#if defined(WEBRTC_ANDROID)
  InitializeAndroidObjects();
#endif
}

VideoProcessorIntegrationTest::~VideoProcessorIntegrationTest() = default;

void VideoProcessorIntegrationTest::SetCodecSettings(TestConfig* config,
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

  // TODO(brandtr): Move the setting of |width| and |height| to the tests, and
  // DCHECK that they are set before initializing the codec instead.
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
}

// Processes all frames in the clip and verifies the result.
void VideoProcessorIntegrationTest::ProcessFramesAndMaybeVerify(
    const std::vector<RateProfile>& rate_profiles,
    const std::vector<RateControlThresholds>* rc_thresholds,
    const QualityThresholds* quality_thresholds,
    const BitstreamThresholds* bs_thresholds,
    const VisualizationParams* visualization_params) {
  RTC_DCHECK(!rate_profiles.empty());
  // The Android HW codec needs to be run on a task queue, so we simply always
  // run the test on a task queue.
  rtc::TaskQueue task_queue("VidProc TQ");
  rtc::Event sync_event(false, false);

  SetUpAndInitObjects(&task_queue, rate_profiles[0].target_kbps,
                      rate_profiles[0].input_fps, visualization_params);

  // Set initial rates.
  int rate_update_index = 0;
  task_queue.PostTask([this, &rate_profiles, rate_update_index] {
    processor_->SetRates(rate_profiles[rate_update_index].target_kbps,
                         rate_profiles[rate_update_index].input_fps);
  });

  cpu_process_time_->Start();

  // Process all frames.
  int frame_number = 0;
  const int num_frames = config_.num_frames;
  RTC_DCHECK_GE(num_frames, 1);
  while (frame_number < num_frames) {
    if (RunEncodeInRealTime(config_)) {
      // Roughly pace the frames.
      SleepMs(rtc::kNumMillisecsPerSec /
              rate_profiles[rate_update_index].input_fps);
    }

    task_queue.PostTask([this] { processor_->ProcessFrame(); });
    ++frame_number;

    if (frame_number ==
        rate_profiles[rate_update_index].frame_index_rate_update) {
      ++rate_update_index;
      RTC_DCHECK_GT(rate_profiles.size(), rate_update_index);

      task_queue.PostTask([this, &rate_profiles, rate_update_index] {
        processor_->SetRates(rate_profiles[rate_update_index].target_kbps,
                             rate_profiles[rate_update_index].input_fps);
      });
    }
  }

  // Give the VideoProcessor pipeline some time to process the last frame,
  // and then release the codecs.
  if (config_.hw_encoder || config_.hw_decoder) {
    SleepMs(1 * rtc::kNumMillisecsPerSec);
  }
  cpu_process_time_->Stop();
  ReleaseAndCloseObjects(&task_queue);

  // Calculate and print rate control statistics.
  std::vector<int> num_dropped_frames;
  std::vector<int> num_spatial_resizes;
  sync_event.Reset();
  task_queue.PostTask(
      [this, &num_dropped_frames, &num_spatial_resizes, &sync_event]() {
        num_dropped_frames = processor_->NumberDroppedFramesPerRateUpdate();
        num_spatial_resizes = processor_->NumberSpatialResizesPerRateUpdate();
        sync_event.Set();
      });
  sync_event.Wait(rtc::Event::kForever);

  rate_update_index = 0;
  frame_number = 0;
  ResetRateControlMetrics(rate_update_index, rate_profiles);
  while (frame_number < num_frames) {
    UpdateRateControlMetrics(frame_number);

    if (bs_thresholds) {
      VerifyBitstream(frame_number, *bs_thresholds);
    }

    ++frame_number;

    if (frame_number ==
        rate_profiles[rate_update_index].frame_index_rate_update) {
      PrintRateControlMetrics(rate_update_index, num_dropped_frames,
                              num_spatial_resizes);
      VerifyRateControlMetrics(rate_update_index, rc_thresholds,
                               num_dropped_frames, num_spatial_resizes);
      ++rate_update_index;
      ResetRateControlMetrics(rate_update_index, rate_profiles);
    }
  }

  PrintRateControlMetrics(rate_update_index, num_dropped_frames,
                          num_spatial_resizes);
  VerifyRateControlMetrics(rate_update_index, rc_thresholds, num_dropped_frames,
                           num_spatial_resizes);

  // Calculate and print other statistics.
  EXPECT_EQ(num_frames, static_cast<int>(stats_.size()));
  stats_.PrintSummary();
  cpu_process_time_->Print();

  // Calculate and print image quality statistics.
  // TODO(marpan): Should compute these quality metrics per SetRates update.
  QualityMetricsResult psnr_result, ssim_result;
  EXPECT_EQ(0, I420MetricsFromFiles(config_.input_filename.c_str(),
                                    config_.output_filename.c_str(),
                                    config_.codec_settings.width,
                                    config_.codec_settings.height, &psnr_result,
                                    &ssim_result));
  if (quality_thresholds) {
    VerifyQuality(psnr_result, ssim_result, *quality_thresholds);
  }
  PrintQualityMetrics(psnr_result, ssim_result);

  // Remove analysis file.
  if (remove(config_.output_filename.c_str()) < 0) {
    fprintf(stderr, "Failed to remove temporary file!\n");
  }
}

void VideoProcessorIntegrationTest::CreateEncoderAndDecoder() {
  std::unique_ptr<cricket::WebRtcVideoEncoderFactory> encoder_factory;
  if (config_.hw_encoder) {
#if defined(WEBRTC_ANDROID)
    encoder_factory.reset(new jni::MediaCodecVideoEncoderFactory());
#elif defined(WEBRTC_IOS)
    EXPECT_EQ(kVideoCodecH264, config_.codec_settings.codecType)
        << "iOS HW codecs only support H264.";
    encoder_factory = CreateObjCEncoderFactory();
#else
    RTC_NOTREACHED() << "Only support HW encoder on Android and iOS.";
#endif
  } else {
    encoder_factory.reset(new cricket::InternalEncoderFactory());
  }

  std::unique_ptr<cricket::WebRtcVideoDecoderFactory> decoder_factory;
  if (config_.hw_decoder) {
#if defined(WEBRTC_ANDROID)
    decoder_factory.reset(new jni::MediaCodecVideoDecoderFactory());
#elif defined(WEBRTC_IOS)
    EXPECT_EQ(kVideoCodecH264, config_.codec_settings.codecType)
        << "iOS HW codecs only support H264.";
    decoder_factory = CreateObjCDecoderFactory();
#else
    RTC_NOTREACHED() << "Only support HW decoder on Android and iOS.";
#endif
  } else {
    decoder_factory.reset(new cricket::InternalDecoderFactory());
  }

  cricket::VideoCodec codec;
  cricket::VideoDecoderParams decoder_params;  // Empty.
  switch (config_.codec_settings.codecType) {
    case kVideoCodecVP8:
      codec = cricket::VideoCodec(cricket::kVp8CodecName);
      encoder_.reset(encoder_factory->CreateVideoEncoder(codec));
      decoder_.reset(
          decoder_factory->CreateVideoDecoderWithParams(codec, decoder_params));
      break;
    case kVideoCodecVP9:
      codec = cricket::VideoCodec(cricket::kVp9CodecName);
      encoder_.reset(encoder_factory->CreateVideoEncoder(codec));
      decoder_.reset(
          decoder_factory->CreateVideoDecoderWithParams(codec, decoder_params));
      break;
    case kVideoCodecH264:
      // TODO(brandtr): Generalize so that we support multiple profiles here.
      codec = cricket::VideoCodec(cricket::kH264CodecName);
      if (config_.packetization_mode == H264PacketizationMode::NonInterleaved) {
        codec.SetParam(cricket::kH264FmtpPacketizationMode, "1");
      } else {
        RTC_CHECK_EQ(config_.packetization_mode,
                     H264PacketizationMode::SingleNalUnit);
        codec.SetParam(cricket::kH264FmtpPacketizationMode, "0");
      }
      encoder_.reset(encoder_factory->CreateVideoEncoder(codec));
      decoder_.reset(
          decoder_factory->CreateVideoDecoderWithParams(codec, decoder_params));
      break;
    default:
      RTC_NOTREACHED();
      break;
  }

  if (config_.sw_fallback_encoder) {
    encoder_ = rtc::MakeUnique<VideoEncoderSoftwareFallbackWrapper>(
        codec, std::move(encoder_));
  }
  if (config_.sw_fallback_decoder) {
    decoder_ = rtc::MakeUnique<VideoDecoderSoftwareFallbackWrapper>(
        config_.codec_settings.codecType, std::move(decoder_));
  }

  EXPECT_TRUE(encoder_) << "Encoder not successfully created.";
  EXPECT_TRUE(decoder_) << "Decoder not successfully created.";
}

void VideoProcessorIntegrationTest::DestroyEncoderAndDecoder() {
  encoder_.reset();
  decoder_.reset();
}

void VideoProcessorIntegrationTest::SetUpAndInitObjects(
    rtc::TaskQueue* task_queue,
    const int initial_bitrate_kbps,
    const int initial_framerate_fps,
    const VisualizationParams* visualization_params) {
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
    const std::string implementation_type = config_.hw_encoder ? "hw" : "sw";
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

  cpu_process_time_.reset(new CpuProcessTime(config_));
  packet_manipulator_.reset(new PacketManipulatorImpl(
      &packet_reader_, config_.networking_config, config_.verbose));

  config_.codec_settings.minBitrate = 0;
  config_.codec_settings.startBitrate = initial_bitrate_kbps;
  config_.codec_settings.maxFramerate = initial_framerate_fps;

  rtc::Event sync_event(false, false);
  task_queue->PostTask([this, &sync_event]() {
    processor_ = rtc::MakeUnique<VideoProcessor>(
        encoder_.get(), decoder_.get(), analysis_frame_reader_.get(),
        analysis_frame_writer_.get(), packet_manipulator_.get(), config_,
        &stats_, encoded_frame_writer_.get(), decoded_frame_writer_.get());
    processor_->Init();
    sync_event.Set();
  });
  sync_event.Wait(rtc::Event::kForever);
}

void VideoProcessorIntegrationTest::ReleaseAndCloseObjects(
    rtc::TaskQueue* task_queue) {
  rtc::Event sync_event(false, false);
  task_queue->PostTask([this, &sync_event]() {
    processor_->Release();
    sync_event.Set();
  });
  sync_event.Wait(rtc::Event::kForever);

  // The VideoProcessor must be ::Release()'d before we destroy the codecs.
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
}

// For every encoded frame, update the rate control metrics.
void VideoProcessorIntegrationTest::UpdateRateControlMetrics(int frame_number) {
  RTC_CHECK_GE(frame_number, 0);

  const int tl_idx = TemporalLayerIndexForFrame(frame_number);
  ++actual_.num_frames_layer[tl_idx];
  ++actual_.num_frames;

  const FrameStatistic* frame_stat = stats_.GetFrame(frame_number);
  FrameType frame_type = frame_stat->frame_type;
  float framesize_kbits = frame_stat->encoded_frame_size_bytes * 8.0f / 1000.0f;

  // Update rate mismatch relative to per-frame bandwidth.
  if (frame_type == kVideoFrameDelta) {
    // TODO(marpan): Should we count dropped (zero size) frames in mismatch?
    actual_.sum_delta_framesize_mismatch_layer[tl_idx] +=
        fabs(framesize_kbits - target_.framesize_kbits_layer[tl_idx]) /
        target_.framesize_kbits_layer[tl_idx];
  } else {
    float key_framesize_kbits = (frame_number == 0)
                                    ? target_.key_framesize_kbits_initial
                                    : target_.key_framesize_kbits;
    actual_.sum_key_framesize_mismatch +=
        fabs(framesize_kbits - key_framesize_kbits) / key_framesize_kbits;
    ++actual_.num_key_frames;
  }
  actual_.sum_framesize_kbits += framesize_kbits;
  actual_.sum_framesize_kbits_layer[tl_idx] += framesize_kbits;

  // Encoded bitrate: from the start of the update/run to current frame.
  actual_.kbps = actual_.sum_framesize_kbits * target_.fps / actual_.num_frames;
  actual_.kbps_layer[tl_idx] = actual_.sum_framesize_kbits_layer[tl_idx] *
                               target_.fps_layer[tl_idx] /
                               actual_.num_frames_layer[tl_idx];

  // Number of frames to hit target bitrate.
  if (actual_.BitrateMismatchPercent(target_.kbps) <
      kMaxBitrateMismatchPercent) {
    actual_.num_frames_to_hit_target =
        std::min(actual_.num_frames, actual_.num_frames_to_hit_target);
  }
}

// Verify expected behavior of rate control.
void VideoProcessorIntegrationTest::VerifyRateControlMetrics(
    int rate_update_index,
    const std::vector<RateControlThresholds>* rc_thresholds,
    const std::vector<int>& num_dropped_frames,
    const std::vector<int>& num_spatial_resizes) const {
  if (!rc_thresholds)
    return;

  const RateControlThresholds& rc_threshold =
      (*rc_thresholds)[rate_update_index];

  EXPECT_LE(num_dropped_frames[rate_update_index],
            rc_threshold.max_num_dropped_frames);
  EXPECT_EQ(rc_threshold.num_spatial_resizes,
            num_spatial_resizes[rate_update_index]);

  EXPECT_LE(actual_.num_frames_to_hit_target,
            rc_threshold.max_num_frames_to_hit_target);
  EXPECT_EQ(rc_threshold.num_key_frames, actual_.num_key_frames);
  EXPECT_LE(actual_.KeyFrameSizeMismatchPercent(),
            rc_threshold.max_key_framesize_mismatch_percent);
  EXPECT_LE(actual_.BitrateMismatchPercent(target_.kbps),
            rc_threshold.max_bitrate_mismatch_percent);

  const int num_temporal_layers =
      NumberOfTemporalLayers(config_.codec_settings);
  for (int i = 0; i < num_temporal_layers; ++i) {
    EXPECT_LE(actual_.DeltaFrameSizeMismatchPercent(i),
              rc_threshold.max_delta_framesize_mismatch_percent);
    EXPECT_LE(actual_.BitrateMismatchPercent(i, target_.kbps_layer[i]),
              rc_threshold.max_bitrate_mismatch_percent);
  }
}

void VideoProcessorIntegrationTest::PrintRateControlMetrics(
    int rate_update_index,
    const std::vector<int>& num_dropped_frames,
    const std::vector<int>& num_spatial_resizes) const {
  printf("Rate update #%d:\n", rate_update_index);
  printf(" Target bitrate         : %d\n", target_.kbps);
  printf(" Encoded bitrate        : %f\n", actual_.kbps);
  printf(" Frame rate             : %d\n", target_.fps);
  printf(" # processed frames     : %d\n", actual_.num_frames);
  printf(" # frames to convergence: %d\n", actual_.num_frames_to_hit_target);
  printf(" # dropped frames       : %d\n",
         num_dropped_frames[rate_update_index]);
  printf(" # spatial resizes      : %d\n",
         num_spatial_resizes[rate_update_index]);
  printf(" # key frames           : %d\n", actual_.num_key_frames);
  printf(" Key frame rate mismatch: %d\n",
         actual_.KeyFrameSizeMismatchPercent());

  const int num_temporal_layers =
      NumberOfTemporalLayers(config_.codec_settings);
  for (int i = 0; i < num_temporal_layers; ++i) {
    printf(" Temporal layer #%d:\n", i);
    printf("  Layer target bitrate        : %f\n", target_.kbps_layer[i]);
    printf("  Layer frame rate            : %f\n", target_.fps_layer[i]);
    printf("  Layer per frame bandwidth   : %f\n",
           target_.framesize_kbits_layer[i]);
    printf("  Layer encoded bitrate       : %f\n", actual_.kbps_layer[i]);
    printf("  Layer frame size %% mismatch : %d\n",
           actual_.DeltaFrameSizeMismatchPercent(i));
    printf("  Layer bitrate %% mismatch    : %d\n",
           actual_.BitrateMismatchPercent(i, target_.kbps_layer[i]));
    printf("  # processed frames per layer: %d\n", actual_.num_frames_layer[i]);
  }
  printf("\n");
}

void VideoProcessorIntegrationTest::VerifyBitstream(
    int frame_number,
    const BitstreamThresholds& bs_thresholds) {
  RTC_CHECK_GE(frame_number, 0);
  const FrameStatistic* frame_stat = stats_.GetFrame(frame_number);
  EXPECT_LE(*(frame_stat->max_nalu_length), bs_thresholds.max_nalu_length);
}

// Temporal layer index corresponding to frame number, for up to 3 layers.
int VideoProcessorIntegrationTest::TemporalLayerIndexForFrame(
    int frame_number) const {
  int tl_idx = -1;
  switch (NumberOfTemporalLayers(config_.codec_settings)) {
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

// Reset quantities before each encoder rate update.
void VideoProcessorIntegrationTest::ResetRateControlMetrics(
    int rate_update_index,
    const std::vector<RateProfile>& rate_profiles) {
  RTC_DCHECK_GT(rate_profiles.size(), rate_update_index);
  // Set new rates.
  target_.kbps = rate_profiles[rate_update_index].target_kbps;
  target_.fps = rate_profiles[rate_update_index].input_fps;
  SetRatesPerTemporalLayer();

  // Set key frame target sizes.
  if (rate_update_index == 0) {
    target_.key_framesize_kbits_initial =
        0.5 * kInitialBufferSize * target_.kbps_layer[0];
  }

  // Set maximum size of key frames, following setting in the VP8 wrapper.
  float max_key_size = kScaleKeyFrameSize * kOptimalBufferSize * target_.fps;
  // We don't know exact target size of the key frames (except for first one),
  // but the minimum in libvpx is ~|3 * per_frame_bandwidth| and maximum is
  // set by |max_key_size_ * per_frame_bandwidth|. Take middle point/average
  // as reference for mismatch. Note key frames always correspond to base
  // layer frame in this test.
  target_.key_framesize_kbits =
      0.5 * (3 + max_key_size) * target_.framesize_kbits_layer[0];

  // Reset rate control metrics.
  actual_ = TestResults();
  actual_.num_frames_to_hit_target =  // Set to max number of frames.
      rate_profiles[rate_update_index].frame_index_rate_update;
}

void VideoProcessorIntegrationTest::SetRatesPerTemporalLayer() {
  const int num_temporal_layers =
      NumberOfTemporalLayers(config_.codec_settings);
  RTC_DCHECK_LE(num_temporal_layers, kMaxNumTemporalLayers);

  for (int i = 0; i < num_temporal_layers; ++i) {
    float bitrate_ratio;
    if (i > 0) {
      bitrate_ratio = kVp8LayerRateAlloction[num_temporal_layers - 1][i] -
                      kVp8LayerRateAlloction[num_temporal_layers - 1][i - 1];
    } else {
      bitrate_ratio = kVp8LayerRateAlloction[num_temporal_layers - 1][i];
    }
    target_.kbps_layer[i] = target_.kbps * bitrate_ratio;
    target_.fps_layer[i] =
        target_.fps / static_cast<float>(1 << (num_temporal_layers - 1));
  }
  if (num_temporal_layers == 3) {
    target_.fps_layer[2] = target_.fps / 2.0f;
  }

  // Update layer per-frame-bandwidth.
  for (int i = 0; i < num_temporal_layers; ++i) {
    target_.framesize_kbits_layer[i] =
        target_.kbps_layer[i] / target_.fps_layer[i];
  }
}

}  // namespace test
}  // namespace webrtc
