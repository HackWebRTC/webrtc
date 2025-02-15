#pragma once

#include "api/video_codecs/video_encoder.h"
#include "common_video/h264/h264_bitstream_parser.h"
#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

class TransitVideoEncoder : public VideoEncoder {
 public:
  TransitVideoEncoder() : callback_(nullptr), got_key_(false) {}
  ~TransitVideoEncoder() {}

  int32_t InitEncode(const VideoCodec* codec_settings,
                     const VideoEncoder::Settings& settings) override {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  VideoEncoder::EncoderInfo GetEncoderInfo() const override;

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override {
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override { return WEBRTC_VIDEO_CODEC_OK; }

  int32_t Encode(const VideoFrame& frame,
                 const std::vector<VideoFrameType>* frame_types) override;

  void SetRates(const RateControlParameters& parameters) override {}

 private:
  EncodedImageCallback* callback_;
  bool got_key_;

  webrtc::H264BitstreamParser h264_bitstream_parser_;
};

}  // namespace webrtc
