/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCAudioTrack.h"

#import "webrtc/api/objc/RTCAudioTrack+Private.h"
#import "webrtc/api/objc/RTCMediaStreamTrack+Private.h"
#import "webrtc/api/objc/RTCPeerConnectionFactory+Private.h"
#import "webrtc/base/objc/NSString+StdString.h"

@implementation RTCAudioTrack

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                        trackId:(NSString *)trackId {
  NSParameterAssert(factory);
  NSParameterAssert(trackId.length);
  std::string nativeId = [NSString stdStringForString:trackId];
  rtc::scoped_refptr<webrtc::AudioTrackInterface> track =
      factory.nativeFactory->CreateAudioTrack(nativeId, nullptr);
  return [self initWithNativeTrack:track type:RTCMediaStreamTrackTypeAudio];
}

- (instancetype)initWithNativeTrack:
    (rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeTrack
                               type:(RTCMediaStreamTrackType)type {
  NSParameterAssert(nativeTrack);
  NSParameterAssert(type == RTCMediaStreamTrackTypeAudio);
  return [super initWithNativeTrack:nativeTrack type:type];
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::AudioTrackInterface>)nativeAudioTrack {
  return static_cast<webrtc::AudioTrackInterface *>(self.nativeTrack.get());
}

@end
