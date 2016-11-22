/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_decoder.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"

namespace webrtc {
VideoDecoder* VideoDecoder::Create(VideoDecoder::DecoderType codec_type) {
  switch (codec_type) {
    case kH264:
      if (!H264Decoder::IsSupported()) {
        // This could happen in a software-fallback for a codec type only
        // supported externally (e.g. H.264 on iOS or Android) or in current
        // usage in WebRtcVideoEngine2 if the external decoder fails to be
        // created.
        LOG(LS_ERROR) << "Unable to create an H.264 decoder fallback. "
                      << "Decoding of this stream will be broken.";
        return new NullVideoDecoder();
      }
      return H264Decoder::Create();
    case kVp8:
      return VP8Decoder::Create();
    case kVp9:
      RTC_DCHECK(VP9Decoder::IsSupported());
      return VP9Decoder::Create();
    case kUnsupportedCodec:
      LOG(LS_ERROR) << "Creating NullVideoDecoder for unsupported codec.";
      return new NullVideoDecoder();
  }
  RTC_NOTREACHED();
  return nullptr;
}

NullVideoDecoder::NullVideoDecoder() {}

int32_t NullVideoDecoder::InitDecode(const VideoCodec* codec_settings,
                                     int32_t number_of_cores) {
  LOG(LS_ERROR) << "Can't initialize NullVideoDecoder.";
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t NullVideoDecoder::Decode(const EncodedImage& input_image,
    bool missing_frames,
    const RTPFragmentationHeader* fragmentation,
    const CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  LOG(LS_ERROR) << "The NullVideoDecoder doesn't support decoding.";
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t NullVideoDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  LOG(LS_ERROR)
      << "Can't register decode complete callback on NullVideoDecoder.";
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t NullVideoDecoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}

const char* NullVideoDecoder::ImplementationName() const {
  return "NullVideoDecoder";
}

}  // namespace webrtc
