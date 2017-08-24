/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/test/videoprocessor.h"

#include <string.h>

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/video_coding/codecs/vp8/simulcast_rate_allocator.h"
#include "webrtc/modules/video_coding/include/video_codec_initializer.h"
#include "webrtc/modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/timeutils.h"
#include "webrtc/system_wrappers/include/cpu_info.h"

namespace webrtc {
namespace test {

namespace {

const int kRtpClockRateHz = 90000;

std::unique_ptr<VideoBitrateAllocator> CreateBitrateAllocator(
    TestConfig* config) {
  std::unique_ptr<TemporalLayersFactory> tl_factory;
  if (config->codec_settings.codecType == VideoCodecType::kVideoCodecVP8) {
    tl_factory.reset(new TemporalLayersFactory());
    config->codec_settings.VP8()->tl_factory = tl_factory.get();
  }
  return std::unique_ptr<VideoBitrateAllocator>(
      VideoCodecInitializer::CreateBitrateAllocator(config->codec_settings,
                                                    std::move(tl_factory)));
}

void PrintCodecSettings(const VideoCodec& codec_settings) {
  printf(" Codec settings:\n");
  printf("  Codec type        : %s\n",
         CodecTypeToPayloadString(codec_settings.codecType));
  printf("  Start bitrate     : %d kbps\n", codec_settings.startBitrate);
  printf("  Max bitrate       : %d kbps\n", codec_settings.maxBitrate);
  printf("  Min bitrate       : %d kbps\n", codec_settings.minBitrate);
  printf("  Width             : %d\n", codec_settings.width);
  printf("  Height            : %d\n", codec_settings.height);
  printf("  Max frame rate    : %d\n", codec_settings.maxFramerate);
  printf("  QPmax             : %d\n", codec_settings.qpMax);
  if (codec_settings.codecType == kVideoCodecVP8) {
    printf("  Complexity        : %d\n", codec_settings.VP8().complexity);
    printf("  Resilience        : %d\n", codec_settings.VP8().resilience);
    printf("  # temporal layers : %d\n",
           codec_settings.VP8().numberOfTemporalLayers);
    printf("  Denoising         : %d\n", codec_settings.VP8().denoisingOn);
    printf("  Error concealment : %d\n",
           codec_settings.VP8().errorConcealmentOn);
    printf("  Automatic resize  : %d\n",
           codec_settings.VP8().automaticResizeOn);
    printf("  Frame dropping    : %d\n", codec_settings.VP8().frameDroppingOn);
    printf("  Key frame interval: %d\n", codec_settings.VP8().keyFrameInterval);
  } else if (codec_settings.codecType == kVideoCodecVP9) {
    printf("  Complexity        : %d\n", codec_settings.VP9().complexity);
    printf("  Resilience        : %d\n", codec_settings.VP9().resilienceOn);
    printf("  # temporal layers : %d\n",
           codec_settings.VP9().numberOfTemporalLayers);
    printf("  Denoising         : %d\n", codec_settings.VP9().denoisingOn);
    printf("  Frame dropping    : %d\n", codec_settings.VP9().frameDroppingOn);
    printf("  Key frame interval: %d\n", codec_settings.VP9().keyFrameInterval);
    printf("  Adaptive QP mode  : %d\n", codec_settings.VP9().adaptiveQpMode);
    printf("  Automatic resize  : %d\n",
           codec_settings.VP9().automaticResizeOn);
    printf("  # spatial layers  : %d\n",
           codec_settings.VP9().numberOfSpatialLayers);
    printf("  Flexible mode     : %d\n", codec_settings.VP9().flexibleMode);
  } else if (codec_settings.codecType == kVideoCodecH264) {
    printf("  Frame dropping    : %d\n", codec_settings.H264().frameDroppingOn);
    printf("  Key frame interval: %d\n",
           codec_settings.H264().keyFrameInterval);
    printf("  Profile           : %d\n", codec_settings.H264().profile);
  }
}

int GetElapsedTimeMicroseconds(int64_t start_ns, int64_t stop_ns) {
  int64_t diff_us = (stop_ns - start_ns) / rtc::kNumNanosecsPerMicrosec;
  RTC_DCHECK_GE(diff_us, std::numeric_limits<int>::min());
  RTC_DCHECK_LE(diff_us, std::numeric_limits<int>::max());
  return static_cast<int>(diff_us);
}

}  // namespace

const char* ExcludeFrameTypesToStr(ExcludeFrameTypes e) {
  switch (e) {
    case kExcludeOnlyFirstKeyFrame:
      return "ExcludeOnlyFirstKeyFrame";
    case kExcludeAllKeyFrames:
      return "ExcludeAllKeyFrames";
    default:
      RTC_NOTREACHED();
      return "Unknown";
  }
}

VideoProcessor::VideoProcessor(webrtc::VideoEncoder* encoder,
                               webrtc::VideoDecoder* decoder,
                               FrameReader* analysis_frame_reader,
                               FrameWriter* analysis_frame_writer,
                               PacketManipulator* packet_manipulator,
                               const TestConfig& config,
                               Stats* stats,
                               IvfFileWriter* encoded_frame_writer,
                               FrameWriter* decoded_frame_writer)
    : initialized_(false),
      config_(config),
      encoder_(encoder),
      decoder_(decoder),
      bitrate_allocator_(CreateBitrateAllocator(&config_)),
      encode_callback_(this),
      decode_callback_(this),
      packet_manipulator_(packet_manipulator),
      analysis_frame_reader_(analysis_frame_reader),
      analysis_frame_writer_(analysis_frame_writer),
      encoded_frame_writer_(encoded_frame_writer),
      decoded_frame_writer_(decoded_frame_writer),
      last_encoded_frame_num_(-1),
      last_decoded_frame_num_(-1),
      first_key_frame_has_been_excluded_(false),
      last_decoded_frame_buffer_(analysis_frame_reader->FrameLength()),
      stats_(stats),
      num_dropped_frames_(0),
      num_spatial_resizes_(0) {
  RTC_DCHECK(encoder);
  RTC_DCHECK(decoder);
  RTC_DCHECK(packet_manipulator);
  RTC_DCHECK(analysis_frame_reader);
  RTC_DCHECK(analysis_frame_writer);
  RTC_DCHECK(stats);
  frame_infos_.reserve(analysis_frame_reader->NumberOfFrames());
}

VideoProcessor::~VideoProcessor() = default;

void VideoProcessor::Init() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  RTC_DCHECK(!initialized_) << "VideoProcessor already initialized.";
  initialized_ = true;

