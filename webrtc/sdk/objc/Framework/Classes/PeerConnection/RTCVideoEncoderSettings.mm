/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoCodec.h"

#import "NSString+StdString.h"
#import "RTCVideoCodec+Private.h"
#import "WebRTC/RTCVideoCodecFactory.h"

@implementation RTCVideoEncoderSettings

@synthesize name = _name;
@synthesize width = _width;
@synthesize height = _height;
@synthesize startBitrate = _startBitrate;
@synthesize maxBitrate = _maxBitrate;
@synthesize minBitrate = _minBitrate;
@synthesize targetBitrate = _targetBitrate;
@synthesize maxFramerate = _maxFramerate;
@synthesize qpMax = _qpMax;

- (instancetype)initWithNativeVideoCodec:(const webrtc::VideoCodec *)videoCodec {
  if (self = [super init]) {
    if (videoCodec) {
      rtc::Optional<const char *> codecName = CodecTypeToPayloadName(videoCodec->codecType);
      if (codecName) {
        _name = [NSString stringWithUTF8String:codecName.value()];
      }

      _width = videoCodec->width;
      _height = videoCodec->height;
      _startBitrate = videoCodec->startBitrate;
      _maxBitrate = videoCodec->maxBitrate;
      _minBitrate = videoCodec->minBitrate;
      _targetBitrate = videoCodec->targetBitrate;
      _maxFramerate = videoCodec->maxFramerate;
      _qpMax = videoCodec->qpMax;
    }
  }

  return self;
}

- (std::unique_ptr<webrtc::VideoCodec>)createNativeVideoEncoderSettings {
  auto codecSettings = std::unique_ptr<webrtc::VideoCodec>(new webrtc::VideoCodec);

  rtc::Optional<webrtc::VideoCodecType> codecType =
      webrtc::PayloadNameToCodecType([NSString stdStringForString:_name]);
  if (codecType) {
    codecSettings->codecType = codecType.value();
  }

  codecSettings->width = _width;
  codecSettings->height = _height;
  codecSettings->startBitrate = _startBitrate;
  codecSettings->maxBitrate = _maxBitrate;
  codecSettings->minBitrate = _minBitrate;
  codecSettings->targetBitrate = _targetBitrate;
  codecSettings->maxFramerate = _maxFramerate;
  codecSettings->qpMax = _qpMax;

  return codecSettings;
}

@end
