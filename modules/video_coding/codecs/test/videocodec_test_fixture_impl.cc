/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videocodec_test_fixture_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#if defined(WEBRTC_ANDROID)
#include "modules/video_coding/codecs/test/android_codec_factory_helper.h"
#endif

#include "api/video_codecs/sdp_video_format.h"
#include "common_types.h"  // NOLINT(build/include)
#include "media/engine/internaldecoderfactory.h"
#include "media/engine/internalencoderfactory.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "media/engine/videodecodersoftwarefallbackwrapper.h"
#include "media/engine/videoencodersoftwarefallbackwrapper.h"
#include "modules/video_coding/codecs/vp8/include/vp8_common_types.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_coding.h"
#include "rtc_base/checks.h"
#include "rtc_base/cpu_time.h"
#include "rtc_base/event.h"
#include "rtc_base/file.h"
#include "rtc_base/ptr_util.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {

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

// TODO(kthelgason): Move this out of the test fixture impl and
// make available as a shared utility class.
void VideoCodecTestFixtureImpl::H264KeyframeChecker::
    CheckEncodedFrame(webrtc::VideoCodecType codec,
                      const EncodedImage& encoded_frame) const {
  EXPECT_EQ(kVideoCodecH264, codec);
  bool contains_sps = false;
  bool contains_pps = false;
  bool contains_idr = false;
  const std::vector<webrtc::H264::NaluIndex> nalu_indices =
      webrtc::H264::FindNaluIndices(encoded_frame._buffer,
                                    encoded_frame._length);
  for (const webrtc::H264::NaluIndex& index : nalu_indices) {
    webrtc::H264::NaluType nalu_type = webrtc::H264::ParseNaluType(
        encoded_frame._buffer[index.payload_start_offset]);
    if (nalu_type == webrtc::H264::NaluType::kSps) {
      contains_sps = true;
    } else if (nalu_type == webrtc::H264::NaluType::kPps) {
      contains_pps = true;
    } else if (nalu_type == webrtc::H264::NaluType::kIdr) {
      contains_idr = true;
    }
  }
  if (encoded_frame._frameType == kVideoFrameKey) {
    EXPECT_TRUE(contains_sps) << "Keyframe should contain SPS.";
    EXPECT_TRUE(contains_pps) << "Keyframe should contain PPS.";
    EXPECT_TRUE(contains_idr) << "Keyframe should contain IDR.";
  } else if (encoded_frame._frameType == kVideoFrameDelta) {
    EXPECT_FALSE(contains_sps) << "Delta frame should not contain SPS.";
    EXPECT_FALSE(contains_pps) << "Delta frame should not contain PPS.";
    EXPECT_FALSE(contains_idr) << "Delta frame should not contain IDR.";
  } else {
    RTC_NOTREACHED();
  }
}

