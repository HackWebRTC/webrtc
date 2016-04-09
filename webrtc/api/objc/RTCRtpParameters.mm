/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpParameters+Private.h"
#import "RTCRtpEncodingParameters+Private.h"

@implementation RTCRtpParameters

@synthesize encodings = _encodings;

- (instancetype)init {
  return [super init];
}

- (instancetype)initWithNativeParameters:
    (const webrtc::RtpParameters &)nativeParameters {
  if (self = [self init]) {
    NSMutableArray *encodings = [[NSMutableArray alloc] init];
    for (const auto &encoding : nativeParameters.encodings) {
      [encodings addObject:[[RTCRtpEncodingParameters alloc]
                               initWithNativeParameters:encoding]];
    }
    _encodings = encodings;
  }
  return self;
}

- (webrtc::RtpParameters)nativeParameters {
    webrtc::RtpParameters parameters;
  for (RTCRtpEncodingParameters *encoding in _encodings) {
    parameters.encodings.push_back(encoding.nativeParameters);
  }
  return parameters;
}

@end