  // Setup required callbacks for the encoder and decoder.
  RTC_CHECK_EQ(encoder_->RegisterEncodeCompleteCallback(&encode_callback_),
               WEBRTC_VIDEO_CODEC_OK)
      << "Failed to register encode complete callback";
  RTC_CHECK_EQ(decoder_->RegisterDecodeCompleteCallback(&decode_callback_),
               WEBRTC_VIDEO_CODEC_OK)
      << "Failed to register decode complete callback";

  // Initialize the encoder and decoder.
  uint32_t num_cores =
      config_.use_single_core ? 1 : CpuInfo::DetectNumberOfCores();
  RTC_CHECK_EQ(
      encoder_->InitEncode(&config_.codec_settings, num_cores,
                           config_.networking_config.max_payload_size_in_bytes),
      WEBRTC_VIDEO_CODEC_OK)
      << "Failed to initialize VideoEncoder";

  RTC_CHECK_EQ(decoder_->InitDecode(&config_.codec_settings, num_cores),
               WEBRTC_VIDEO_CODEC_OK)
      << "Failed to initialize VideoDecoder";

  if (config_.verbose) {
    printf("Video Processor:\n");
    printf(" Filename         : %s\n", config_.filename.c_str());
    printf(" Total # of frames: %d\n",
           analysis_frame_reader_->NumberOfFrames());
    printf(" # CPU cores used : %d\n", num_cores);
    const char* encoder_name = encoder_->ImplementationName();
    printf(" Encoder implementation name: %s\n", encoder_name);
    const char* decoder_name = decoder_->ImplementationName();
    printf(" Decoder implementation name: %s\n", decoder_name);
    if (strcmp(encoder_name, decoder_name) == 0) {
      printf(" Codec implementation name  : %s_%s\n",
             CodecTypeToPayloadString(config_.codec_settings.codecType),
             encoder_->ImplementationName());
    }
    PrintCodecSettings(config_.codec_settings);
    printf("\n");
  }
}

void VideoProcessor::Release() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  RTC_CHECK_EQ(encoder_->Release(), WEBRTC_VIDEO_CODEC_OK);
  RTC_CHECK_EQ(decoder_->Release(), WEBRTC_VIDEO_CODEC_OK);

