/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpCodecParameters+Private.h"

#import "webrtc/base/objc/NSString+StdString.h"
#import "webrtc/media/base/mediaconstants.h"

const NSString * const kRtxCodecMimeType = @(cricket::kRtxCodecName);
const NSString * const kRedCodecMimeType = @(cricket::kRedCodecName);
const NSString * const kUlpfecCodecMimeType = @(cricket::kUlpfecCodecName);
const NSString * const kOpusCodecMimeType = @(cricket::kOpusCodecName);
const NSString * const kIsacCodecMimeType = @(cricket::kIsacCodecName);
const NSString * const kL16CodecMimeType  = @(cricket::kL16CodecName);
const NSString * const kG722CodecMimeType = @(cricket::kG722CodecName);
const NSString * const kIlbcCodecMimeType = @(cricket::kIlbcCodecName);
const NSString * const kPcmuCodecMimeType = @(cricket::kPcmuCodecName);
const NSString * const kPcmaCodecMimeType = @(cricket::kPcmaCodecName);
const NSString * const kDtmfCodecMimeType = @(cricket::kDtmfCodecName);
const NSString * const kComfortNoiseCodecMimeType = @(cricket::kComfortNoiseCodecName);
const NSString * const kVp8CodecMimeType = @(cricket::kVp8CodecName);
const NSString * const kVp9CodecMimeType = @(cricket::kVp9CodecName);
const NSString * const kH264CodecMimeType = @(cricket::kH264CodecName);

@implementation RTCRtpCodecParameters

@synthesize payloadType = _payloadType;
@synthesize mimeType = _mimeType;
@synthesize clockRate = _clockRate;
@synthesize channels = _channels;

- (instancetype)init {
  return [super init];
}

- (instancetype)initWithNativeParameters:
    (const webrtc::RtpCodecParameters &)nativeParameters {
  if (self = [self init]) {
    _payloadType = nativeParameters.payload_type;
    _mimeType = [NSString stringForStdString:nativeParameters.mime_type];
    _clockRate = nativeParameters.clock_rate;
    _channels = nativeParameters.channels;
  }
  return self;
}

- (webrtc::RtpCodecParameters)nativeParameters {
  webrtc::RtpCodecParameters parameters;
  parameters.payload_type = _payloadType;
  parameters.mime_type = [NSString stdStringForString:_mimeType];
  parameters.clock_rate = _clockRate;
  parameters.channels = _channels;
  return parameters;
}

@end
