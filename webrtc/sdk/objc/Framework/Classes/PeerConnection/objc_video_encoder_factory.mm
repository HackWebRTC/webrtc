/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/objc/Framework/Classes/PeerConnection/objc_video_encoder_factory.h"

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
class ObjCVideoEncoder : public VideoEncoder {
 public:
  ObjCVideoEncoder(id<RTCVideoEncoder> encoder) : encoder_(encoder) {}
  ~ObjCVideoEncoder() {}

  int32_t InitEncode(const VideoCodec *codec_settings,
                     int32_t number_of_cores,
                     size_t max_payload_size) {
    RTCVideoEncoderSettings *settings =
        [[RTCVideoEncoderSettings alloc] initWithVideoCodec:codec_settings];
    return [encoder_ initEncodeWithSettings:settings numberOfCores:number_of_cores];
  }

  int32_t RegisterEncodeCompleteCallback(EncodedImageCallback *callback) {
    [encoder_ setCallback:^(RTCEncodedImage *frame,
                            id<RTCCodecSpecificInfo> info,
                            RTCRtpFragmentationHeader *header) {
      EncodedImage encodedImage = [frame toCpp];

      // Handle types than can be converted into one of webrtc::CodecSpecificInfo's hard coded
      // cases.
      CodecSpecificInfo codecSpecificInfo;
      if ([info isKindOfClass:[RTCCodecSpecificInfoH264 class]]) {
        codecSpecificInfo = [(RTCCodecSpecificInfoH264 *)info toCpp];
      }

      RTPFragmentationHeader *fragmentationHeader = [header toCpp];
      callback->OnEncodedImage(encodedImage, &codecSpecificInfo, fragmentationHeader);
    }];

    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() { return [encoder_ releaseEncode]; }

  int32_t Encode(const VideoFrame &frame,
                 const CodecSpecificInfo *codec_specific_info,
                 const std::vector<FrameType> *frame_types) {
    RTC_CHECK(frame.video_frame_buffer()->type() == VideoFrameBuffer::Type::kNative);

    id<RTCVideoFrameBuffer> frame_buffer =
        static_cast<ObjCFrameBuffer *>(frame.video_frame_buffer().get())->wrapped_frame_buffer();
    RTCVideoFrame *rtcFrame =
        [[RTCVideoFrame alloc] initWithBuffer:frame_buffer
                                     rotation:RTCVideoRotation(frame.rotation())
                                  timeStampNs:frame.timestamp_us() * rtc::kNumNanosecsPerMicrosec];
    rtcFrame.timeStamp = frame.timestamp();

    // webrtc::CodecSpecificInfo only handles a hard coded list of codecs
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

    return
        [encoder_ encode:rtcFrame codecSpecificInfo:rtcCodecSpecificInfo frameTypes:rtcFrameTypes];
  }

  int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) { return WEBRTC_VIDEO_CODEC_OK; }

  int32_t SetRates(uint32_t bitrate, uint32_t framerate) {
    if ([encoder_ setBitrate:bitrate framerate:framerate]) {
      return WEBRTC_VIDEO_CODEC_OK;
    } else {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  bool SupportsNativeHandle() const { return true; }

 private:
  id<RTCVideoEncoder> encoder_;
};
}  // namespace

ObjCVideoEncoderFactory::ObjCVideoEncoderFactory(id<RTCVideoEncoderFactory> encoder_factory)
    : encoder_factory_(encoder_factory) {}

ObjCVideoEncoderFactory::~ObjCVideoEncoderFactory() {}

id<RTCVideoEncoderFactory> ObjCVideoEncoderFactory::wrapped_encoder_factory() const {
  return encoder_factory_;
}

webrtc::VideoEncoder *ObjCVideoEncoderFactory::CreateVideoEncoder(
    const cricket::VideoCodec &codec) {
  RTCVideoCodecInfo *info = [[RTCVideoCodecInfo alloc] initWithVideoCodec:codec];
  id<RTCVideoEncoder> encoder = [encoder_factory_ createEncoder:info];
  return new ObjCVideoEncoder(encoder);
}

const std::vector<cricket::VideoCodec> &ObjCVideoEncoderFactory::supported_codecs() const {
  supported_codecs_.clear();
  for (RTCVideoCodecInfo *supportedCodec in encoder_factory_.supportedCodecs) {
    cricket::VideoCodec codec = [supportedCodec toCpp];
    supported_codecs_.push_back(codec);
  }

  return supported_codecs_;
}

void ObjCVideoEncoderFactory::DestroyVideoEncoder(webrtc::VideoEncoder *encoder) {
  delete encoder;
  encoder = nullptr;
}

}  // namespace webrtc
