/*
 * Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 * Use of this source code is governed by a BSD-style license.
 */
#import "RTCCodecSpecificInfoH265+Private.h"

@implementation RTC_OBJC_TYPE(RTCCodecSpecificInfoH265)
- (webrtc::CodecSpecificInfo)nativeCodecSpecificInfo {
  webrtc::CodecSpecificInfo info;
  info.codecType = webrtc::kVideoCodecH265;
  return info;
}
@end
