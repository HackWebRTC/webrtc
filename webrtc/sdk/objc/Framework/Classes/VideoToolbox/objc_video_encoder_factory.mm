/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/objc/Framework/Classes/VideoToolbox/objc_video_encoder_factory.h"

#include <string>

#import "NSString+StdString.h"
#import "RTCI420Buffer+Private.h"
#import "RTCVideoCodec+Private.h"
#import "WebRTC/RTCVideoCodec.h"
#import "WebRTC/RTCVideoCodecFactory.h"
#import "WebRTC/RTCVideoCodecH264.h"
#import "WebRTC/RTCVideoFrame.h"
#import "WebRTC/RTCVideoFrameBuffer.h"

#include "webrtc/api/video/video_frame.h"
#include "webrtc/api/video_codecs/video_encoder.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_error_codes.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/timeutils.h"
#include "webrtc/sdk/objc/Framework/Classes/Common/helpers.h"
#include "webrtc/sdk/objc/Framework/Classes/Video/objc_frame_buffer.h"

namespace webrtc {

namespace {

id<RTCVideoFrameBuffer> nativeToRtcFrameBuffer(const rtc::scoped_refptr<VideoFrameBuffer> &buffer) {
  return buffer->type() == VideoFrameBuffer::Type::kNative ?
      static_cast<ObjCFrameBuffer *>(buffer.get())->wrapped_frame_buffer() :
      [[RTCI420Buffer alloc] initWithFrameBuffer:buffer->ToI420()];
}

RTCVideoFrame *nativeToRtcFrame(const VideoFrame &frame) {
  RTCVideoFrame *rtcFrame =
      [[RTCVideoFrame alloc] initWithBuffer:nativeToRtcFrameBuffer(frame.video_frame_buffer())
                                   rotation:RTCVideoRotation(frame.rotation())
                                timeStampNs:frame.timestamp_us() * rtc::kNumNanosecsPerMicrosec];
  rtcFrame.timeStamp = frame.timestamp();
  return rtcFrame;
}

class ObjCVideoEncoder : public VideoEncoder {
 public:
  ObjCVideoEncoder(id<RTCVideoEncoder> encoder)
      : encoder_(encoder), implementation_name_([encoder implementationName].stdString) {}

  int32_t InitEncode(const VideoCodec *codec_settings,
                     int32_t number_of_cores,
                     size_t max_payload_size) {
    RTCVideoEncoderSettings *settings =
        [[RTCVideoEncoderSettings alloc] initWithNativeVideoCodec:codec_settings];
    return [encoder_ startEncodeWithSettings:settings numberOfCores:number_of_cores];
  }

  int32_t RegisterEncodeCompleteCallback(EncodedImageCallback *callback) {
    [encoder_ setCallback:^BOOL(RTCEncodedImage *_Nonnull frame,
                                id<RTCCodecSpecificInfo> _Nonnull info,
                                RTCRtpFragmentationHeader *_Nonnull header) {
      EncodedImage encodedImage = [frame nativeEncodedImage];

      // Handle types than can be converted into one of CodecSpecificInfo's hard coded cases.
      CodecSpecificInfo codecSpecificInfo;
      if ([info isKindOfClass:[RTCCodecSpecificInfoH264 class]]) {
        codecSpecificInfo = [(RTCCodecSpecificInfoH264 *)info nativeCodecSpecificInfo];
      }

      std::unique_ptr<RTPFragmentationHeader> fragmentationHeader =
          [header createNativeFragmentationHeader];
      EncodedImageCallback::Result res =
          callback->OnEncodedImage(encodedImage, &codecSpecificInfo, fragmentationHeader.release());
      return res.error == EncodedImageCallback::Result::OK;
    }];

    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() { return [encoder_ releaseEncoder]; }

  int32_t Encode(const VideoFrame &frame,
                 const CodecSpecificInfo *codec_specific_info,
                 const std::vector<FrameType> *frame_types) {
    // CodecSpecificInfo only handles a hard coded list of codecs
    id<RTCCodecSpecificInfo> rtcCodecSpecificInfo = nil;
    if (codec_specific_info) {
      if (strcmp(codec_specific_info->codec_name, "H264") == 0) {
        RTCCodecSpecificInfoH264 *h264Info = [[RTCCodecSpecificInfoH264 alloc] init];
        h264Info.packetizationMode =
            (RTCH264PacketizationMode)codec_specific_info->codecSpecific.H264.packetization_mode;
        rtcCodecSpecificInfo = h264Info;
      }
    }

    NSMutableArray<NSNumber *> *rtcFrameTypes = [NSMutableArray array];
    for (size_t i = 0; i < frame_types->size(); ++i) {
      [rtcFrameTypes addObject:@(RTCFrameType(frame_types->at(i)))];
    }

    return [encoder_ encode:nativeToRtcFrame(frame)
          codecSpecificInfo:rtcCodecSpecificInfo
                 frameTypes:rtcFrameTypes];
  }

  int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) { return WEBRTC_VIDEO_CODEC_OK; }

  int32_t SetRates(uint32_t bitrate, uint32_t framerate) {
    return [encoder_ setBitrate:bitrate framerate:framerate];
  }

  bool SupportsNativeHandle() const { return true; }

  VideoEncoder::ScalingSettings GetScalingSettings() const {
    RTCVideoEncoderQpThresholds* qp_thresholds = [encoder_ scalingSettings];
    return qp_thresholds ?
        ScalingSettings(true /* enabled */, qp_thresholds.low, qp_thresholds.high) :
        ScalingSettings(false /* enabled */);
  }

  const char *ImplementationName() const { return implementation_name_.c_str(); }

 private:
  id<RTCVideoEncoder> encoder_;
  const std::string implementation_name_;
};
}  // namespace

ObjCVideoEncoderFactory::ObjCVideoEncoderFactory(id<RTCVideoEncoderFactory> encoder_factory)
    : encoder_factory_(encoder_factory) {}

ObjCVideoEncoderFactory::~ObjCVideoEncoderFactory() {}

id<RTCVideoEncoderFactory> ObjCVideoEncoderFactory::wrapped_encoder_factory() const {
  return encoder_factory_;
}

VideoEncoder *ObjCVideoEncoderFactory::CreateVideoEncoder(const cricket::VideoCodec &codec) {
  RTCVideoCodecInfo *info = [[RTCVideoCodecInfo alloc] initWithNativeVideoCodec:codec];
  id<RTCVideoEncoder> encoder = [encoder_factory_ createEncoder:info];
  return new ObjCVideoEncoder(encoder);
}

const std::vector<cricket::VideoCodec> &ObjCVideoEncoderFactory::supported_codecs() const {
  supported_codecs_.clear();
  for (RTCVideoCodecInfo *supportedCodec in encoder_factory_.supportedCodecs) {
    cricket::VideoCodec codec = [supportedCodec nativeVideoCodec];
    supported_codecs_.push_back(codec);
  }

  return supported_codecs_;
}

void ObjCVideoEncoderFactory::DestroyVideoEncoder(VideoEncoder *encoder) {
  delete encoder;
  encoder = nullptr;
}

}  // namespace webrtc
