/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videoprocessor.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "api/video/i420_buffer.h"
#include "common_types.h"  // NOLINT(build/include)
#include "common_video/h264/h264_common.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/codecs/vp8/simulcast_rate_allocator.h"
#include "modules/video_coding/include/video_codec_initializer.h"
#include "modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "rtc_base/checks.h"
#include "rtc_base/timeutils.h"
#include "test/gtest.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace webrtc {
namespace test {

namespace {
const int kMsToRtpTimestamp = kVideoPayloadTypeFrequency / 1000;

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

size_t GetMaxNaluSizeBytes(const EncodedImage& encoded_frame,
                           const TestConfig& config) {
  if (config.codec_settings.codecType != kVideoCodecH264)
    return 0;

  std::vector<webrtc::H264::NaluIndex> nalu_indices =
      webrtc::H264::FindNaluIndices(encoded_frame._buffer,
                                    encoded_frame._length);

  RTC_CHECK(!nalu_indices.empty());

  size_t max_size = 0;
  for (const webrtc::H264::NaluIndex& index : nalu_indices)
    max_size = std::max(max_size, index.payload_size);

  return max_size;
}

int GetElapsedTimeMicroseconds(int64_t start_ns, int64_t stop_ns) {
  int64_t diff_us = (stop_ns - start_ns) / rtc::kNumNanosecsPerMicrosec;
  RTC_DCHECK_GE(diff_us, std::numeric_limits<int>::min());
  RTC_DCHECK_LE(diff_us, std::numeric_limits<int>::max());
  return static_cast<int>(diff_us);
}

void ExtractBufferWithSize(const VideoFrame& image,
                           int width,
                           int height,
                           rtc::Buffer* buffer) {
  if (image.width() != width || image.height() != height) {
    EXPECT_DOUBLE_EQ(static_cast<double>(width) / height,
                     static_cast<double>(image.width()) / image.height());
    // Same aspect ratio, no cropping needed.
    rtc::scoped_refptr<I420Buffer> scaled(I420Buffer::Create(width, height));
    scaled->ScaleFrom(*image.video_frame_buffer()->ToI420());

    size_t length =
        CalcBufferSize(VideoType::kI420, scaled->width(), scaled->height());
    buffer->SetSize(length);
    RTC_CHECK_NE(ExtractBuffer(scaled, length, buffer->data()), -1);
    return;
  }

  // No resize.
  size_t length =
      CalcBufferSize(VideoType::kI420, image.width(), image.height());
  buffer->SetSize(length);
  RTC_CHECK_NE(ExtractBuffer(image, length, buffer->data()), -1);
}

}  // namespace

VideoProcessor::VideoProcessor(webrtc::VideoEncoder* encoder,
                               VideoDecoderList* decoders,
                               FrameReader* input_frame_reader,
                               const TestConfig& config,
                               std::vector<Stats>* stats,
                               IvfFileWriterList* encoded_frame_writers,
                               FrameWriterList* decoded_frame_writers)
    : config_(config),
      num_simulcast_or_spatial_layers_(
          std::max(config_.NumberOfSimulcastStreams(),
                   config_.NumberOfSpatialLayers())),
      encoder_(encoder),
      decoders_(decoders),
      bitrate_allocator_(CreateBitrateAllocator(&config_)),
      encode_callback_(this),
      decode_callback_(this),
      input_frame_reader_(input_frame_reader),
      encoded_frame_writers_(encoded_frame_writers),
      decoded_frame_writers_(decoded_frame_writers),
      last_inputed_frame_num_(0),
      last_encoded_frame_num_(0),
      last_encoded_simulcast_svc_idx_(0),
      last_decoded_frame_num_(0),
      num_encoded_frames_(0),
      num_decoded_frames_(0),
      stats_(stats) {
  RTC_CHECK(encoder);
  RTC_CHECK(decoders && decoders->size() == num_simulcast_or_spatial_layers_);
  RTC_CHECK(input_frame_reader);
  RTC_CHECK(stats);
  RTC_CHECK(!encoded_frame_writers ||
            encoded_frame_writers->size() == num_simulcast_or_spatial_layers_);
  RTC_CHECK(!decoded_frame_writers ||
            decoded_frame_writers->size() == num_simulcast_or_spatial_layers_);

  // Setup required callbacks for the encoder and decoder and initialize them.
  RTC_CHECK_EQ(encoder_->RegisterEncodeCompleteCallback(&encode_callback_),
               WEBRTC_VIDEO_CODEC_OK);

  RTC_CHECK_EQ(encoder_->InitEncode(&config_.codec_settings,
                                    static_cast<int>(config_.NumberOfCores()),
                                    config_.max_payload_size_bytes),
               WEBRTC_VIDEO_CODEC_OK);

  for (auto& decoder : *decoders_) {
    RTC_CHECK_EQ(decoder->InitDecode(&config_.codec_settings,
                                     static_cast<int>(config_.NumberOfCores())),
                 WEBRTC_VIDEO_CODEC_OK);
    RTC_CHECK_EQ(decoder->RegisterDecodeCompleteCallback(&decode_callback_),
                 WEBRTC_VIDEO_CODEC_OK);
  }
}

VideoProcessor::~VideoProcessor() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  RTC_CHECK_EQ(encoder_->Release(), WEBRTC_VIDEO_CODEC_OK);
  encoder_->RegisterEncodeCompleteCallback(nullptr);

