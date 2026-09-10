/*
 * Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 * Use of this source code is governed by a BSD-style license.
 */
#import "RTCCodecSpecificInfoH265.h"

#include "modules/video_coding/include/video_codec_interface.h"

@interface RTC_OBJC_TYPE(RTCCodecSpecificInfoH265) ()
- (webrtc::CodecSpecificInfo)nativeCodecSpecificInfo;
@end
