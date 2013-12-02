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

#import "RTCPeerConnection+internal.h"

#import "RTCEnumConverter.h"
#import "RTCICECandidate+internal.h"
#import "RTCICEServer+internal.h"
#import "RTCMediaConstraints+internal.h"
#import "RTCMediaStream+internal.h"
#import "RTCSessionDescription+internal.h"
#import "RTCSessionDescriptonDelegate.h"
#import "RTCSessionDescription.h"

#include "talk/app/webrtc/jsep.h"

NSString* const kRTCSessionDescriptionDelegateErrorDomain = @"RTCSDPError";
int const kRTCSessionDescriptionDelegateErrorCode = -1;

namespace webrtc {

class RTCCreateSessionDescriptionObserver
    : public CreateSessionDescriptionObserver {
 public:
  RTCCreateSessionDescriptionObserver(id<RTCSessionDescriptonDelegate> delegate,
                                      RTCPeerConnection *peerConnection) {
    _delegate = delegate;
    _peerConnection = peerConnection;
  }

  virtual void OnSuccess(SessionDescriptionInterface *desc) OVERRIDE {
    RTCSessionDescription *session =
        [[RTCSessionDescription alloc] initWithSessionDescription:desc];
    [_delegate peerConnection:_peerConnection
        didCreateSessionDescription:session
        error:nil];
  }

  virtual void OnFailure(const std::string &error) OVERRIDE {
    NSString *str = @(error.c_str());
    NSError *err =
        [NSError errorWithDomain:kRTCSessionDescriptionDelegateErrorDomain
                            code:kRTCSessionDescriptionDelegateErrorCode
                        userInfo:@{ @"error" : str }];
    [_delegate peerConnection:_peerConnection
        didCreateSessionDescription:nil
        error:err];
  }

 private:
  id<RTCSessionDescriptonDelegate> _delegate;
  RTCPeerConnection *_peerConnection;
};

class RTCSetSessionDescriptionObserver : public SetSessionDescriptionObserver {
 public:
  RTCSetSessionDescriptionObserver(id<RTCSessionDescriptonDelegate> delegate,
                                   RTCPeerConnection *peerConnection) {
    _delegate = delegate;
    _peerConnection = peerConnection;
  }

  virtual void OnSuccess() OVERRIDE {
    [_delegate peerConnection:_peerConnection
        didSetSessionDescriptionWithError:nil];
  }

  virtual void OnFailure(const std::string &error) OVERRIDE {
    NSString *str = @(error.c_str());
    NSError *err =
        [NSError errorWithDomain:kRTCSessionDescriptionDelegateErrorDomain
                            code:kRTCSessionDescriptionDelegateErrorCode
                        userInfo:@{ @"error" : str }];
    [_delegate peerConnection:_peerConnection
        didSetSessionDescriptionWithError:err];
  }

 private:
  id<RTCSessionDescriptonDelegate> _delegate;
  RTCPeerConnection *_peerConnection;
};

}

@implementation RTCPeerConnection {
  NSMutableArray *_localStreams;
  talk_base::scoped_ptr<webrtc::RTCPeerConnectionObserver>_observer;
  talk_base::scoped_refptr<webrtc::PeerConnectionInterface> _peerConnection;
}

- (BOOL)addICECandidate:(RTCICECandidate *)candidate {
  talk_base::scoped_ptr<const webrtc::IceCandidateInterface> iceCandidate(
      candidate.candidate);
  return self.peerConnection->AddIceCandidate(iceCandidate.get());
}

- (BOOL)addStream:(RTCMediaStream *)stream
      constraints:(RTCMediaConstraints *)constraints {
  BOOL ret = self.peerConnection->AddStream(stream.mediaStream,
                                            constraints.constraints);
  if (!ret) {
    return NO;
  }
  [_localStreams addObject:stream];
  return YES;
}

- (void)createAnswerWithDelegate:(id<RTCSessionDescriptonDelegate>)delegate
                     constraints:(RTCMediaConstraints *)constraints {
  talk_base::scoped_refptr<webrtc::RTCCreateSessionDescriptionObserver>
      observer(new talk_base::RefCountedObject<
          webrtc::RTCCreateSessionDescriptionObserver>(delegate, self));
  self.peerConnection->CreateAnswer(observer, constraints.constraints);
}

