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

@implementation RTCVideoDecoderFactoryH265

- (id<RTCVideoDecoder>)createDecoder:(RTCVideoCodecInfo*)info {
  return [[RTCVideoDecoderH265 alloc] init];
}

- (NSArray<RTCVideoCodecInfo*>*)supportedCodecs {
  NSString* codecName = kRTCVideoCodecH265Name;
  return @[ [[RTCVideoCodecInfo alloc] initWithName:codecName parameters:nil] ];
}

@end
