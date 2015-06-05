/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_encoder.h"

#include "webrtc/base/checks.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {
VideoEncoder* VideoEncoder::Create(VideoEncoder::EncoderType codec_type) {
  switch (codec_type) {
    case kVp8:
      return VP8Encoder::Create();
    case kVp9:
      return VP9Encoder::Create();
    case kUnsupportedCodec:
      RTC_NOTREACHED();
      return nullptr;
  }
  RTC_NOTREACHED();
  return nullptr;
}

VideoEncoder::EncoderType CodecToEncoderType(VideoCodecType codec_type) {
  switch (codec_type) {
    case kVideoCodecVP8:
      return VideoEncoder::kVp8;
    case kVideoCodecVP9:
      return VideoEncoder::kVp9;
    default:
      return VideoEncoder::kUnsupportedCodec;
  }
}

VideoEncoderSoftwareFallbackWrapper::VideoEncoderSoftwareFallbackWrapper(
    VideoCodecType codec_type,
    webrtc::VideoEncoder* encoder)
    : encoder_type_(CodecToEncoderType(codec_type)),
      encoder_(encoder),
      callback_(nullptr) {
}

int32_t VideoEncoderSoftwareFallbackWrapper::InitEncode(
    const VideoCodec* codec_settings,
    int32_t number_of_cores,
    size_t max_payload_size) {
  int32_t ret =
      encoder_->InitEncode(codec_settings, number_of_cores, max_payload_size);
  if (ret == WEBRTC_VIDEO_CODEC_OK || encoder_type_ == kUnsupportedCodec) {
    fallback_encoder_.reset();
    if (callback_)
      encoder_->RegisterEncodeCompleteCallback(callback_);
    return ret;
  }
  // Try to instantiate software codec.
  fallback_encoder_.reset(VideoEncoder::Create(encoder_type_));
  if (fallback_encoder_->InitEncode(codec_settings, number_of_cores,
                                    max_payload_size) ==
      WEBRTC_VIDEO_CODEC_OK) {
    if (callback_)
      fallback_encoder_->RegisterEncodeCompleteCallback(callback_);
    return WEBRTC_VIDEO_CODEC_OK;
  }
  // Software encoder failed, reset and use original return code.
  fallback_encoder_.reset();
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  int32_t ret = encoder_->RegisterEncodeCompleteCallback(callback);
  if (fallback_encoder_)
    return fallback_encoder_->RegisterEncodeCompleteCallback(callback);
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::Release() {
  if (fallback_encoder_)
    return fallback_encoder_->Release();
  return encoder_->Release();
}

int32_t VideoEncoderSoftwareFallbackWrapper::Encode(
    const VideoFrame& frame,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<VideoFrameType>* frame_types) {
  if (fallback_encoder_)
    return fallback_encoder_->Encode(frame, codec_specific_info, frame_types);
  return encoder_->Encode(frame, codec_specific_info, frame_types);
}

int32_t VideoEncoderSoftwareFallbackWrapper::SetChannelParameters(
    uint32_t packet_loss,
    int64_t rtt) {
  int32_t ret = encoder_->SetChannelParameters(packet_loss, rtt);
  if (fallback_encoder_)
    return fallback_encoder_->SetChannelParameters(packet_loss, rtt);
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::SetRates(uint32_t bitrate,
                                                      uint32_t framerate) {
  int32_t ret = encoder_->SetRates(bitrate, framerate);
  if (fallback_encoder_)
    return fallback_encoder_->SetRates(bitrate, framerate);
  return ret;
}

void VideoEncoderSoftwareFallbackWrapper::OnDroppedFrame() {
  if (fallback_encoder_)
    return fallback_encoder_->OnDroppedFrame();
  return encoder_->OnDroppedFrame();
}

bool VideoEncoderSoftwareFallbackWrapper::SupportsNativeHandle() const {
  if (fallback_encoder_)
    return fallback_encoder_->SupportsNativeHandle();
  return encoder_->SupportsNativeHandle();
}

}  // namespace webrtc
