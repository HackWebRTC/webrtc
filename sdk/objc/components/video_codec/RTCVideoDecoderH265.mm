/*
 * Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 * Use of this source code is governed by a BSD-style license.
 */
#import "RTCVideoDecoderH265.h"

#import "RTCH265ProfileLevelId.h"

#import <VideoToolbox/VideoToolbox.h>

@implementation RTC_OBJC_TYPE(RTCVideoDecoderH265)
+ (BOOL)isSupported {
  return VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC);
}

+ (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)supportedCodecs {
  if (![self isSupported])
    return @[];
  return @[ [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc]
      initWithName:kRTCVideoCodecH265Name
        parameters:@{}] ];
}
@end