  for (auto& decoder : *decoders_) {
    RTC_CHECK_EQ(decoder->Release(), WEBRTC_VIDEO_CODEC_OK);
    decoder->RegisterDecodeCompleteCallback(nullptr);
  }

  RTC_CHECK(last_encoded_frames_.empty());
}

void VideoProcessor::ProcessFrame() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  const size_t frame_number = last_inputed_frame_num_++;

  // Get frame from file.
  rtc::scoped_refptr<I420BufferInterface> buffer(
      input_frame_reader_->ReadFrame());
  RTC_CHECK(buffer) << "Tried to read too many frames from the file.";

  size_t rtp_timestamp =
      (frame_number > 0) ? input_frames_[frame_number - 1]->timestamp() : 0;
  rtp_timestamp +=
      kVideoPayloadTypeFrequency / config_.codec_settings.maxFramerate;

  input_frames_[frame_number] = rtc::MakeUnique<VideoFrame>(
      buffer, static_cast<uint32_t>(rtp_timestamp),
      static_cast<int64_t>(rtp_timestamp / kMsToRtpTimestamp),
      webrtc::kVideoRotation_0);

  std::vector<FrameType> frame_types = config_.FrameTypeForFrame(frame_number);

  // Create frame statistics object for all simulcast /spatial layers.
  for (size_t simulcast_svc_idx = 0;
       simulcast_svc_idx < num_simulcast_or_spatial_layers_;
       ++simulcast_svc_idx) {
    stats_->at(simulcast_svc_idx).AddFrame(rtp_timestamp);
  }

  // For the highest measurement accuracy of the encode time, the start/stop
  // time recordings should wrap the Encode call as tightly as possible.
  const int64_t encode_start_ns = rtc::TimeNanos();
  for (size_t simulcast_svc_idx = 0;
       simulcast_svc_idx < num_simulcast_or_spatial_layers_;
       ++simulcast_svc_idx) {
    FrameStatistic* frame_stat =
        stats_->at(simulcast_svc_idx).GetFrame(frame_number);
    frame_stat->encode_start_ns = encode_start_ns;
  }

  const int encode_return_code =
      encoder_->Encode(*input_frames_[frame_number], nullptr, &frame_types);

  for (size_t simulcast_svc_idx = 0;
       simulcast_svc_idx < num_simulcast_or_spatial_layers_;
       ++simulcast_svc_idx) {
    FrameStatistic* frame_stat =
        stats_->at(simulcast_svc_idx).GetFrame(frame_number);
    frame_stat->encode_return_code = encode_return_code;
  }

  // For async codecs frame decoding is done in frame encode callback.
  if (!config_.IsAsyncCodec()) {
    for (size_t simulcast_svc_idx = 0;
         simulcast_svc_idx < num_simulcast_or_spatial_layers_;
         ++simulcast_svc_idx) {
      if (last_encoded_frames_.find(simulcast_svc_idx) !=
          last_encoded_frames_.end()) {
        EncodedImage& encoded_image = last_encoded_frames_[simulcast_svc_idx];

        FrameStatistic* frame_stat =
            stats_->at(simulcast_svc_idx).GetFrame(frame_number);

        if (encoded_frame_writers_) {
          RTC_CHECK(encoded_frame_writers_->at(simulcast_svc_idx)
                        ->WriteFrame(encoded_image,
                                     config_.codec_settings.codecType));
        }

        // For the highest measurement accuracy of the decode time, the
        // start/stop time recordings should wrap the Decode call as tightly as
        // possible.
        frame_stat->decode_start_ns = rtc::TimeNanos();
        frame_stat->decode_return_code =
            decoders_->at(simulcast_svc_idx)
                ->Decode(encoded_image, false, nullptr);

        RTC_CHECK(encoded_image._buffer);
        delete[] encoded_image._buffer;
        encoded_image._buffer = nullptr;

        last_encoded_frames_.erase(simulcast_svc_idx);
      }
    }
  }
}

