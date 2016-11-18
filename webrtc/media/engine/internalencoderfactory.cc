/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/internalencoderfactory.h"

#include <utility>

#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"

namespace cricket {

InternalEncoderFactory::InternalEncoderFactory() {
  supported_codecs_.push_back(cricket::VideoCodec(kVp8CodecName));
  if (webrtc::VP9Encoder::IsSupported())
    supported_codecs_.push_back(cricket::VideoCodec(kVp9CodecName));
  if (webrtc::H264Encoder::IsSupported()) {
    cricket::VideoCodec codec(kH264CodecName);
    // TODO(magjed): Move setting these parameters into webrtc::H264Encoder
    // instead.
    // TODO(hta): Set FMTP parameters for all codecs of type H264.
    codec.SetParam(kH264FmtpProfileLevelId,
                   kH264ProfileLevelConstrainedBaseline);
    codec.SetParam(kH264FmtpLevelAsymmetryAllowed, "1");
    codec.SetParam(kH264FmtpPacketizationMode, "1");
    supported_codecs_.push_back(std::move(codec));
  }

  supported_codecs_.push_back(cricket::VideoCodec(kRedCodecName));
  supported_codecs_.push_back(cricket::VideoCodec(kUlpfecCodecName));
}

InternalEncoderFactory::~InternalEncoderFactory() {}

// WebRtcVideoEncoderFactory implementation.
webrtc::VideoEncoder* InternalEncoderFactory::CreateVideoEncoder(
    const cricket::VideoCodec& codec) {
  switch (CodecTypeFromName(codec.name)) {
    case webrtc::kVideoCodecH264:
      return webrtc::H264Encoder::Create();
    case webrtc::kVideoCodecVP8:
      return webrtc::VP8Encoder::Create();
    case webrtc::kVideoCodecVP9:
      return webrtc::VP9Encoder::Create();
    default:
      return nullptr;
  }
}

const std::vector<cricket::VideoCodec>&
InternalEncoderFactory::supported_codecs() const {
  return supported_codecs_;
}

void InternalEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  delete encoder;
}

}  // namespace cricket
