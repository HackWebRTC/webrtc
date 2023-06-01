/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCDefaultVideoEncoderFactory.h"

#import "RTCH264ProfileLevelId.h"
#import "RTCVideoEncoderH264.h"
#import "api/video_codec/RTCVideoCodecConstants.h"
#import "api/video_codec/RTCVideoEncoderVP8.h"
#import "api/video_codec/RTCVideoEncoderVP9.h"
#import "base/RTCVideoCodecInfo.h"

#if defined(RTC_USE_LIBAOM_AV1_ENCODER)
#import "api/video_codec/RTCVideoEncoderAV1.h"  // nogncheck
#endif
#ifdef WEBRTC_USE_H265
#import "RTCH265ProfileLevelId.h"
#import "RTCVideoEncoderH265.h"
#endif

@implementation RTC_OBJC_TYPE (RTCDefaultVideoEncoderFactory)

@synthesize preferredCodec;

+ (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)supportedCodecs {
  NSDictionary<NSString *, NSString *> *constrainedHighParams = @{
    @"profile-level-id" : kRTCMaxSupportedH264ProfileLevelConstrainedHigh,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTC_OBJC_TYPE(RTCVideoCodecInfo) *constrainedHighInfo =
      [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:kRTCVideoCodecH264Name
                                                  parameters:constrainedHighParams];

  NSDictionary<NSString *, NSString *> *constrainedBaselineParams = @{
    @"profile-level-id" : kRTCMaxSupportedH264ProfileLevelConstrainedBaseline,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTC_OBJC_TYPE(RTCVideoCodecInfo) *constrainedBaselineInfo =
      [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:kRTCVideoCodecH264Name
                                                  parameters:constrainedBaselineParams];

  RTC_OBJC_TYPE(RTCVideoCodecInfo) *vp8Info =
      [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:kRTCVideoCodecVp8Name];

  NSMutableArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *result = [@[
    constrainedHighInfo,
    constrainedBaselineInfo,
    vp8Info,
  ] mutableCopy];

  if ([RTC_OBJC_TYPE(RTCVideoEncoderVP9) isSupported]) {
    [result
        addObject:[[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:kRTCVideoCodecVp9Name]];
  }

#if defined(RTC_USE_LIBAOM_AV1_ENCODER)
  [result addObject:[[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:kRTCVideoCodecAv1Name]];
#endif
#ifdef WEBRTC_USE_H265
  RTCVideoCodecInfo *h265Info = [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecH265Name];
  [result addObject:h265Info];
#endif

  return result;
}

- (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)createEncoder:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info {
  if ([info.name isEqualToString:kRTCVideoCodecH264Name]) {
    return [[RTC_OBJC_TYPE(RTCVideoEncoderH264) alloc] initWithCodecInfo:info];
  } else if ([info.name isEqualToString:kRTCVideoCodecVp8Name]) {
    return [RTC_OBJC_TYPE(RTCVideoEncoderVP8) vp8Encoder];
  } else if ([info.name isEqualToString:kRTCVideoCodecVp9Name] &&
             [RTC_OBJC_TYPE(RTCVideoEncoderVP9) isSupported]) {
    return [RTC_OBJC_TYPE(RTCVideoEncoderVP9) vp9Encoder];
#ifdef WEBRTC_USE_H265
  } else if (@available(iOS 11, *)) {
    if ([info.name isEqualToString:kRTCVideoCodecH265Name]) {
      return [[RTCVideoEncoderH265 alloc] initWithCodecInfo:info];
    }
#endif
  }

#if defined(RTC_USE_LIBAOM_AV1_ENCODER)
  if ([info.name isEqualToString:kRTCVideoCodecAv1Name]) {
    return [RTC_OBJC_TYPE(RTCVideoEncoderAV1) av1Encoder];
  }
#endif

  return nil;
}

- (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)supportedCodecs {
  NSMutableArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *codecs =
      [[[self class] supportedCodecs] mutableCopy];

  NSMutableArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *orderedCodecs = [NSMutableArray array];
  NSUInteger index = [codecs indexOfObject:self.preferredCodec];
  if (index != NSNotFound) {
    [orderedCodecs addObject:[codecs objectAtIndex:index]];
    [codecs removeObjectAtIndex:index];
  }
  [orderedCodecs addObjectsFromArray:codecs];

  return [orderedCodecs copy];
}

@end
