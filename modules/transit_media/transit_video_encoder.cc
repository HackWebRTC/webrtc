#include "modules/transit_media/transit_video_encoder.h"

#include "api/video/encoded_image.h"
#include "common_video/h264/h264_common.h"
#include "modules/transit_media/transit_video_frame_buffer.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/codecs/interface/common_constants.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

VideoEncoder::EncoderInfo TransitVideoEncoder::GetEncoderInfo() const {
  VideoEncoder::EncoderInfo info;
  info.supports_native_handle = true;
  info.implementation_name = "TransitVideoEncoder";
  return info;
}

int32_t TransitVideoEncoder::Encode(
    const VideoFrame& frame,
    const std::vector<VideoFrameType>* frame_types) {
  if (!callback_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  rtc::scoped_refptr<VideoFrameBuffer> buffer = frame.video_frame_buffer();
  if (buffer->type() != VideoFrameBuffer::Type::kNative) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  TransitVideoFrameBuffer* transit_buffer =
      reinterpret_cast<TransitVideoFrameBuffer*>(buffer.get());

  std::vector<webrtc::H264::NaluIndex> indices = webrtc::H264::FindNaluIndices(
      transit_buffer->data(), transit_buffer->size());
  RTPFragmentationHeader frag_header;
  frag_header.VerifyAndAllocateFragmentationHeader(indices.size());
  bool key_frame = false;
  for (size_t i = 0; i < indices.size(); i++) {
    frag_header.fragmentationOffset[i] = indices[i].payload_start_offset;
    frag_header.fragmentationLength[i] = indices[i].payload_size;
    H264::NaluType type = H264::ParseNaluType(
        transit_buffer->data()[indices[i].payload_start_offset]);
    if (type == H264::NaluType::kSps || type == H264::NaluType::kPps) {
      key_frame = true;
    }
  }
  // RTC_LOG(LS_INFO) << "TransitVideoEncoder::Encode frame ts " <<
  // frame.timestamp_us() << ", key_frame " << key_frame;
  if (key_frame) {
    got_key_ = true;
  }
  if (!got_key_ && !key_frame) {
    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  }

  EncodedImage encoded_image(transit_buffer->mutable_data(),
                             transit_buffer->size(), transit_buffer->size());

  encoded_image._encodedWidth = transit_buffer->width();
  encoded_image._encodedHeight = transit_buffer->height();
  encoded_image.SetTimestamp(frame.timestamp());
  encoded_image._frameType = key_frame ? VideoFrameType::kVideoFrameKey
                                       : VideoFrameType::kVideoFrameDelta;
  encoded_image.rotation_ = webrtc::VideoRotation(0);
  encoded_image.capture_time_ms_ =
      frame.timestamp_us() / rtc::kNumMicrosecsPerMillisec;
  encoded_image.timing_.flags = webrtc::VideoSendTiming::kInvalid;
  encoded_image._completeFrame = true;

  // Parse QP.
  h264_bitstream_parser_.ParseBitstream(encoded_image.data(),
                                        encoded_image.size());
  h264_bitstream_parser_.GetLastSliceQp(&encoded_image.qp_);
  encoded_image.content_type_ = webrtc::VideoContentType::UNSPECIFIED;

  CodecSpecificInfo codec_specific;
  codec_specific.codecType = kVideoCodecH264;
  codec_specific.codecSpecific.H264.packetization_mode =
      H264PacketizationMode::NonInterleaved;

  callback_->OnEncodedImage(encoded_image, &codec_specific, &frag_header);
  return WEBRTC_VIDEO_CODEC_OK;
}

}  // namespace webrtc
