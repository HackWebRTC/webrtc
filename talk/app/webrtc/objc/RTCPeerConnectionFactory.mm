/*
 * libjingle
 * Copyright 2013, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "RTCPeerConnectionFactory.h"

#include <vector>

#import "RTCAudioTrack+internal.h"
#import "RTCICEServer+internal.h"
#import "RTCMediaConstraints+internal.h"
#import "RTCMediaSource+internal.h"
#import "RTCMediaStream+internal.h"
#import "RTCMediaStreamTrack+internal.h"
#import "RTCPeerConnection+internal.h"
#import "RTCPeerConnectionDelegate.h"
#import "RTCPeerConnectionObserver.h"
#import "RTCVideoCapturer+internal.h"
#import "RTCVideoSource+internal.h"
#import "RTCVideoTrack+internal.h"

#include "talk/app/webrtc/audiotrack.h"
#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/peerconnectionfactory.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/videosourceinterface.h"
#include "talk/app/webrtc/videotrack.h"
#include "talk/base/logging.h"

@interface RTCPeerConnectionFactory ()

@property(nonatomic, assign) talk_base::scoped_refptr<
    webrtc::PeerConnectionFactoryInterface> nativeFactory;

@end

@implementation RTCPeerConnectionFactory

- (id)init {
  if ((self = [super init])) {
    _nativeFactory = webrtc::CreatePeerConnectionFactory();
    NSAssert(_nativeFactory, @"Failed to initialize PeerConnectionFactory!");
    // Uncomment to get sensitive logs emitted (to stderr or logcat).
    // talk_base::LogMessage::LogToDebug(talk_base::LS_SENSITIVE);
  }
  return self;
}

- (RTCPeerConnection *)
    peerConnectionWithICEServers:(NSArray *)servers
                     constraints:(RTCMediaConstraints *)constraints
                        delegate:(id<RTCPeerConnectionDelegate>)delegate {
  webrtc::PeerConnectionInterface::IceServers iceServers;
  for (RTCICEServer *server in servers) {
    iceServers.push_back(server.iceServer);
  }
  webrtc::RTCPeerConnectionObserver *observer =
      new webrtc::RTCPeerConnectionObserver(delegate);
  webrtc::DTLSIdentityServiceInterface* dummy_dtls_identity_service = NULL;
  talk_base::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection =
      self.nativeFactory->CreatePeerConnection(
          iceServers, constraints.constraints, dummy_dtls_identity_service,
          observer);
  RTCPeerConnection *pc =
      [[RTCPeerConnection alloc] initWithPeerConnection:peerConnection
                                               observer:observer];
  observer->SetPeerConnection(pc);
  return pc;
}

- (RTCMediaStream *)mediaStreamWithLabel:(NSString *)label {
  talk_base::scoped_refptr<webrtc::MediaStreamInterface> nativeMediaStream =
      self.nativeFactory->CreateLocalMediaStream([label UTF8String]);
  return [[RTCMediaStream alloc] initWithMediaStream:nativeMediaStream];
}

- (RTCVideoSource *)videoSourceWithCapturer:(RTCVideoCapturer *)capturer
                                constraints:(RTCMediaConstraints *)constraints {
  if (!capturer) {
    return nil;
  }
  talk_base::scoped_refptr<webrtc::VideoSourceInterface> source =
      self.nativeFactory->CreateVideoSource(capturer.capturer.get(),
                                            constraints.constraints);
  return [[RTCVideoSource alloc] initWithMediaSource:source];
}

- (RTCVideoTrack *)videoTrackWithID:(NSString *)videoId
                             source:(RTCVideoSource *)source {
  talk_base::scoped_refptr<webrtc::VideoTrackInterface> track =
      self.nativeFactory->CreateVideoTrack([videoId UTF8String],
                                           source.videoSource);
  return [[RTCVideoTrack alloc] initWithMediaTrack:track];
}

- (RTCAudioTrack *)audioTrackWithID:(NSString *)audioId {
  talk_base::scoped_refptr<webrtc::AudioTrackInterface> track =
      self.nativeFactory->CreateAudioTrack([audioId UTF8String], NULL);
  return [[RTCAudioTrack alloc] initWithMediaTrack:track];
}

@end
