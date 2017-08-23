/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCBuiltinAudioEncoderFactory.h"

#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/rtc_base/scoped_ref_ptr.h"

@implementation RTCBuiltinAudioEncoderFactory {
  rtc::scoped_refptr<webrtc::AudioEncoderFactory> _nativeAudioEncoderFactory;
}

- (rtc::scoped_refptr<webrtc::AudioEncoderFactory>)nativeAudioEncoderFactory {
  if (_nativeAudioEncoderFactory == nullptr) {
    _nativeAudioEncoderFactory = webrtc::CreateBuiltinAudioEncoderFactory();
  }
  return _nativeAudioEncoderFactory;
}

@end
