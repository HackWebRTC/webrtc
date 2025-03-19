
#pragma once

#include <cstdio>

#include "api/scoped_refptr.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/transit_media/exposed_h264_bitstream_parser.h"
#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

class TransitVideoDecoder : public VideoDecoder {
 public:
  TransitVideoDecoder() : dump_(nullptr), callback_(nullptr) {}

  bool Configure(const Settings& settings) override;

  int32_t Decode(const EncodedImage& input_image,
                 int64_t render_time_ms = -1) override;

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override {
    callback_ = callback;

    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override;

  const char* ImplementationName() const override {
    return "TransitVideoDecoder";
  }

 private:
  const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& GetBlackFrameBuffer(
      int width,
      int height);

  FILE* dump_;
  DecodedImageCallback* callback_;

  ExposedH264BitstreamParser h264_bitstream_parser_;
  rtc::scoped_refptr<VideoFrameBuffer> black_frame_buffer_;
};

}  // namespace webrtc