- (void)createOfferWithDelegate:(id<RTCSessionDescriptonDelegate>)delegate
                    constraints:(RTCMediaConstraints *)constraints {
  talk_base::scoped_refptr<webrtc::RTCCreateSessionDescriptionObserver>
      observer(new talk_base::RefCountedObject<
          webrtc::RTCCreateSessionDescriptionObserver>(delegate, self));
  self.peerConnection->CreateOffer(observer, constraints.constraints);
}

- (void)removeStream:(RTCMediaStream *)stream {
  self.peerConnection->RemoveStream(stream.mediaStream);
  [_localStreams removeObject:stream];
}

- (void)
    setLocalDescriptionWithDelegate:(id<RTCSessionDescriptonDelegate>)delegate
                 sessionDescription:(RTCSessionDescription *)sdp {
  talk_base::scoped_refptr<webrtc::RTCSetSessionDescriptionObserver> observer(
      new talk_base::RefCountedObject<webrtc::RTCSetSessionDescriptionObserver>(
          delegate, self));
  self.peerConnection->SetLocalDescription(observer, sdp.sessionDescription);
}

- (void)
    setRemoteDescriptionWithDelegate:(id<RTCSessionDescriptonDelegate>)delegate
                  sessionDescription:(RTCSessionDescription *)sdp {
  talk_base::scoped_refptr<webrtc::RTCSetSessionDescriptionObserver> observer(
      new talk_base::RefCountedObject<webrtc::RTCSetSessionDescriptionObserver>(
          delegate, self));
  self.peerConnection->SetRemoteDescription(observer, sdp.sessionDescription);
}

- (BOOL)updateICEServers:(NSArray *)servers
             constraints:(RTCMediaConstraints *)constraints {
  webrtc::PeerConnectionInterface::IceServers iceServers;
  for (RTCICEServer *server in servers) {
    iceServers.push_back(server.iceServer);
  }
  return self.peerConnection->UpdateIce(iceServers, constraints.constraints);
}

- (RTCSessionDescription *)localDescription {
  const webrtc::SessionDescriptionInterface *sdi =
      self.peerConnection->local_description();
  return sdi ?
      [[RTCSessionDescription alloc] initWithSessionDescription:sdi] :
      nil;
}

- (NSArray *)localStreams {
  return [_localStreams copy];
}

- (RTCSessionDescription *)remoteDescription {
  const webrtc::SessionDescriptionInterface *sdi =
      self.peerConnection->remote_description();
  return sdi ?
      [[RTCSessionDescription alloc] initWithSessionDescription:sdi] :
      nil;
}

- (RTCICEConnectionState)iceConnectionState {
  return [RTCEnumConverter convertIceConnectionStateToObjC:
      self.peerConnection->ice_connection_state()];
}

- (RTCICEGatheringState)iceGatheringState {
  return [RTCEnumConverter convertIceGatheringStateToObjC:
      self.peerConnection->ice_gathering_state()];
}

- (RTCSignalingState)signalingState {
  return [RTCEnumConverter
      convertSignalingStateToObjC:self.peerConnection->signaling_state()];
}

- (void)close {
  self.peerConnection->Close();
}

@end

@implementation RTCPeerConnection (Internal)

- (id)initWithPeerConnection:(
    talk_base::scoped_refptr<webrtc::PeerConnectionInterface>)peerConnection
                    observer:(webrtc::RTCPeerConnectionObserver *)observer {
  if (!peerConnection || !observer) {
    NSAssert(NO, @"nil arguments not allowed");
    self = nil;
    return nil;
  }
  if ((self = [super init])) {
    _peerConnection = peerConnection;
    _localStreams = [[NSMutableArray alloc] init];
    _observer.reset(observer);
  }
  return self;
}

- (talk_base::scoped_refptr<webrtc::PeerConnectionInterface>)peerConnection {
  return _peerConnection;
}

@end