  encoder_->RegisterEncodeCompleteCallback(nullptr);
  decoder_->RegisterDecodeCompleteCallback(nullptr);

  initialized_ = false;
}

void VideoProcessor::ProcessFrame(int frame_number) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  RTC_DCHECK_EQ(frame_number, frame_infos_.size())
      << "Must process frames in sequence.";
  RTC_DCHECK(initialized_) << "VideoProcessor not initialized.";

  // Get frame from file.
  rtc::scoped_refptr<I420BufferInterface> buffer(
      analysis_frame_reader_->ReadFrame());
  RTC_CHECK(buffer) << "Tried to read too many frames from the file.";
  const int64_t kNoRenderTime = 0;
  VideoFrame source_frame(buffer, FrameNumberToTimestamp(frame_number),
                          kNoRenderTime, webrtc::kVideoRotation_0);

  // Decide if we are going to force a keyframe.
  std::vector<FrameType> frame_types(1, kVideoFrameDelta);
  if (config_.keyframe_interval > 0 &&
      frame_number % config_.keyframe_interval == 0) {
    frame_types[0] = kVideoFrameKey;
  }

  // Store frame information during the different stages of encode and decode.
  frame_infos_.emplace_back();
  FrameInfo* frame_info = &frame_infos_.back();

  // Create frame statistics object used for aggregation at end of test run.
  FrameStatistic* frame_stat = &stats_->NewFrame(frame_number);

  // For the highest measurement accuracy of the encode time, the start/stop
  // time recordings should wrap the Encode call as tightly as possible.
  frame_info->encode_start_ns = rtc::TimeNanos();
  frame_stat->encode_return_code =
      encoder_->Encode(source_frame, nullptr, &frame_types);

  if (frame_stat->encode_return_code != WEBRTC_VIDEO_CODEC_OK) {
    LOG(LS_WARNING) << "Failed to encode frame " << frame_number
                    << ", return code: " << frame_stat->encode_return_code
                    << ".";
  }
}

void VideoProcessor::SetRates(int bitrate_kbps, int framerate_fps) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  config_.codec_settings.maxFramerate = framerate_fps;
  int set_rates_result = encoder_->SetRateAllocation(
      bitrate_allocator_->GetAllocation(bitrate_kbps * 1000, framerate_fps),
      framerate_fps);
  RTC_DCHECK_GE(set_rates_result, 0)
      << "Failed to update encoder with new rate " << bitrate_kbps << ".";
  num_dropped_frames_ = 0;
  num_spatial_resizes_ = 0;
}

int VideoProcessor::GetQpFromEncoder(int frame_number) const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  RTC_CHECK_LT(frame_number, frame_infos_.size());
  return frame_infos_[frame_number].qp_encoder;
}

int VideoProcessor::GetQpFromBitstream(int frame_number) const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  RTC_CHECK_LT(frame_number, frame_infos_.size());
  return frame_infos_[frame_number].qp_bitstream;
}

int VideoProcessor::NumberDroppedFrames() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  return num_dropped_frames_;
}

int VideoProcessor::NumberSpatialResizes() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  return num_spatial_resizes_;
}

