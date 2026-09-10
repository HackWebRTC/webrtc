/*
 * Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 * Use of this source code is governed by a BSD-style license.
 */
#import "RTCVideoEncoderH265.h"

#import "RTCH265ProfileLevelId.h"

#import <VideoToolbox/VideoToolbox.h>

@implementation RTC_OBJC_TYPE(RTCVideoEncoderH265)
+ (BOOL)isHardwareAccelerated {
  CFArrayRef encoders = nullptr;
  if (VTCopyVideoEncoderList(nullptr, &encoders) != noErr || !encoders)
    return NO;
  BOOL supported = NO;
  for (CFIndex index = 0; index < CFArrayGetCount(encoders); ++index) {
    CFDictionaryRef encoder = static_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(encoders, index));
    CFNumberRef codec = static_cast<CFNumberRef>(
        CFDictionaryGetValue(encoder, kVTVideoEncoderList_CodecType));
    int64_t codec_type = 0;
    if (!codec || !CFNumberGetValue(codec, kCFNumberSInt64Type, &codec_type) ||
        codec_type != kCMVideoCodecType_HEVC) {
      continue;
    }
    CFBooleanRef hardware = static_cast<CFBooleanRef>(CFDictionaryGetValue(
        encoder, kVTVideoEncoderList_IsHardwareAccelerated));
    if (hardware && CFBooleanGetValue(hardware)) {
      supported = YES;
      break;
    }
  }
  CFRelease(encoders);
  return supported;
}

+ (BOOL)isSupported {
  CFStringRef encoder_id = nullptr;
  CFDictionaryRef properties = nullptr;
  OSStatus status = VTCopySupportedPropertyDictionaryForEncoder(
      640, 480, kCMVideoCodecType_HEVC, nullptr, &encoder_id, &properties);
  if (encoder_id)
    CFRelease(encoder_id);
  if (properties)
    CFRelease(properties);
  return status == noErr;
}

+ (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)supportedCodecs {
  if (![self isSupported])
    return @[];
  return @[ [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc]
      initWithName:kRTCVideoCodecH265Name
        parameters:@{}] ];
}
@end