void VideoProcessor::SetRates(size_t bitrate_kbps, size_t framerate_fps) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  config_.codec_settings.maxFramerate = static_cast<uint32_t>(framerate_fps);
  bitrate_allocation_ = bitrate_allocator_->GetAllocation(
      static_cast<uint32_t>(bitrate_kbps * 1000),
      static_cast<uint32_t>(framerate_fps));
  const int set_rates_result = encoder_->SetRateAllocation(
      bitrate_allocation_, static_cast<uint32_t>(framerate_fps));
  RTC_DCHECK_GE(set_rates_result, 0)
      << "Failed to update encoder with new rate " << bitrate_kbps << ".";
}

void VideoProcessor::FrameEncoded(
    const webrtc::EncodedImage& encoded_image,
    const webrtc::CodecSpecificInfo& codec_specific) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  // For the highest measurement accuracy of the encode time, the start/stop
  // time recordings should wrap the Encode call as tightly as possible.
  int64_t encode_stop_ns = rtc::TimeNanos();

  const VideoCodecType codec = codec_specific.codecType;
  if (config_.encoded_frame_checker) {
    config_.encoded_frame_checker->CheckEncodedFrame(codec, encoded_image);
  }

  size_t simulcast_svc_idx = 0;
  size_t temporal_idx = 0;

  if (codec == kVideoCodecVP8) {
    simulcast_svc_idx = codec_specific.codecSpecific.VP8.simulcastIdx;
    temporal_idx = codec_specific.codecSpecific.VP8.temporalIdx;
  } else if (codec == kVideoCodecVP9) {
    simulcast_svc_idx = codec_specific.codecSpecific.VP9.spatial_idx;
    temporal_idx = codec_specific.codecSpecific.VP9.temporal_idx;
  }

  if (simulcast_svc_idx == kNoSpatialIdx) {
    simulcast_svc_idx = 0;
  }

  if (temporal_idx == kNoTemporalIdx) {
    temporal_idx = 0;
  }

  const size_t frame_wxh =
      encoded_image._encodedWidth * encoded_image._encodedHeight;
  frame_wxh_to_simulcast_svc_idx_[frame_wxh] = simulcast_svc_idx;

  FrameStatistic* frame_stat =
      stats_->at(simulcast_svc_idx)
          .GetFrameWithTimestamp(encoded_image._timeStamp);
  const size_t frame_number = frame_stat->frame_number;

  // Reordering is unexpected. Frames of different layers have the same value
  // of frame_number. VP8 multi-res delivers frames starting from hires layer.
  RTC_CHECK_GE(frame_number, last_encoded_frame_num_);

  // Ensure SVC spatial layers are delivered in ascending order.
  if (config_.NumberOfSpatialLayers() > 1) {
    RTC_CHECK(simulcast_svc_idx > last_encoded_simulcast_svc_idx_ ||
              frame_number != last_encoded_frame_num_ ||
              num_encoded_frames_ == 0);
  }

  last_encoded_frame_num_ = frame_number;
  last_encoded_simulcast_svc_idx_ = simulcast_svc_idx;

  // Update frame statistics.
  frame_stat->encoding_successful = true;
  frame_stat->encode_time_us =
      GetElapsedTimeMicroseconds(frame_stat->encode_start_ns, encode_stop_ns);

  // TODO(ssilkin): Implement bitrate allocation for VP9 SVC. For now set
  // target for base layers equal to total target to avoid devision by zero
  // at analysis.
  frame_stat->target_bitrate_kbps =
      bitrate_allocation_.GetSpatialLayerSum(
          codec == kVideoCodecVP9 ? 0 : simulcast_svc_idx) /
      1000;
  frame_stat->encoded_frame_size_bytes = encoded_image._length;
  frame_stat->frame_type = encoded_image._frameType;
  frame_stat->temporal_layer_idx = temporal_idx;
  frame_stat->simulcast_svc_idx = simulcast_svc_idx;
  frame_stat->max_nalu_size_bytes = GetMaxNaluSizeBytes(encoded_image, config_);
  frame_stat->qp = encoded_image.qp_;

  if (!config_.IsAsyncCodec()) {
    // Store encoded frame. It will be decoded after all layers are encoded.
    CopyEncodedImage(encoded_image, codec, frame_number, simulcast_svc_idx);
  } else {
    const size_t simulcast_idx =
        codec == kVideoCodecVP8 ? codec_specific.codecSpecific.VP8.simulcastIdx
                                : 0;
    frame_stat->decode_start_ns = rtc::TimeNanos();
    frame_stat->decode_return_code =
        decoders_->at(simulcast_idx)->Decode(encoded_image, false, nullptr);
  }

  ++num_encoded_frames_;
}