void VideoProcessor::FrameEncoded(webrtc::VideoCodecType codec,
                                  const EncodedImage& encoded_image) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  // For the highest measurement accuracy of the encode time, the start/stop
  // time recordings should wrap the Encode call as tightly as possible.
  int64_t encode_stop_ns = rtc::TimeNanos();

  if (encoded_frame_writer_) {
    RTC_CHECK(encoded_frame_writer_->WriteFrame(encoded_image, codec));
  }

  // Timestamp is proportional to frame number, so this gives us number of
  // dropped frames.
  int frame_number = TimestampToFrameNumber(encoded_image._timeStamp);
  bool last_frame_missing = false;
  if (frame_number > 0) {
    RTC_DCHECK_GE(last_encoded_frame_num_, 0);
    int num_dropped_from_last_encode =
        frame_number - last_encoded_frame_num_ - 1;
    RTC_DCHECK_GE(num_dropped_from_last_encode, 0);
    num_dropped_frames_ += num_dropped_from_last_encode;
    if (num_dropped_from_last_encode > 0) {
      // For dropped frames, we write out the last decoded frame to avoid
      // getting out of sync for the computation of PSNR and SSIM.
      for (int i = 0; i < num_dropped_from_last_encode; i++) {
        RTC_DCHECK_EQ(last_decoded_frame_buffer_.size(),
                      analysis_frame_writer_->FrameLength());
        RTC_CHECK(analysis_frame_writer_->WriteFrame(
            last_decoded_frame_buffer_.data()));
        if (decoded_frame_writer_) {
          RTC_DCHECK_EQ(last_decoded_frame_buffer_.size(),
                        decoded_frame_writer_->FrameLength());
          RTC_CHECK(decoded_frame_writer_->WriteFrame(
              last_decoded_frame_buffer_.data()));
        }
      }
    }

    last_frame_missing =
        (frame_infos_[last_encoded_frame_num_].manipulated_length == 0);
  }
  // Ensure strict monotonicity.
  RTC_CHECK_GT(frame_number, last_encoded_frame_num_);
  last_encoded_frame_num_ = frame_number;

  // Frame is not dropped, so update frame information and statistics.
  RTC_CHECK_LT(frame_number, frame_infos_.size());
  FrameInfo* frame_info = &frame_infos_[frame_number];
  frame_info->qp_encoder = encoded_image.qp_;
  if (codec == kVideoCodecVP8) {
    vp8::GetQp(encoded_image._buffer, encoded_image._length,
      &frame_info->qp_bitstream);
  } else if (codec == kVideoCodecVP9) {
    vp9::GetQp(encoded_image._buffer, encoded_image._length,
      &frame_info->qp_bitstream);
  }
  FrameStatistic* frame_stat = &stats_->stats_[frame_number];
  frame_stat->encode_time_in_us =
      GetElapsedTimeMicroseconds(frame_info->encode_start_ns, encode_stop_ns);
  frame_stat->encoding_successful = true;
  frame_stat->encoded_frame_length_in_bytes = encoded_image._length;
  frame_stat->frame_number = frame_number;
  frame_stat->frame_type = encoded_image._frameType;
  frame_stat->qp = encoded_image.qp_;
  frame_stat->bit_rate_in_kbps = static_cast<int>(
      encoded_image._length * config_.codec_settings.maxFramerate * 8 / 1000);
  frame_stat->total_packets =
      encoded_image._length / config_.networking_config.packet_size_in_bytes +
      1;

  // Simulate packet loss.
  bool exclude_this_frame = false;
  if (encoded_image._frameType == kVideoFrameKey) {
    // Only keyframes can be excluded.
    switch (config_.exclude_frame_types) {
      case kExcludeOnlyFirstKeyFrame:
        if (!first_key_frame_has_been_excluded_) {
          first_key_frame_has_been_excluded_ = true;
          exclude_this_frame = true;
        }
        break;
      case kExcludeAllKeyFrames:
        exclude_this_frame = true;
        break;
      default:
        RTC_NOTREACHED();
    }
  }

  // Make a raw copy of the |encoded_image| buffer.
  size_t copied_buffer_size = encoded_image._length +
                              EncodedImage::GetBufferPaddingBytes(codec);
  std::unique_ptr<uint8_t[]> copied_buffer(new uint8_t[copied_buffer_size]);
  memcpy(copied_buffer.get(), encoded_image._buffer, encoded_image._length);
  // The image to feed to the decoder.
  EncodedImage copied_image;
  memcpy(&copied_image, &encoded_image, sizeof(copied_image));
  copied_image._size = copied_buffer_size;
  copied_image._buffer = copied_buffer.get();

  if (!exclude_this_frame) {
    frame_stat->packets_dropped =
        packet_manipulator_->ManipulatePackets(&copied_image);
  }
  frame_info->manipulated_length = copied_image._length;

  // For the highest measurement accuracy of the decode time, the start/stop
  // time recordings should wrap the Decode call as tightly as possible.
  frame_info->decode_start_ns = rtc::TimeNanos();
  frame_stat->decode_return_code =
      decoder_->Decode(copied_image, last_frame_missing, nullptr);

  if (frame_stat->decode_return_code != WEBRTC_VIDEO_CODEC_OK) {
    // Write the last successful frame the output file to avoid getting it out
    // of sync with the source file for SSIM and PSNR comparisons.
    RTC_DCHECK_EQ(last_decoded_frame_buffer_.size(),
                  analysis_frame_writer_->FrameLength());
    RTC_CHECK(
        analysis_frame_writer_->WriteFrame(last_decoded_frame_buffer_.data()));
    if (decoded_frame_writer_) {
      RTC_DCHECK_EQ(last_decoded_frame_buffer_.size(),
                    decoded_frame_writer_->FrameLength());
      RTC_CHECK(
          decoded_frame_writer_->WriteFrame(last_decoded_frame_buffer_.data()));
    }
  }
}