class VideoCodecTestFixtureImpl::CpuProcessTime final {
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
      printf("cpu_usage_percent: %f\n",
             GetUsagePercent() / config_.NumberOfCores());
      printf("\n");
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

VideoCodecTestFixtureImpl::
    VideoCodecTestFixtureImpl(TestConfig config)
    : config_(config) {
#if defined(WEBRTC_ANDROID)
  InitializeAndroidObjects();
#endif
}

VideoCodecTestFixtureImpl::
    VideoCodecTestFixtureImpl(
        TestConfig config,
        std::unique_ptr<VideoDecoderFactory> decoder_factory,
        std::unique_ptr<VideoEncoderFactory> encoder_factory)
    : decoder_factory_(std::move(decoder_factory)),
      encoder_factory_(std::move(encoder_factory)),
      config_(config) {
#if defined(WEBRTC_ANDROID)
  InitializeAndroidObjects();
#endif
}

VideoCodecTestFixtureImpl::
    ~VideoCodecTestFixtureImpl() = default;

// Processes all frames in the clip and verifies the result.
void VideoCodecTestFixtureImpl::RunTest(
    const std::vector<RateProfile>& rate_profiles,
    const std::vector<RateControlThresholds>* rc_thresholds,
    const std::vector<QualityThresholds>* quality_thresholds,
    const BitstreamThresholds* bs_thresholds,
    const VisualizationParams* visualization_params) {
  RTC_DCHECK(!rate_profiles.empty());

  // The Android HW codec needs to be run on a task queue, so we simply always
  // run the test on a task queue.
  rtc::TaskQueue task_queue("VidProc TQ");

  SetUpAndInitObjects(
      &task_queue, static_cast<const int>(rate_profiles[0].target_kbps),
      static_cast<const int>(rate_profiles[0].input_fps), visualization_params);
  PrintSettings(&task_queue);
  ProcessAllFrames(&task_queue, rate_profiles);
  ReleaseAndCloseObjects(&task_queue);

  AnalyzeAllFrames(rate_profiles, rc_thresholds, quality_thresholds,
                   bs_thresholds);
}

void VideoCodecTestFixtureImpl::ProcessAllFrames(
    rtc::TaskQueue* task_queue,
    const std::vector<RateProfile>& rate_profiles) {
  // Process all frames.
  size_t rate_update_index = 0;

  // Set initial rates.
  task_queue->PostTask([this, &rate_profiles, rate_update_index] {
    processor_->SetRates(rate_profiles[rate_update_index].target_kbps,
                         rate_profiles[rate_update_index].input_fps);
  });

  cpu_process_time_->Start();

  for (size_t frame_number = 0; frame_number < config_.num_frames;
       ++frame_number) {
    if (frame_number ==
        rate_profiles[rate_update_index].frame_index_rate_update) {
      ++rate_update_index;
      RTC_DCHECK_GT(rate_profiles.size(), rate_update_index);

      task_queue->PostTask([this, &rate_profiles, rate_update_index] {
        processor_->SetRates(rate_profiles[rate_update_index].target_kbps,
                             rate_profiles[rate_update_index].input_fps);
      });
    }

    task_queue->PostTask([this] { processor_->ProcessFrame(); });

    if (RunEncodeInRealTime(config_)) {
      // Roughly pace the frames.
      const size_t frame_duration_ms =
          rtc::kNumMillisecsPerSec / rate_profiles[rate_update_index].input_fps;
      SleepMs(static_cast<int>(frame_duration_ms));
    }
  }

  rtc::Event sync_event(false, false);
  task_queue->PostTask([&sync_event] { sync_event.Set(); });
  sync_event.Wait(rtc::Event::kForever);

  // Give the VideoProcessor pipeline some time to process the last frame,
  // and then release the codecs.
  if (config_.IsAsyncCodec()) {
    SleepMs(1 * rtc::kNumMillisecsPerSec);
  }

  cpu_process_time_->Stop();
}

void VideoCodecTestFixtureImpl::AnalyzeAllFrames(
    const std::vector<RateProfile>& rate_profiles,
    const std::vector<RateControlThresholds>* rc_thresholds,
    const std::vector<QualityThresholds>* quality_thresholds,
    const BitstreamThresholds* bs_thresholds) {
  for (size_t rate_update_idx = 0; rate_update_idx < rate_profiles.size();
       ++rate_update_idx) {
    const size_t first_frame_num =
        (rate_update_idx == 0)
            ? 0
            : rate_profiles[rate_update_idx - 1].frame_index_rate_update;
    const size_t last_frame_num =
        rate_profiles[rate_update_idx].frame_index_rate_update - 1;
    RTC_CHECK(last_frame_num >= first_frame_num);

    std::vector<VideoStatistics> layer_stats =
        stats_.SliceAndCalcLayerVideoStatistic(first_frame_num, last_frame_num);
    printf("==> Receive stats\n");
    for (const auto& layer_stat : layer_stats) {
      printf("%s\n\n", layer_stat.ToString("recv_").c_str());
    }

    VideoStatistics send_stat = stats_.SliceAndCalcAggregatedVideoStatistic(
        first_frame_num, last_frame_num);
    printf("==> Send stats\n");
    printf("%s\n", send_stat.ToString("send_").c_str());

    const RateControlThresholds* rc_threshold =
        rc_thresholds ? &(*rc_thresholds)[rate_update_idx] : nullptr;
    const QualityThresholds* quality_threshold =
        quality_thresholds ? &(*quality_thresholds)[rate_update_idx] : nullptr;

    VerifyVideoStatistic(send_stat, rc_threshold, quality_threshold,
                         bs_thresholds,
                         rate_profiles[rate_update_idx].target_kbps,
                         rate_profiles[rate_update_idx].input_fps);
  }

  if (config_.print_frame_level_stats) {
    stats_.PrintFrameStatistics();
  }

  cpu_process_time_->Print();
  printf("\n");
}

void VideoCodecTestFixtureImpl::VerifyVideoStatistic(
    const VideoStatistics& video_stat,
    const RateControlThresholds* rc_thresholds,
    const QualityThresholds* quality_thresholds,
    const BitstreamThresholds* bs_thresholds,
    size_t target_bitrate_kbps,
    float input_framerate_fps) {
  if (rc_thresholds) {
    const float bitrate_mismatch_percent =
        100 * std::fabs(1.0f * video_stat.bitrate_kbps - target_bitrate_kbps) /
        target_bitrate_kbps;
    const float framerate_mismatch_percent =
        100 * std::fabs(video_stat.framerate_fps - input_framerate_fps) /
        input_framerate_fps;
    EXPECT_LE(bitrate_mismatch_percent,
              rc_thresholds->max_avg_bitrate_mismatch_percent);
    EXPECT_LE(video_stat.time_to_reach_target_bitrate_sec,
              rc_thresholds->max_time_to_reach_target_bitrate_sec);
    EXPECT_LE(framerate_mismatch_percent,
              rc_thresholds->max_avg_framerate_mismatch_percent);
    EXPECT_LE(video_stat.avg_delay_sec,
              rc_thresholds->max_avg_buffer_level_sec);
    EXPECT_LE(video_stat.max_key_frame_delay_sec,
              rc_thresholds->max_max_key_frame_delay_sec);
    EXPECT_LE(video_stat.max_delta_frame_delay_sec,
              rc_thresholds->max_max_delta_frame_delay_sec);
    EXPECT_LE(video_stat.num_spatial_resizes,
              rc_thresholds->max_num_spatial_resizes);
    EXPECT_LE(video_stat.num_key_frames, rc_thresholds->max_num_key_frames);
  }

  if (quality_thresholds) {
    EXPECT_GT(video_stat.avg_psnr, quality_thresholds->min_avg_psnr);
    EXPECT_GT(video_stat.min_psnr, quality_thresholds->min_min_psnr);
    EXPECT_GT(video_stat.avg_ssim, quality_thresholds->min_avg_ssim);
    EXPECT_GT(video_stat.min_ssim, quality_thresholds->min_min_ssim);
  }

  if (bs_thresholds) {
    EXPECT_LE(video_stat.max_nalu_size_bytes,
              bs_thresholds->max_max_nalu_size_bytes);
  }
}

std::unique_ptr<VideoDecoderFactory>
VideoCodecTestFixtureImpl::CreateDecoderFactory() {
  if (config_.hw_decoder) {
#if defined(WEBRTC_ANDROID)
    return CreateAndroidDecoderFactory();
#else
    RTC_NOTREACHED() << "Only support HW decoder on Android.";
    return nullptr;
#endif
  } else {
    return rtc::MakeUnique<InternalDecoderFactory>();
  }
}

std::unique_ptr<VideoEncoderFactory>
VideoCodecTestFixtureImpl::CreateEncoderFactory() {
  if (config_.hw_encoder) {
#if defined(WEBRTC_ANDROID)
    return CreateAndroidEncoderFactory();
#else
    RTC_NOTREACHED() << "Only support HW encoder on Android.";
    return nullptr;
#endif
  } else {
    return rtc::MakeUnique<InternalEncoderFactory>();
  }
}

void VideoCodecTestFixtureImpl::CreateEncoderAndDecoder() {
  const SdpVideoFormat format = config_.ToSdpVideoFormat();
  if (!decoder_factory_)
    decoder_factory_ = CreateDecoderFactory();
  if (!encoder_factory_)
    encoder_factory_ = CreateEncoderFactory();
  if (config_.simulcast_adapted_encoder) {
    EXPECT_EQ("VP8", format.name);
    encoder_.reset(new SimulcastEncoderAdapter(encoder_factory_.get()));
  } else {
    encoder_ = encoder_factory_->CreateVideoEncoder(format);
  }

  const size_t num_simulcast_or_spatial_layers = std::max(
      config_.NumberOfSimulcastStreams(), config_.NumberOfSpatialLayers());

  for (size_t i = 0; i < num_simulcast_or_spatial_layers; ++i) {
    decoders_.push_back(std::unique_ptr<VideoDecoder>(
        decoder_factory_->CreateVideoDecoder(format)));
  }

  if (config_.sw_fallback_encoder) {
    EXPECT_FALSE(config_.simulcast_adapted_encoder)
        << "SimulcastEncoderAdapter and VideoEncoderSoftwareFallbackWrapper "
           "are not jointly supported.";
    encoder_ = rtc::MakeUnique<VideoEncoderSoftwareFallbackWrapper>(
        InternalEncoderFactory().CreateVideoEncoder(format),
        std::move(encoder_));
  }
  if (config_.sw_fallback_decoder) {
    for (auto& decoder : decoders_) {
      decoder = rtc::MakeUnique<VideoDecoderSoftwareFallbackWrapper>(
          InternalDecoderFactory().CreateVideoDecoder(format),
          std::move(decoder));
    }
  }

  EXPECT_TRUE(encoder_) << "Encoder not successfully created.";

  for (const auto& decoder : decoders_) {
    EXPECT_TRUE(decoder) << "Decoder not successfully created.";
  }
}

void VideoCodecTestFixtureImpl::DestroyEncoderAndDecoder() {
  decoders_.clear();
  encoder_.reset();
}

Stats VideoCodecTestFixtureImpl::GetStats() {
  return stats_;
}

void VideoCodecTestFixtureImpl::SetUpAndInitObjects(
    rtc::TaskQueue* task_queue,
    int initial_bitrate_kbps,
    int initial_framerate_fps,
    const VisualizationParams* visualization_params) {
  CreateEncoderAndDecoder();

  config_.codec_settings.minBitrate = 0;
  config_.codec_settings.startBitrate = initial_bitrate_kbps;
  config_.codec_settings.maxFramerate = initial_framerate_fps;

  // Create file objects for quality analysis.
  source_frame_reader_.reset(
      new YuvFrameReaderImpl(config_.filepath, config_.codec_settings.width,
                             config_.codec_settings.height));
  EXPECT_TRUE(source_frame_reader_->Init());

  const size_t num_simulcast_or_spatial_layers = std::max(
      config_.NumberOfSimulcastStreams(), config_.NumberOfSpatialLayers());

  if (visualization_params) {
    RTC_DCHECK(encoded_frame_writers_.empty());
    RTC_DCHECK(decoded_frame_writers_.empty());
    for (size_t simulcast_svc_idx = 0;
         simulcast_svc_idx < num_simulcast_or_spatial_layers;
         ++simulcast_svc_idx) {
      const std::string output_filename_base =
          OutputPath() + config_.FilenameWithParams() + "_" +
          std::to_string(simulcast_svc_idx);

      if (visualization_params->save_encoded_ivf) {
        rtc::File post_encode_file =
            rtc::File::Create(output_filename_base + ".ivf");
        encoded_frame_writers_.push_back(
            IvfFileWriter::Wrap(std::move(post_encode_file), 0));
      }

      if (visualization_params->save_decoded_y4m) {
        FrameWriter* decoded_frame_writer = new Y4mFrameWriterImpl(
            output_filename_base + ".y4m", config_.codec_settings.width,
            config_.codec_settings.height, initial_framerate_fps);
        EXPECT_TRUE(decoded_frame_writer->Init());
        decoded_frame_writers_.push_back(
            std::unique_ptr<FrameWriter>(decoded_frame_writer));
      }
    }
  }

  stats_.Clear();

  cpu_process_time_.reset(new CpuProcessTime(config_));

  rtc::Event sync_event(false, false);
  task_queue->PostTask([this, &sync_event]() {
    processor_ = rtc::MakeUnique<VideoProcessor>(
        encoder_.get(), &decoders_, source_frame_reader_.get(), config_,
        &stats_,
        encoded_frame_writers_.empty() ? nullptr : &encoded_frame_writers_,
        decoded_frame_writers_.empty() ? nullptr : &decoded_frame_writers_);
    sync_event.Set();
  });
  sync_event.Wait(rtc::Event::kForever);
}

void VideoCodecTestFixtureImpl::ReleaseAndCloseObjects(
    rtc::TaskQueue* task_queue) {
  rtc::Event sync_event(false, false);
  task_queue->PostTask([this, &sync_event]() {
    processor_.reset();
    sync_event.Set();
  });
  sync_event.Wait(rtc::Event::kForever);

  // The VideoProcessor must be destroyed before the codecs.
  DestroyEncoderAndDecoder();

  source_frame_reader_->Close();

  // Close visualization files.
  for (auto& encoded_frame_writer : encoded_frame_writers_) {
    EXPECT_TRUE(encoded_frame_writer->Close());
  }
  encoded_frame_writers_.clear();
  for (auto& decoded_frame_writer : decoded_frame_writers_) {
    decoded_frame_writer->Close();
  }
  decoded_frame_writers_.clear();
}

void VideoCodecTestFixtureImpl::PrintSettings(
    rtc::TaskQueue* task_queue) const {
  printf("==> TestConfig\n");
  printf("%s\n", config_.ToString().c_str());

  printf("==> Codec names\n");
  std::string encoder_name;
  std::string decoder_name;
  rtc::Event sync_event(false, false);
  task_queue->PostTask([this, &encoder_name, &decoder_name, &sync_event] {
    encoder_name = encoder_->ImplementationName();
    decoder_name = decoders_.at(0)->ImplementationName();
    sync_event.Set();
  });
  sync_event.Wait(rtc::Event::kForever);
  printf("enc_impl_name: %s\n", encoder_name.c_str());
  printf("dec_impl_name: %s\n", decoder_name.c_str());
  if (encoder_name == decoder_name) {
    printf("codec_impl_name: %s_%s\n", config_.CodecName().c_str(),
           encoder_name.c_str());
  }
  printf("\n");
}

}  // namespace test
}  // namespace webrtc
