/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_stream_decoder_impl.h"

#include "rtc_base/ptr_util.h"

namespace webrtc {
VideoStreamDecoderImpl::VideoStreamDecoderImpl(
    VideoStreamDecoder::Callbacks* callbacks,
    VideoDecoderFactory* decoder_factory,
    std::map<int, std::pair<SdpVideoFormat, int>> decoder_settings)
    : callbacks_(callbacks),
      decoder_factory_(decoder_factory),
      decoder_settings_(std::move(decoder_settings)) {}

VideoStreamDecoderImpl::~VideoStreamDecoderImpl() {}

void VideoStreamDecoderImpl::OnFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {}
}  // namespace webrtc
