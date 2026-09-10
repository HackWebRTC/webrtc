/*
 * Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 * Use of this source code is governed by a BSD-style license.
 */
#import "RTCVideoEncoderH264.h"

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE(RTCVideoEncoderH265)
    : RTC_OBJC_TYPE(RTCVideoEncoderH264)
+ (BOOL)isSupported;
+ (BOOL)isHardwareAccelerated;
+ (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)supportedCodecs;
@end
