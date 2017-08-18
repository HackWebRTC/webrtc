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

@implementation RTCVideoCodecInfo

@synthesize payload = _payload;
@synthesize name = _name;
@synthesize parameters = _parameters;

- (instancetype)initWithName:(NSString *)name
                  parameters:(nullable NSDictionary<NSString *, NSString *> *)parameters {
  if (self = [super init]) {
    _payload = 0;
    _name = name;
    _parameters = (parameters ? parameters : @{});
  }

  return self;
}

- (instancetype)initWithNativeVideoCodec:(cricket::VideoCodec)videoCodec {
  NSMutableDictionary *params = [NSMutableDictionary dictionary];
  for (auto it = videoCodec.params.begin(); it != videoCodec.params.end(); ++it) {
    [params setObject:[NSString stringForStdString:it->second]
               forKey:[NSString stringForStdString:it->first]];
  }
  return [self initWithPayload:videoCodec.id
                          name:[NSString stringForStdString:videoCodec.name]
                    parameters:params];
}

- (instancetype)initWithPayload:(NSInteger)payload
                           name:(NSString *)name
                     parameters:(NSDictionary<NSString *, NSString *> *)parameters {
  if (self = [self initWithName:name parameters:parameters]) {
    _payload = payload;
  }

  return self;
}

- (cricket::VideoCodec)nativeVideoCodec {
  cricket::VideoCodec codec([NSString stdStringForString:_name]);
  for (NSString *paramKey in _parameters.allKeys) {
    codec.SetParam([NSString stdStringForString:paramKey],
                   [NSString stdStringForString:_parameters[paramKey]]);
  }

  return codec;
}

@end

@implementation RTCVideoEncoderQpThresholds

@synthesize low = _low;
@synthesize high = _high;

- (instancetype)initWithThresholdsLow:(NSInteger)low high:(NSInteger)high {
  if (self = [super init]) {
    _low = low;
    _high = high;
  }
  return self;
}

@end
