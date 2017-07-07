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

#import "RTCVideoCodec+Private.h"
#import "WebRTC/RTCVideoCodecFactory.h"

#include "webrtc/sdk/objc/Framework/Classes/Common/helpers.h"

@implementation RTCVideoCodecInfo

@synthesize payload = _payload;
@synthesize name = _name;
@synthesize parameters = _parameters;

- (instancetype)initWithPayload:(int)payload
                           name:(NSString *)name
                     parameters:(NSDictionary<NSString *, NSString *> *)parameters {
  if (self = [super init]) {
    _payload = payload;
    _name = name;
    _parameters = parameters;
  }

  return self;
}

- (instancetype)initWithVideoCodec:(cricket::VideoCodec)videoCodec {
  NSMutableDictionary *params = [NSMutableDictionary dictionary];
  for (auto it = videoCodec.params.begin(); it != videoCodec.params.end(); ++it) {
    [params setObject:webrtc::ios::NSStringFromStdString(it->second)
               forKey:webrtc::ios::NSStringFromStdString(it->first)];
  }
  return [self initWithPayload:videoCodec.id
                          name:webrtc::ios::NSStringFromStdString(videoCodec.name)
                    parameters:params];
}

- (cricket::VideoCodec)toCpp {
  cricket::VideoCodec codec(webrtc::ios::StdStringFromNSString(_name));
  for (NSString *paramKey in [_parameters allKeys]) {
    codec.SetParam(webrtc::ios::StdStringFromNSString(paramKey),
                   webrtc::ios::StdStringFromNSString(_parameters[paramKey]));
  }

  return codec;
}

@end

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

- (instancetype)initWithVideoCodec:(const webrtc::VideoCodec *__nullable)videoCodec {
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

- (webrtc::VideoCodec *)toCpp {
  webrtc::VideoCodec *codecSettings = new webrtc::VideoCodec;

  rtc::Optional<webrtc::VideoCodecType> codecType =
      webrtc::PayloadNameToCodecType(webrtc::ios::StdStringFromNSString(_name));
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