void VideoProcessor::FrameDecoded(const VideoFrame& image) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  // For the highest measurement accuracy of the decode time, the start/stop
  // time recordings should wrap the Decode call as tightly as possible.
  int64_t decode_stop_ns = rtc::TimeNanos();

  // Update frame information and statistics.
  int frame_number = TimestampToFrameNumber(image.timestamp());
  RTC_DCHECK_LT(frame_number, frame_infos_.size());
  FrameInfo* frame_info = &frame_infos_[frame_number];
  frame_info->decoded_width = image.width();
  frame_info->decoded_height = image.height();
  FrameStatistic* frame_stat = &stats_->stats_[frame_number];
  frame_stat->decode_time_in_us =
      GetElapsedTimeMicroseconds(frame_info->decode_start_ns, decode_stop_ns);
  frame_stat->decoding_successful = true;

  // Check if the codecs have resized the frame since previously decoded frame.
  if (frame_number > 0) {
    RTC_DCHECK_GE(last_decoded_frame_num_, 0);
    const FrameInfo& last_decoded_frame_info =
        frame_infos_[last_decoded_frame_num_];
    if (static_cast<int>(image.width()) !=
            last_decoded_frame_info.decoded_width ||
        static_cast<int>(image.height()) !=
            last_decoded_frame_info.decoded_height) {
      ++num_spatial_resizes_;
    }
  }
  // Ensure strict monotonicity.
  RTC_CHECK_GT(frame_number, last_decoded_frame_num_);
  last_decoded_frame_num_ = frame_number;

  // Check if codec size is different from the original size, and if so,
  // scale back to original size. This is needed for the PSNR and SSIM
  // calculations.
  size_t extracted_length;
  rtc::Buffer extracted_buffer;
  if (image.width() != config_.codec_settings.width ||
      image.height() != config_.codec_settings.height) {
    rtc::scoped_refptr<I420Buffer> scaled_buffer(I420Buffer::Create(
        config_.codec_settings.width, config_.codec_settings.height));
    // Should be the same aspect ratio, no cropping needed.
    scaled_buffer->ScaleFrom(*image.video_frame_buffer()->ToI420());

    size_t length = CalcBufferSize(VideoType::kI420, scaled_buffer->width(),
                                   scaled_buffer->height());
    extracted_buffer.SetSize(length);
    extracted_length =
        ExtractBuffer(scaled_buffer, length, extracted_buffer.data());
  } else {
    // No resize.
    size_t length =
        CalcBufferSize(VideoType::kI420, image.width(), image.height());
    extracted_buffer.SetSize(length);
    extracted_length = ExtractBuffer(image.video_frame_buffer()->ToI420(),
                                     length, extracted_buffer.data());
  }

  RTC_DCHECK_EQ(extracted_length, analysis_frame_writer_->FrameLength());
  RTC_CHECK(analysis_frame_writer_->WriteFrame(extracted_buffer.data()));
  if (decoded_frame_writer_) {
    RTC_DCHECK_EQ(extracted_length, decoded_frame_writer_->FrameLength());
    RTC_CHECK(decoded_frame_writer_->WriteFrame(extracted_buffer.data()));
  }

  last_decoded_frame_buffer_ = std::move(extracted_buffer);
}

uint32_t VideoProcessor::FrameNumberToTimestamp(int frame_number) const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  RTC_DCHECK_GE(frame_number, 0);
  const int ticks_per_frame =
      kRtpClockRateHz / config_.codec_settings.maxFramerate;
  return (frame_number + 1) * ticks_per_frame;
}

int VideoProcessor::TimestampToFrameNumber(uint32_t timestamp) const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  RTC_DCHECK_GT(timestamp, 0);
  const int ticks_per_frame =
      kRtpClockRateHz / config_.codec_settings.maxFramerate;
  RTC_DCHECK_EQ(timestamp % ticks_per_frame, 0);
  return (timestamp / ticks_per_frame) - 1;
}

}  // namespace test
}  // namespace webrtc
