/*
 * Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 * Use of this source code is governed by a BSD-style license.
 */
#import "RTCVideoDecoderH264.h"

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE(RTCVideoDecoderH265)
    : RTC_OBJC_TYPE(RTCVideoDecoderH264)
+ (BOOL)isSupported;
+ (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)supportedCodecs;
@end
