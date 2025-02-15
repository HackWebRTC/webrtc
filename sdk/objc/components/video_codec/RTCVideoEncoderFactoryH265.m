/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoEncoderFactoryH265.h"

#import "RTCH265ProfileLevelId.h"
#import "RTCVideoEncoderH265.h"

@implementation RTC_OBJC_TYPE (RTCVideoEncoderFactoryH265)

- (bool) transitMode {
    return false;
}

- (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo)*>*)supportedCodecs {
  NSMutableArray<RTC_OBJC_TYPE(RTCVideoCodecInfo)*>* codecs = [NSMutableArray array];
  NSString* codecName = kRTCVideoCodecH265Name;

  NSDictionary<NSString*, NSString*>* mainParams = @{
    @"profile-level-id" : kRTCLevel31Main,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTC_OBJC_TYPE(RTCVideoCodecInfo)* constrainedBaselineInfo =
      [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:codecName parameters:mainParams];
  [codecs addObject:constrainedBaselineInfo];

  return [codecs copy];
}

- (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)createEncoder:(RTC_OBJC_TYPE(RTCVideoCodecInfo)*)info {
  return [[RTC_OBJC_TYPE(RTCVideoEncoderH265) alloc] initWithCodecInfo:info];
}

@end
