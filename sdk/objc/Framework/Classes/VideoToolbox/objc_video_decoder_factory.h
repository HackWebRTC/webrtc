/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_PEERCONNECTION_OBJC_VIDEO_DECODER_FACTORY_H_
#define WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_PEERCONNECTION_OBJC_VIDEO_DECODER_FACTORY_H_

#include "webrtc/media/base/codec.h"
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"

@protocol RTCVideoDecoderFactory;

namespace webrtc {

class ObjCVideoDecoderFactory : public cricket::WebRtcVideoDecoderFactory {
 public:
  explicit ObjCVideoDecoderFactory(id<RTCVideoDecoderFactory>);
  ~ObjCVideoDecoderFactory();

  id<RTCVideoDecoderFactory> wrapped_decoder_factory() const;

  VideoDecoder* CreateVideoDecoderWithParams(
      const cricket::VideoCodec& codec,
      cricket::VideoDecoderParams params) override;

  void DestroyVideoDecoder(webrtc::VideoDecoder* decoder) override;

 private:
  id<RTCVideoDecoderFactory> decoder_factory_;
  std::vector<cricket::VideoCodec> supported_codecs_;
};

}  // namespace webrtc

#endif  // WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_PEERCONNECTION_OBJC_VIDEO_DECODER_FACTORY_H_