void VideoProcessor::FrameDecoded(const VideoFrame& decoded_frame) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  // For the highest measurement accuracy of the decode time, the start/stop
  // time recordings should wrap the Decode call as tightly as possible.
  int64_t decode_stop_ns = rtc::TimeNanos();

  RTC_CHECK(frame_wxh_to_simulcast_svc_idx_.find(decoded_frame.size()) !=
            frame_wxh_to_simulcast_svc_idx_.end());
  const size_t simulcast_svc_idx =
      frame_wxh_to_simulcast_svc_idx_[decoded_frame.size()];

  FrameStatistic* frame_stat =
      stats_->at(simulcast_svc_idx)
          .GetFrameWithTimestamp(decoded_frame.timestamp());
  const size_t frame_number = frame_stat->frame_number;

  // Reordering is unexpected. Frames of different layers have the same value
  // of frame_number.
  RTC_CHECK_GE(frame_number, last_decoded_frame_num_);

  if (decoded_frame_writers_ && num_decoded_frames_ > 0) {
    // For dropped frames, write out the last decoded frame to make it look like
    // a freeze at playback.
    for (size_t num_dropped_frames = 0; num_dropped_frames < frame_number;
         ++num_dropped_frames) {
      const FrameStatistic* prev_frame_stat =
          stats_->at(simulcast_svc_idx)
              .GetFrame(frame_number - num_dropped_frames - 1);
      if (prev_frame_stat->decoding_successful) {
        break;
      }
      WriteDecodedFrameToFile(&last_decoded_frame_buffers_[simulcast_svc_idx],
                              simulcast_svc_idx);
    }
  }

  last_decoded_frame_num_ = frame_number;

  // Update frame statistics.
  frame_stat->decoding_successful = true;
  frame_stat->decode_time_us =
      GetElapsedTimeMicroseconds(frame_stat->decode_start_ns, decode_stop_ns);
  frame_stat->decoded_width = decoded_frame.width();
  frame_stat->decoded_height = decoded_frame.height();

  // Skip quality metrics calculation to not affect CPU usage.
  if (!config_.measure_cpu) {
    CalculateFrameQuality(*input_frames_[frame_number], decoded_frame,
                          frame_stat);
  }

  // Delay erasing of input frames by one frame. The current frame might
  // still be needed for other simulcast stream or spatial layer.
  if (frame_number > 0) {
    auto input_frame_erase_to = input_frames_.lower_bound(frame_number - 1);
    input_frames_.erase(input_frames_.begin(), input_frame_erase_to);
  }

  if (decoded_frame_writers_) {
    ExtractBufferWithSize(decoded_frame, config_.codec_settings.width,
                          config_.codec_settings.height,
                          &last_decoded_frame_buffers_[simulcast_svc_idx]);
    WriteDecodedFrameToFile(&last_decoded_frame_buffers_[simulcast_svc_idx],
                            simulcast_svc_idx);
  }

  ++num_decoded_frames_;
}

