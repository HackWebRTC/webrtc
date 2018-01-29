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
#include "sdk/android/native_api/codecs/wrapper.h"
#include "sdk/android/native_api/jni/class_loader.h"
#include "sdk/android/native_api/jni/jvm.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"
#elif defined(WEBRTC_IOS)
#include "modules/video_coding/codecs/test/objc_codec_h264_test.h"
#endif

#include "common_types.h"  // NOLINT(build/include)
#include "media/base/h264_profile_level_id.h"
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
#include "rtc_base/ptr_util.h"
#include "system_wrappers/include/sleep.h"
#include "test/statistics.h"
#include "test/testsupport/fileutils.h"
#include "test/testsupport/metrics/video_metrics.h"

namespace webrtc {
namespace test {

namespace {

const int kRtpClockRateHz = 90000;

const int kMaxBitrateMismatchPercent = 20;

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

SdpVideoFormat CreateSdpVideoFormat(const TestConfig& config) {
  switch (config.codec_settings.codecType) {
    case kVideoCodecVP8:
      return SdpVideoFormat(cricket::kVp8CodecName);

    case kVideoCodecVP9:
      return SdpVideoFormat(cricket::kVp9CodecName);

    case kVideoCodecH264: {
      const char* packetization_mode =
          config.h264_codec_settings.packetization_mode ==
                  H264PacketizationMode::NonInterleaved
              ? "1"
              : "0";
      return SdpVideoFormat(
          cricket::kH264CodecName,
          {{cricket::kH264FmtpProfileLevelId,
            *H264::ProfileLevelIdToString(H264::ProfileLevelId(
                config.h264_codec_settings.profile, H264::kLevel3_1))},
           {cricket::kH264FmtpPacketizationMode, packetization_mode}});
    }
    default:
      RTC_NOTREACHED();
      return SdpVideoFormat("");
  }
}

}  // namespace

void VideoProcessorIntegrationTest::H264KeyframeChecker::CheckEncodedFrame(
    webrtc::VideoCodecType codec,
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

VideoProcessorIntegrationTest::VideoProcessorIntegrationTest() {
#if defined(WEBRTC_ANDROID)
  InitializeAndroidObjects();
#endif
}

VideoProcessorIntegrationTest::~VideoProcessorIntegrationTest() = default;

// Processes all frames in the clip and verifies the result.
void VideoProcessorIntegrationTest::ProcessFramesAndMaybeVerify(
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
  PrintSettings();

  ProcessAllFrames(&task_queue, rate_profiles);

  ReleaseAndCloseObjects(&task_queue);

  AnalyzeAllFrames(rate_profiles, rc_thresholds, quality_thresholds,
                   bs_thresholds);
}

void VideoProcessorIntegrationTest::ProcessAllFrames(
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
      size_t frame_duration_ms =
          rtc::kNumMillisecsPerSec / rate_profiles[rate_update_index].input_fps;
      SleepMs(static_cast<int>(frame_duration_ms));
    }
  }

  rtc::Event sync_event(false, false);
  task_queue->PostTask([&sync_event] { sync_event.Set(); });
  sync_event.Wait(rtc::Event::kForever);

  // Give the VideoProcessor pipeline some time to process the last frame,
  // and then release the codecs.
  if (config_.hw_encoder || config_.hw_decoder) {
    SleepMs(1 * rtc::kNumMillisecsPerSec);
  }

