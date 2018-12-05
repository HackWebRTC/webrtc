/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder_factory.h"

// TODO(webrtc:10065) Remove once downstream projects have been updated.
namespace webrtc {

VideoEncoderFactory::CodecInfo::CodecInfo()
    : is_hardware_accelerated(false), has_internal_source(false) {}

VideoEncoderFactory::CodecInfo::~CodecInfo() = default;

VideoEncoderFactory::CodecInfo VideoEncoderFactory::QueryVideoEncoder(
    const SdpVideoFormat& format) const {
  return CodecInfo();
}

}  // namespace webrtc