void VideoProcessor::CopyEncodedImage(const EncodedImage& encoded_image,
                                      const VideoCodecType codec,
                                      size_t frame_number,
                                      size_t simulcast_svc_idx) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  EncodedImage base_image;
  RTC_CHECK_EQ(base_image._length, 0);

  // Each SVC layer is decoded with dedicated decoder. Add data of base layers
  // to current coded frame buffer.
  if (config_.NumberOfSpatialLayers() > 1 && simulcast_svc_idx > 0) {
    RTC_CHECK(last_encoded_frames_.find(simulcast_svc_idx - 1) !=
              last_encoded_frames_.end());
    base_image = last_encoded_frames_[simulcast_svc_idx - 1];
  }

  const size_t payload_size_bytes = base_image._length + encoded_image._length;
  const size_t buffer_size_bytes =
      payload_size_bytes + EncodedImage::GetBufferPaddingBytes(codec);

  uint8_t* copied_buffer = new uint8_t[buffer_size_bytes];
  RTC_CHECK(copied_buffer);

  if (base_image._length) {
    memcpy(copied_buffer, base_image._buffer, base_image._length);
  }

  memcpy(copied_buffer + base_image._length, encoded_image._buffer,
         encoded_image._length);

  EncodedImage copied_image = encoded_image;
  copied_image = encoded_image;
  copied_image._buffer = copied_buffer;
  copied_image._length = payload_size_bytes;
  copied_image._size = buffer_size_bytes;

  last_encoded_frames_[simulcast_svc_idx] = copied_image;
}

void VideoProcessor::CalculateFrameQuality(const VideoFrame& ref_frame,
                                           const VideoFrame& dec_frame,
                                           FrameStatistic* frame_stat) {
  if (ref_frame.width() == dec_frame.width() ||
      ref_frame.height() == dec_frame.height()) {
    frame_stat->psnr = I420PSNR(&ref_frame, &dec_frame);
    frame_stat->ssim = I420SSIM(&ref_frame, &dec_frame);
  } else {
    RTC_CHECK_GE(ref_frame.width(), dec_frame.width());
    RTC_CHECK_GE(ref_frame.height(), dec_frame.height());
    // Downscale reference frame. Use bilinear interpolation since it is used
    // to get lowres inputs for encoder at simulcasting.
    // TODO(ssilkin): Sync with VP9 SVC which uses 8-taps polyphase.
    rtc::scoped_refptr<I420Buffer> scaled_buffer =
        I420Buffer::Create(dec_frame.width(), dec_frame.height());
    const I420BufferInterface& ref_buffer =
        *ref_frame.video_frame_buffer()->ToI420();
    I420Scale(ref_buffer.DataY(), ref_buffer.StrideY(), ref_buffer.DataU(),
              ref_buffer.StrideU(), ref_buffer.DataV(), ref_buffer.StrideV(),
              ref_buffer.width(), ref_buffer.height(),
              scaled_buffer->MutableDataY(), scaled_buffer->StrideY(),
              scaled_buffer->MutableDataU(), scaled_buffer->StrideU(),
              scaled_buffer->MutableDataV(), scaled_buffer->StrideV(),
              scaled_buffer->width(), scaled_buffer->height(),
              libyuv::kFilterBilinear);
    frame_stat->psnr =
        I420PSNR(*scaled_buffer, *dec_frame.video_frame_buffer()->ToI420());
    frame_stat->ssim =
        I420SSIM(*scaled_buffer, *dec_frame.video_frame_buffer()->ToI420());
  }
}

void VideoProcessor::WriteDecodedFrameToFile(rtc::Buffer* buffer,
                                             size_t simulcast_svc_idx) {
  RTC_CHECK(simulcast_svc_idx < decoded_frame_writers_->size());
  RTC_DCHECK_EQ(buffer->size(),
                decoded_frame_writers_->at(simulcast_svc_idx)->FrameLength());
  RTC_CHECK(decoded_frame_writers_->at(simulcast_svc_idx)
                ->WriteFrame(buffer->data()));
}

}  // namespace test
}  // namespace webrtc
