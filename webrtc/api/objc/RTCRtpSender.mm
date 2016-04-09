/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpSender.h"

#import "webrtc/api/objc/RTCRtpParameters+Private.h"
#import "webrtc/api/objc/RTCRtpSender+Private.h"
#import "webrtc/api/objc/RTCMediaStreamTrack+Private.h"

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/rtpsenderinterface.h"

@implementation RTCRtpSender {
  rtc::scoped_refptr<webrtc::RtpSenderInterface> _nativeRtpSender;
}

- (instancetype)initWithNativeRtpSender:
    (rtc::scoped_refptr<webrtc::RtpSenderInterface>)nativeRtpSender {
  if (self = [super init]) {
    _nativeRtpSender = nativeRtpSender;
  }
  return self;
}

- (RTCRtpParameters *)parameters {
  return [[RTCRtpParameters alloc]
      initWithNativeParameters:_nativeRtpSender->GetParameters()];
}

- (BOOL)setParameters:(RTCRtpParameters *)parameters {
  return _nativeRtpSender->SetParameters(parameters.nativeParameters);
}

- (RTCMediaStreamTrack *)track {
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> nativeTrack(
    _nativeRtpSender->track());
  if (nativeTrack) {
    return [[RTCMediaStreamTrack alloc] initWithNativeTrack:nativeTrack];
  }
  return nil;
}

@end
