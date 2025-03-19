#include "modules/transit_media/transit_video_decoder.h"

#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "common_video/h264/h264_common.h"
#include "modules/transit_media/transit_video_frame_buffer.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/codecs/interface/common_constants.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

bool TransitVideoDecoder::Configure(const Settings& settings) {
  // TODO: open dump file
  // dump_ = fopen("path", "wb");
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t TransitVideoDecoder::Decode(const EncodedImage& input_image,
                                    int64_t render_time_ms) {
  if (dump_) {
    fwrite(input_image.data(), 1, input_image.size(), dump_);
  }

  h264_bitstream_parser_.ParseBitstream(input_image);
  int qp = h264_bitstream_parser_.GetLastSliceQp().value_or(-1);

  VideoFrame black_frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(GetBlackFrameBuffer(
              h264_bitstream_parser_.width(), h264_bitstream_parser_.height()))
          .set_rotation(VideoRotation::kVideoRotation_0)
          .set_timestamp_rtp(input_image.RtpTimestamp())
          .build();

  callback_->Decoded(
      black_frame, std::nullopt,
      qp >= 0 ? std::optional<uint8_t>((uint8_t)qp) : std::nullopt);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t TransitVideoDecoder::Release() {
  if (dump_) {
    fclose(dump_);
    dump_ = nullptr;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

const rtc::scoped_refptr<webrtc::VideoFrameBuffer>&
TransitVideoDecoder::GetBlackFrameBuffer(int width, int height) {
  if (!black_frame_buffer_ || black_frame_buffer_->width() != width ||
      black_frame_buffer_->height() != height) {
    rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(width, height);
    webrtc::I420Buffer::SetBlack(buffer.get());
    black_frame_buffer_ = buffer;
  }

  return black_frame_buffer_;
}

}  // namespace webrtc
