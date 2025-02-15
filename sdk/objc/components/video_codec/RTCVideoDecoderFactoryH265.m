/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoDecoderFactoryH265.h"

#import "RTCH265ProfileLevelId.h"
#import "RTCVideoDecoderH265.h"

@implementation RTC_OBJC_TYPE (RTCVideoDecoderFactoryH265)

- (bool) transitMode {
    return false;
}

- (id<RTC_OBJC_TYPE(RTCVideoDecoder)>)createDecoder:(RTC_OBJC_TYPE(RTCVideoCodecInfo)*)info {
  return [[RTC_OBJC_TYPE(RTCVideoDecoderH265) alloc] init];
}

- (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo)*>*)supportedCodecs {
  NSString* codecName = kRTCVideoCodecH265Name;
  return @[ [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:codecName parameters:nil] ];
}

@end
