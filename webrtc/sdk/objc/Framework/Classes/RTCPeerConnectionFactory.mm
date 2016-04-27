/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnectionFactory+Private.h"

#import "NSString+StdString.h"
#if defined(WEBRTC_IOS)
#import "RTCAVFoundationVideoSource+Private.h"
#endif
#import "RTCAudioTrack+Private.h"
#import "RTCMediaStream+Private.h"
#import "RTCPeerConnection+Private.h"
#import "RTCVideoSource+Private.h"
#import "RTCVideoTrack+Private.h"

@implementation RTCPeerConnectionFactory {
  rtc::scoped_ptr<rtc::Thread> _signalingThread;
  rtc::scoped_ptr<rtc::Thread> _workerThread;
}

@synthesize nativeFactory = _nativeFactory;

- (instancetype)init {
  if ((self = [super init])) {
    _signalingThread.reset(new rtc::Thread());
    BOOL result = _signalingThread->Start();
    NSAssert(result, @"Failed to start signaling thread.");
    _workerThread.reset(new rtc::Thread());
    result = _workerThread->Start();
    NSAssert(result, @"Failed to start worker thread.");

    _nativeFactory = webrtc::CreatePeerConnectionFactory(
        _workerThread.get(), _signalingThread.get(), nullptr, nullptr, nullptr);
    NSAssert(_nativeFactory, @"Failed to initialize PeerConnectionFactory!");
  }
  return self;
}

- (RTCAVFoundationVideoSource *)avFoundationVideoSourceWithConstraints:
    (nullable RTCMediaConstraints *)constraints {
#if defined(WEBRTC_IOS)
  return [[RTCAVFoundationVideoSource alloc] initWithFactory:self
                                                 constraints:constraints];
#else
  return nil;
#endif
}

- (RTCAudioTrack *)audioTrackWithTrackId:(NSString *)trackId {
  return [[RTCAudioTrack alloc] initWithFactory:self
                                        trackId:trackId];
}

- (RTCVideoTrack *)videoTrackWithSource:(RTCVideoSource *)source
                                trackId:(NSString *)trackId {
  return [[RTCVideoTrack alloc] initWithFactory:self
                                         source:source
                                        trackId:trackId];
}

- (RTCMediaStream *)mediaStreamWithStreamId:(NSString *)streamId {
  return [[RTCMediaStream alloc] initWithFactory:self
                                        streamId:streamId];
}

- (RTCPeerConnection *)peerConnectionWithConfiguration:
    (RTCConfiguration *)configuration
                                           constraints:
    (RTCMediaConstraints *)constraints
                                              delegate:
    (nullable id<RTCPeerConnectionDelegate>)delegate {
  return [[RTCPeerConnection alloc] initWithFactory:self
                                      configuration:configuration
                                        constraints:constraints
                                           delegate:delegate];
}

@end