  cpu_process_time_->Stop();
}

void VideoProcessorIntegrationTest::AnalyzeAllFrames(
    const std::vector<RateProfile>& rate_profiles,
    const std::vector<RateControlThresholds>* rc_thresholds,
    const std::vector<QualityThresholds>* quality_thresholds,
    const BitstreamThresholds* bs_thresholds) {
  const bool is_svc = config_.NumberOfSpatialLayers() > 1;
  const size_t number_of_simulcast_or_spatial_layers =
      std::max(std::size_t{1},
               std::max(config_.NumberOfSpatialLayers(),
                        static_cast<size_t>(
                            config_.codec_settings.numberOfSimulcastStreams)));
  const size_t number_of_temporal_layers = config_.NumberOfTemporalLayers();
  printf("Rate control statistics\n==\n");
  for (size_t rate_update_index = 0; rate_update_index < rate_profiles.size();
       ++rate_update_index) {
    const size_t first_frame_number =
        (rate_update_index == 0)
            ? 0
            : rate_profiles[rate_update_index - 1].frame_index_rate_update;
    const size_t last_frame_number =
        rate_profiles[rate_update_index].frame_index_rate_update - 1;
    RTC_CHECK(last_frame_number >= first_frame_number);
    const size_t number_of_frames = last_frame_number - first_frame_number + 1;
    const float input_duration_sec =
        1.0 * number_of_frames / rate_profiles[rate_update_index].input_fps;

    std::vector<FrameStatistic> overall_stats =
        ExtractLayerStats(number_of_simulcast_or_spatial_layers - 1,
                          number_of_temporal_layers - 1, first_frame_number,
                          last_frame_number, true);

    printf("Rate update #%zu:\n", rate_update_index);

    const RateControlThresholds* rc_threshold =
        rc_thresholds ? &(*rc_thresholds)[rate_update_index] : nullptr;
    const QualityThresholds* quality_threshold =
        quality_thresholds ? &(*quality_thresholds)[rate_update_index]
                           : nullptr;
    AnalyzeAndPrintStats(
        overall_stats, rate_profiles[rate_update_index].target_kbps,
        rate_profiles[rate_update_index].input_fps, input_duration_sec,
        rc_threshold, quality_threshold, bs_thresholds);

    if (config_.print_frame_level_stats) {
      PrintFrameLevelStats(overall_stats);
    }

    for (size_t spatial_layer_number = 0;
         spatial_layer_number < number_of_simulcast_or_spatial_layers;
         ++spatial_layer_number) {
      for (size_t temporal_layer_number = 0;
           temporal_layer_number < number_of_temporal_layers;
           ++temporal_layer_number) {
        std::vector<FrameStatistic> layer_stats =
            ExtractLayerStats(spatial_layer_number, temporal_layer_number,
                              first_frame_number, last_frame_number, is_svc);

        const size_t target_bitrate_kbps = layer_stats[0].target_bitrate_kbps;
        const float target_framerate_fps =
            1.0 * rate_profiles[rate_update_index].input_fps /
            (1 << (number_of_temporal_layers - temporal_layer_number - 1));

        printf("Spatial %zu temporal %zu:\n", spatial_layer_number,
               temporal_layer_number);
        AnalyzeAndPrintStats(layer_stats, target_bitrate_kbps,
                             target_framerate_fps, input_duration_sec, nullptr,
                             nullptr, nullptr);

        if (config_.print_frame_level_stats) {
          PrintFrameLevelStats(layer_stats);
        }
      }
    }
  }

  cpu_process_time_->Print();
}

std::vector<FrameStatistic> VideoProcessorIntegrationTest::ExtractLayerStats(
    size_t target_spatial_layer_number,
    size_t target_temporal_layer_number,
    size_t first_frame_number,
    size_t last_frame_number,
    bool combine_layers_stats) {
  size_t target_bitrate_kbps = 0;
  std::vector<FrameStatistic> layer_stats;

  for (size_t frame_number = first_frame_number;
       frame_number <= last_frame_number; ++frame_number) {
    FrameStatistic superframe_stat =
        *stats_.at(target_spatial_layer_number).GetFrame(frame_number);
    const size_t tl_idx = superframe_stat.temporal_layer_idx;
    if (tl_idx <= target_temporal_layer_number) {
      if (combine_layers_stats) {
        for (size_t spatial_layer_number = 0;
             spatial_layer_number < target_spatial_layer_number;
             ++spatial_layer_number) {
          const FrameStatistic* frame_stat =
              stats_.at(spatial_layer_number).GetFrame(frame_number);
          superframe_stat.encoded_frame_size_bytes +=
              frame_stat->encoded_frame_size_bytes;
          superframe_stat.encode_time_us = std::max(
              superframe_stat.encode_time_us, frame_stat->encode_time_us);
          superframe_stat.decode_time_us = std::max(
              superframe_stat.decode_time_us, frame_stat->decode_time_us);
        }
      }

      target_bitrate_kbps =
          std::max(target_bitrate_kbps, superframe_stat.target_bitrate_kbps);

      if (superframe_stat.encoding_successful) {
        RTC_CHECK(superframe_stat.target_bitrate_kbps <= target_bitrate_kbps ||
                  tl_idx == target_temporal_layer_number);
        RTC_CHECK(superframe_stat.target_bitrate_kbps == target_bitrate_kbps ||
                  tl_idx < target_temporal_layer_number);
      }

      layer_stats.push_back(superframe_stat);
    }
  }

  for (auto& frame_stat : layer_stats) {
    frame_stat.target_bitrate_kbps = target_bitrate_kbps;
  }

  return layer_stats;
}

void VideoProcessorIntegrationTest::CreateEncoderAndDecoder() {
  std::unique_ptr<VideoEncoderFactory> encoder_factory;
  if (config_.hw_encoder) {
#if defined(WEBRTC_ANDROID)
    JNIEnv* env = AttachCurrentThreadIfNeeded();
    ScopedJavaLocalRef<jclass> factory_class =
        GetClass(env, "org/webrtc/HardwareVideoEncoderFactory");
    jmethodID factory_constructor = env->GetMethodID(
        factory_class.obj(), "<init>", "(Lorg/webrtc/EglBase$Context;ZZ)V");
    ScopedJavaLocalRef<jobject> factory_object(
        env, env->NewObject(factory_class.obj(), factory_constructor,
                            nullptr /* shared_context */,
                            false /* enable_intel_vp8_encoder */,
                            true /* enable_h264_high_profile */));
    encoder_factory =
        JavaToNativeVideoEncoderFactory(env, factory_object.obj());
#elif defined(WEBRTC_IOS)
    EXPECT_EQ(kVideoCodecH264, config_.codec_settings.codecType)
        << "iOS HW codecs only support H264.";
    encoder_factory = CreateObjCEncoderFactory();
#else
    RTC_NOTREACHED() << "Only support HW encoder on Android and iOS.";
#endif
  } else {
    encoder_factory = rtc::MakeUnique<InternalEncoderFactory>();
  }

  std::unique_ptr<VideoDecoderFactory> decoder_factory;
  if (config_.hw_decoder) {
#if defined(WEBRTC_ANDROID)
    JNIEnv* env = AttachCurrentThreadIfNeeded();
    ScopedJavaLocalRef<jclass> factory_class =
        GetClass(env, "org/webrtc/HardwareVideoDecoderFactory");
    jmethodID factory_constructor = env->GetMethodID(
        factory_class.obj(), "<init>", "(Lorg/webrtc/EglBase$Context;)V");
    ScopedJavaLocalRef<jobject> factory_object(
        env, env->NewObject(factory_class.obj(), factory_constructor,
                            nullptr /* shared_context */));
    decoder_factory =
        JavaToNativeVideoDecoderFactory(env, factory_object.obj());
#elif defined(WEBRTC_IOS)
    EXPECT_EQ(kVideoCodecH264, config_.codec_settings.codecType)
        << "iOS HW codecs only support H264.";
    decoder_factory = CreateObjCDecoderFactory();
#else
    RTC_NOTREACHED() << "Only support HW decoder on Android and iOS.";
#endif
  } else {
    decoder_factory = rtc::MakeUnique<InternalDecoderFactory>();
  }

  const SdpVideoFormat format = CreateSdpVideoFormat(config_);
  encoder_ = encoder_factory->CreateVideoEncoder(format);
  decoders_.push_back(std::unique_ptr<VideoDecoder>(
      decoder_factory->CreateVideoDecoder(format)));

  if (config_.sw_fallback_encoder) {
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

void VideoProcessorIntegrationTest::DestroyEncoderAndDecoder() {
  encoder_.reset();
  decoders_.clear();
}

void VideoProcessorIntegrationTest::SetUpAndInitObjects(
    rtc::TaskQueue* task_queue,
    const int initial_bitrate_kbps,
    const int initial_framerate_fps,
    const VisualizationParams* visualization_params) {
  CreateEncoderAndDecoder();

  config_.codec_settings.minBitrate = 0;
  config_.codec_settings.startBitrate = initial_bitrate_kbps;
  config_.codec_settings.maxFramerate = initial_framerate_fps;

  // Create file objects for quality analysis.
  source_frame_reader_.reset(new YuvFrameReaderImpl(
      config_.input_filename, config_.codec_settings.width,
      config_.codec_settings.height));
  EXPECT_TRUE(source_frame_reader_->Init());

  const size_t num_simulcast_or_spatial_layers = std::max(
      config_.NumberOfSimulcastStreams(), config_.NumberOfSpatialLayers());

  if (visualization_params) {
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

  stats_.resize(num_simulcast_or_spatial_layers);

  cpu_process_time_.reset(new CpuProcessTime(config_));

  rtc::Event sync_event(false, false);
  task_queue->PostTask([this, &sync_event]() {
    processor_ = rtc::MakeUnique<VideoProcessor>(
        encoder_.get(), decoders_.at(0).get(), source_frame_reader_.get(),
        config_, &stats_.at(0),
        encoded_frame_writers_.empty() ? nullptr
                                       : encoded_frame_writers_.at(0).get(),
        decoded_frame_writers_.empty() ? nullptr
                                       : decoded_frame_writers_.at(0).get());
    sync_event.Set();
  });
  sync_event.Wait(rtc::Event::kForever);
}

void VideoProcessorIntegrationTest::ReleaseAndCloseObjects(
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
  for (auto& decoded_frame_writer : decoded_frame_writers_) {
    decoded_frame_writer->Close();
  }
}

void VideoProcessorIntegrationTest::PrintSettings() const {
  printf("VideoProcessor settings\n==\n");
  printf(" Total # of frames      : %d",
         source_frame_reader_->NumberOfFrames());
  printf("%s\n", config_.ToString().c_str());

  printf("VideoProcessorIntegrationTest settings\n==\n");
  const char* encoder_name = encoder_->ImplementationName();
  printf(" Encoder implementation name: %s\n", encoder_name);
  const char* decoder_name = decoders_.at(0)->ImplementationName();
  printf(" Decoder implementation name: %s\n", decoder_name);
  if (strcmp(encoder_name, decoder_name) == 0) {
    printf(" Codec implementation name  : %s_%s\n", config_.CodecName().c_str(),
           encoder_name);
  }
  printf("\n");
}

void VideoProcessorIntegrationTest::AnalyzeAndPrintStats(
    const std::vector<FrameStatistic>& stats,
    const float target_bitrate_kbps,
    const float target_framerate_fps,
    const float input_duration_sec,
    const RateControlThresholds* rc_thresholds,
    const QualityThresholds* quality_thresholds,
    const BitstreamThresholds* bs_thresholds) {
  const size_t num_input_frames = stats.size();
  size_t num_dropped_frames = 0;
  size_t num_decoded_frames = 0;
  size_t num_spatial_resizes = 0;
  size_t num_key_frames = 0;
  size_t max_nalu_size_bytes = 0;

  size_t encoded_bytes = 0;
  float buffer_level_kbits = 0.0;
  float time_to_reach_target_bitrate_sec = -1.0;

  Statistics buffer_level_sec;
  Statistics key_frame_size_bytes;
  Statistics delta_frame_size_bytes;

  Statistics encoding_time_us;
  Statistics decoding_time_us;
  Statistics psnr;
  Statistics ssim;

  Statistics qp;

  FrameStatistic last_successfully_decoded_frame(0, 0);
  for (size_t frame_idx = 0; frame_idx < stats.size(); ++frame_idx) {
    const FrameStatistic& frame_stat = stats[frame_idx];

    const float time_since_first_input_sec =
        frame_idx == 0
            ? 0.0
            : 1.0 * (frame_stat.rtp_timestamp - stats[0].rtp_timestamp) /
                  kRtpClockRateHz;
    const float time_since_last_input_sec =
        frame_idx == 0 ? 0.0
                       : 1.0 *
                             (frame_stat.rtp_timestamp -
                              stats[frame_idx - 1].rtp_timestamp) /
                             kRtpClockRateHz;

    // Testing framework uses constant input framerate. This guarantees even
    // sampling, which is important, of buffer level.
    buffer_level_kbits -= time_since_last_input_sec * target_bitrate_kbps;
    buffer_level_kbits = std::max(0.0f, buffer_level_kbits);
    buffer_level_kbits += 8.0 * frame_stat.encoded_frame_size_bytes / 1000;
    buffer_level_sec.AddSample(buffer_level_kbits / target_bitrate_kbps);

    encoded_bytes += frame_stat.encoded_frame_size_bytes;
    if (frame_stat.encoded_frame_size_bytes == 0) {
      ++num_dropped_frames;
    } else {
      if (frame_stat.frame_type == kVideoFrameKey) {
        key_frame_size_bytes.AddSample(frame_stat.encoded_frame_size_bytes);
        ++num_key_frames;
      } else {
        delta_frame_size_bytes.AddSample(frame_stat.encoded_frame_size_bytes);
      }

      encoding_time_us.AddSample(frame_stat.encode_time_us);
      qp.AddSample(frame_stat.qp);

      max_nalu_size_bytes =
          std::max(max_nalu_size_bytes, frame_stat.max_nalu_size_bytes);
    }

    if (frame_stat.decoding_successful) {
      psnr.AddSample(frame_stat.psnr);
      ssim.AddSample(frame_stat.ssim);
      if (num_decoded_frames > 0) {
        if (last_successfully_decoded_frame.decoded_width !=
                frame_stat.decoded_width ||
            last_successfully_decoded_frame.decoded_height !=
                frame_stat.decoded_height) {
          ++num_spatial_resizes;
        }
      }
      decoding_time_us.AddSample(frame_stat.decode_time_us);
      last_successfully_decoded_frame = frame_stat;
      ++num_decoded_frames;
    }

    if (time_to_reach_target_bitrate_sec < 0 && frame_idx > 0) {
      const float curr_bitrate_kbps =
          (8.0 * encoded_bytes / 1000) / time_since_first_input_sec;
      const float bitrate_mismatch_percent =
          100 * std::fabs(curr_bitrate_kbps - target_bitrate_kbps) /
          target_bitrate_kbps;
      if (bitrate_mismatch_percent < kMaxBitrateMismatchPercent) {
        time_to_reach_target_bitrate_sec = time_since_first_input_sec;
      }
    }
  }

  const float encoded_bitrate_kbps =
      8 * encoded_bytes / input_duration_sec / 1000;
  const float bitrate_mismatch_percent =
      100 * std::fabs(encoded_bitrate_kbps - target_bitrate_kbps) /
      target_bitrate_kbps;
  const size_t num_encoded_frames = num_input_frames - num_dropped_frames;
  const float encoded_framerate_fps = num_encoded_frames / input_duration_sec;
  const float decoded_framerate_fps = num_decoded_frames / input_duration_sec;
  const float framerate_mismatch_percent =
      100 * std::fabs(decoded_framerate_fps - target_framerate_fps) /
      target_framerate_fps;
  const float max_key_frame_delay_sec =
      8 * key_frame_size_bytes.Max() / 1000 / target_bitrate_kbps;
  const float max_delta_frame_delay_sec =
      8 * delta_frame_size_bytes.Max() / 1000 / target_bitrate_kbps;

  printf("Target bitrate                 : %f kbps\n", target_bitrate_kbps);
  printf("Encoded bitrate                : %f kbps\n", encoded_bitrate_kbps);
  printf("Bitrate mismatch               : %f %%\n", bitrate_mismatch_percent);
  printf("Time to reach target bitrate   : %f sec\n",
         time_to_reach_target_bitrate_sec);
  printf("Target framerate               : %f fps\n", target_framerate_fps);
  printf("Encoding framerate             : %f fps\n", encoded_framerate_fps);
  printf("Decoding framerate             : %f fps\n", decoded_framerate_fps);
  printf("Frame encoding time            : %f us\n", encoding_time_us.Mean());
  printf("Frame decoding time            : %f us\n", decoding_time_us.Mean());
  printf("Framerate mismatch percent     : %f %%\n",
         framerate_mismatch_percent);
  printf("Avg buffer level               : %f sec\n", buffer_level_sec.Mean());
  printf("Max key frame delay            : %f sec\n", max_key_frame_delay_sec);
  printf("Max delta frame delay          : %f sec\n",
         max_delta_frame_delay_sec);
  printf("Avg key frame size             : %f bytes\n",
         key_frame_size_bytes.Mean());
  printf("Avg delta frame size           : %f bytes\n",
         delta_frame_size_bytes.Mean());
  printf("Avg QP                         : %f\n", qp.Mean());
  printf("Avg PSNR                       : %f dB\n", psnr.Mean());
  printf("Min PSNR                       : %f dB\n", psnr.Min());
  printf("Avg SSIM                       : %f\n", ssim.Mean());
  printf("Min SSIM                       : %f\n", ssim.Min());
  printf("# input frames                 : %zu\n", num_input_frames);
  printf("# encoded frames               : %zu\n", num_encoded_frames);
  printf("# decoded frames               : %zu\n", num_decoded_frames);
  printf("# dropped frames               : %zu\n", num_dropped_frames);
  printf("# key frames                   : %zu\n", num_key_frames);
  printf("# encoded bytes                : %zu\n", encoded_bytes);
  printf("# spatial resizes              : %zu\n", num_spatial_resizes);

  if (rc_thresholds) {
    EXPECT_LE(bitrate_mismatch_percent,
              rc_thresholds->max_avg_bitrate_mismatch_percent);
    EXPECT_LE(time_to_reach_target_bitrate_sec,
              rc_thresholds->max_time_to_reach_target_bitrate_sec);
    EXPECT_LE(framerate_mismatch_percent,
              rc_thresholds->max_avg_framerate_mismatch_percent);
    EXPECT_LE(buffer_level_sec.Mean(), rc_thresholds->max_avg_buffer_level_sec);
    EXPECT_LE(max_key_frame_delay_sec,
              rc_thresholds->max_max_key_frame_delay_sec);
    EXPECT_LE(max_delta_frame_delay_sec,
              rc_thresholds->max_max_delta_frame_delay_sec);
    EXPECT_LE(num_spatial_resizes, rc_thresholds->max_num_spatial_resizes);
    EXPECT_LE(num_key_frames, rc_thresholds->max_num_key_frames);
  }

  if (quality_thresholds) {
    EXPECT_GT(psnr.Mean(), quality_thresholds->min_avg_psnr);
    EXPECT_GT(psnr.Min(), quality_thresholds->min_min_psnr);
    EXPECT_GT(ssim.Mean(), quality_thresholds->min_avg_ssim);
    EXPECT_GT(ssim.Min(), quality_thresholds->min_min_ssim);
  }

  if (bs_thresholds) {
    EXPECT_LE(max_nalu_size_bytes, bs_thresholds->max_max_nalu_size_bytes);
  }
}

void VideoProcessorIntegrationTest::PrintFrameLevelStats(
    const std::vector<FrameStatistic>& stats) const {
  for (const auto& frame_stat : stats) {
    printf("%s\n", frame_stat.ToString().c_str());
  }
}

}  // namespace test
}  // namespace webrtc
