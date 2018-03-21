/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_STREAM_DECODER_IMPL_H_
#define VIDEO_VIDEO_STREAM_DECODER_IMPL_H_

#include <functional>
#include <map>
#include <memory>
#include <utility>

#include "api/optional.h"
#include "api/video/encoded_frame.h"
#include "api/video/video_frame.h"
#include "api/video/video_stream_decoder.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"

namespace webrtc {

class VideoStreamDecoderImpl : public VideoStreamDecoder,
                               private DecodedImageCallback {
 public:
  VideoStreamDecoderImpl(
      VideoStreamDecoder::Callbacks* callbacks,
      VideoDecoderFactory* decoder_factory,
      std::map<int, std::pair<SdpVideoFormat, int>> decoder_settings);

  ~VideoStreamDecoderImpl() override;

  void OnFrame(std::unique_ptr<video_coding::EncodedFrame> frame) override;

 private:
  // Implements DecodedImageCallback interface
  int32_t Decoded(VideoFrame& decodedImage) override;
  int32_t Decoded(VideoFrame& decodedImage, int64_t decode_time_ms) override;
  void Decoded(VideoFrame& decodedImage,
               rtc::Optional<int32_t> decode_time_ms,
               rtc::Optional<uint8_t> qp) override;

  VideoStreamDecoder::Callbacks* callbacks_;
  VideoDecoderFactory* decoder_factory_;
  std::map<int, std::pair<SdpVideoFormat, int>> decoder_settings_;
};

}  // namespace webrtc

#endif  // VIDEO_VIDEO_STREAM_DECODER_IMPL_H_
