/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoCodecH264.h"

#include <vector>

#import "RTCVideoCodec+Private.h"
#import "WebRTC/RTCVideoCodec.h"

#include "webrtc/rtc_base/timeutils.h"
#include "webrtc/sdk/objc/Framework/Classes/Video/objc_frame_buffer.h"
#include "webrtc/system_wrappers/include/field_trial.h"

const char kHighProfileExperiment[] = "WebRTC-H264HighProfile";
static NSString *kLevel31ConstrainedHigh = @"640c1f";
static NSString *kLevel31ConstrainedBaseline = @"42e01f";

bool IsHighProfileEnabled() {
  return webrtc::field_trial::IsEnabled(kHighProfileExperiment);
}

// H264 specific settings.
@implementation RTCCodecSpecificInfoH264

@synthesize packetizationMode = _packetizationMode;

- (webrtc::CodecSpecificInfo)nativeCodecSpecificInfo {
  webrtc::CodecSpecificInfo codecSpecificInfo;
  codecSpecificInfo.codecType = webrtc::kVideoCodecH264;
  codecSpecificInfo.codec_name = "H264";
  codecSpecificInfo.codecSpecific.H264.packetization_mode =
      (webrtc::H264PacketizationMode)_packetizationMode;

  return codecSpecificInfo;
}

@end

// Encoder factory.
@implementation RTCVideoEncoderFactoryH264

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  NSMutableArray<RTCVideoCodecInfo *> *codecs = [NSMutableArray array];
  NSString *codecName = [NSString stringWithUTF8String:cricket::kH264CodecName];

  if (IsHighProfileEnabled()) {
    NSDictionary<NSString *, NSString *> *constrainedHighParams = @{
      @"profile-level-id" : kLevel31ConstrainedHigh,
      @"level-asymmetry-allowed" : @"1",
      @"packetization-mode" : @"1",
    };
    RTCVideoCodecInfo *constrainedHighInfo =
        [[RTCVideoCodecInfo alloc] initWithPayload:0
                                              name:codecName
                                        parameters:constrainedHighParams];
    [codecs addObject:constrainedHighInfo];
  }

  NSDictionary<NSString *, NSString *> *constrainedBaselineParams = @{
    @"profile-level-id" : kLevel31ConstrainedBaseline,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTCVideoCodecInfo *constrainedBaselineInfo =
      [[RTCVideoCodecInfo alloc] initWithPayload:0
                                            name:codecName
                                      parameters:constrainedBaselineParams];
  [codecs addObject:constrainedBaselineInfo];

  return [codecs copy];
}

- (id<RTCVideoEncoder>)createEncoder:(RTCVideoCodecInfo *)info {
  return [[RTCVideoEncoderH264 alloc] initWithCodecInfo:info];
}

@end

// Decoder factory.
@implementation RTCVideoDecoderFactoryH264

- (id<RTCVideoDecoder>)createDecoder:(RTCVideoCodecInfo *)info {
  return [[RTCVideoDecoderH264 alloc] init];
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  NSString *codecName = [NSString stringWithUTF8String:cricket::kH264CodecName];
  return @[ [[RTCVideoCodecInfo alloc] initWithPayload:0 name:codecName parameters:@{}] ];
}

@end
