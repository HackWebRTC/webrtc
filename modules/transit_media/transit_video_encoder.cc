#include "modules/transit_media/transit_video_encoder.h"

#include "api/video/encoded_image.h"
#include "common_video/h264/h264_common.h"
#include "modules/transit_media/transit_video_frame_buffer.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/codecs/interface/common_constants.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

#define DEBUG_LOG 0

namespace webrtc {

TransitVideoEncoder::TransitVideoEncoder() : callback_(nullptr), got_key_(false) {
  RTC_LOG(LS_INFO) << "create TransitVideoEncoder";
}

VideoEncoder::EncoderInfo TransitVideoEncoder::GetEncoderInfo() const {
  VideoEncoder::EncoderInfo info;
  info.supports_native_handle = true;
  info.implementation_name = "TransitVideoEncoder";
  return info;
}

int32_t TransitVideoEncoder::Encode(
    const VideoFrame& frame,
    const std::vector<VideoFrameType>* frame_types) {
#if DEBUG_LOG
  RTC_LOG(LS_INFO) << "TransitVideoEncoder::Encode frame ts " << frame.render_time_ms()
        << ", buffer type " << frame.video_frame_buffer()->type()
        << ", got_key_ " << got_key_ << ", callback_ " << (void*)callback_;
#endif
  if (!callback_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  rtc::scoped_refptr<VideoFrameBuffer> buffer = frame.video_frame_buffer();
  if (buffer->type() != VideoFrameBuffer::Type::kNative) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  TransitVideoFrameBuffer* transit_buffer =
      reinterpret_cast<TransitVideoFrameBuffer*>(buffer.get());

  rtc::ArrayView<const uint8_t> array_view(transit_buffer->data(), transit_buffer->size());
  std::vector<webrtc::H264::NaluIndex> indices = webrtc::H264::FindNaluIndices(array_view);
  bool key_frame = false;
  for (size_t i = 0; i < indices.size(); i++) {
    H264::NaluType type = H264::ParseNaluType(
        transit_buffer->data()[indices[i].payload_start_offset]);
    if (type == H264::NaluType::kSps || type == H264::NaluType::kPps) {
      key_frame = true;
    }
  }

  if (key_frame) {
    got_key_ = true;
  }
  if (!got_key_ && !key_frame) {
    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  }

  EncodedImage encoded_image;
  encoded_image.SetEncodedData(EncodedImageBuffer::Create(
    transit_buffer->mutable_data(), transit_buffer->size()));

  encoded_image._encodedWidth = transit_buffer->width();
  encoded_image._encodedHeight = transit_buffer->height();
  encoded_image.SetRtpTimestamp(frame.rtp_timestamp());
  encoded_image._frameType = key_frame ? VideoFrameType::kVideoFrameKey
                                       : VideoFrameType::kVideoFrameDelta;
  encoded_image.rotation_ = webrtc::VideoRotation(0);
  encoded_image.capture_time_ms_ = frame.render_time_ms();
  encoded_image.timing_.flags = webrtc::VideoSendTiming::kInvalid;
  //encoded_image._completeFrame = true;

  // Parse QP.
  h264_bitstream_parser_.ParseBitstream(encoded_image);
  encoded_image.qp_ = h264_bitstream_parser_.GetLastSliceQp().value_or(-1);
  encoded_image.content_type_ = webrtc::VideoContentType::UNSPECIFIED;

#if DEBUG_LOG
  RTC_LOG(LS_INFO) << "TransitVideoEncoder::Encode frame ts " << frame.render_time_ms()
        << ", _frameType " << encoded_image._frameType
        << ", key_frame " << key_frame
        << ", qp " << encoded_image.qp_;
#endif

  CodecSpecificInfo codec_specific;
  codec_specific.codecType = kVideoCodecH264;
  codec_specific.codecSpecific.H264.packetization_mode =
      H264PacketizationMode::NonInterleaved;

  callback_->OnEncodedImage(encoded_image, &codec_specific);
  return WEBRTC_VIDEO_CODEC_OK;
}

}  // namespace webrtc
