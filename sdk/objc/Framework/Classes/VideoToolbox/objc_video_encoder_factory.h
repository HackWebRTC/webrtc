/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_FRAMEWORK_CLASSES_PEERCONNECTION_OBJC_VIDEO_ENCODER_FACTORY_H_
#define SDK_OBJC_FRAMEWORK_CLASSES_PEERCONNECTION_OBJC_VIDEO_ENCODER_FACTORY_H_

#import <Foundation/Foundation.h>

#include "media/engine/webrtcvideoencoderfactory.h"

@protocol RTCVideoEncoderFactory;

namespace webrtc {

class ObjCVideoEncoderFactory : public cricket::WebRtcVideoEncoderFactory {
 public:
  explicit ObjCVideoEncoderFactory(id<RTCVideoEncoderFactory>);
  ~ObjCVideoEncoderFactory();

  id<RTCVideoEncoderFactory> wrapped_encoder_factory() const;

  webrtc::VideoEncoder* CreateVideoEncoder(
      const cricket::VideoCodec& codec) override;
  const std::vector<cricket::VideoCodec>& supported_codecs() const override;
  void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) override;

 private:
  id<RTCVideoEncoderFactory> encoder_factory_;
  mutable std::vector<cricket::VideoCodec> supported_codecs_;
};

}  // namespace webrtc

#endif  // SDK_OBJC_FRAMEWORK_CLASSES_PEERCONNECTION_OBJC_VIDEO_ENCODER_FACTORY_H_
