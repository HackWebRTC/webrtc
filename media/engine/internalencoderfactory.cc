/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/internalencoderfactory.h"

#include <utility>

#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"

namespace cricket {

InternalEncoderFactory::InternalEncoderFactory() {
  supported_codecs_.push_back(VideoCodec(kVp8CodecName));
  if (webrtc::VP9Encoder::IsSupported())
    supported_codecs_.push_back(VideoCodec(kVp9CodecName));

  for (const webrtc::SdpVideoFormat& format : webrtc::SupportedH264Codecs())
    supported_codecs_.push_back(VideoCodec(format));
}

InternalEncoderFactory::~InternalEncoderFactory() {}

// WebRtcVideoEncoderFactory implementation.
webrtc::VideoEncoder* InternalEncoderFactory::CreateVideoEncoder(
    const VideoCodec& codec) {
  const webrtc::VideoCodecType codec_type =
      webrtc::PayloadStringToCodecType(codec.name);
  switch (codec_type) {
    case webrtc::kVideoCodecH264:
      return webrtc::H264Encoder::Create(codec).release();
    case webrtc::kVideoCodecVP8:
      return webrtc::VP8Encoder::Create().release();
    case webrtc::kVideoCodecVP9:
      return webrtc::VP9Encoder::Create().release();
    default:
      return nullptr;
  }
}

const std::vector<VideoCodec>&
InternalEncoderFactory::supported_codecs() const {
  return supported_codecs_;
}

void InternalEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  delete encoder;
}

}  // namespace cricket
