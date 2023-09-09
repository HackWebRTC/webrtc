/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "RTCVideoEncoderH265.h"
#import "RTCH265ProfileLevelId.h"
#include "modules/video_coding/include/video_error_codes.h"

@implementation RTC_OBJC_TYPE (RTCVideoEncoderH265)

+ (bool)supported {
  RTC_OBJC_TYPE(RTCVideoCodecInfo) *info = [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:kRTCVideoCodecH265Name];
  RTC_OBJC_TYPE(RTCVideoEncoderH265) *encoder = [[RTC_OBJC_TYPE(RTCVideoEncoderH265) alloc] initWithCodecInfo:info];
  RTC_OBJC_TYPE(RTCVideoEncoderSettings) *settings = [[RTC_OBJC_TYPE(RTCVideoEncoderSettings) alloc] init];
  settings.name = kRTCVideoCodecH265Name;
  settings.width = 1280;
  settings.height = 720;
  settings.startBitrate = 800;
  settings.mode = RTCVideoCodecModeRealtimeVideo;
  bool supported = [encoder startEncodeWithSettings:settings numberOfCores:1] == WEBRTC_VIDEO_CODEC_OK;
  [encoder releaseEncoder];
  encoder = nil;
  return supported;
}

@end
