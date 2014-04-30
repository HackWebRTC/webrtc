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

#import "RTCPeerConnection+Internal.h"

#import "RTCDataChannel+Internal.h"
#import "RTCEnumConverter.h"
#import "RTCICECandidate+Internal.h"
#import "RTCICEServer+Internal.h"
#import "RTCMediaConstraints+Internal.h"
#import "RTCMediaStream+Internal.h"
#import "RTCMediaStreamTrack+Internal.h"
#import "RTCSessionDescription+Internal.h"
#import "RTCSessionDescriptionDelegate.h"
#import "RTCSessionDescription.h"
#import "RTCStatsDelegate.h"
#import "RTCStatsReport+Internal.h"

#include "talk/app/webrtc/jsep.h"

NSString* const kRTCSessionDescriptionDelegateErrorDomain = @"RTCSDPError";
int const kRTCSessionDescriptionDelegateErrorCode = -1;

namespace webrtc {

class RTCCreateSessionDescriptionObserver
    : public CreateSessionDescriptionObserver {
 public:
  RTCCreateSessionDescriptionObserver(
      id<RTCSessionDescriptionDelegate> delegate,
      RTCPeerConnection* peerConnection) {
    _delegate = delegate;
    _peerConnection = peerConnection;
  }

  virtual void OnSuccess(SessionDescriptionInterface* desc) OVERRIDE {
    RTCSessionDescription* session =
        [[RTCSessionDescription alloc] initWithSessionDescription:desc];
    [_delegate peerConnection:_peerConnection
        didCreateSessionDescription:session
                              error:nil];
  }

  virtual void OnFailure(const std::string& error) OVERRIDE {
    NSString* str = @(error.c_str());
    NSError* err =
        [NSError errorWithDomain:kRTCSessionDescriptionDelegateErrorDomain
                            code:kRTCSessionDescriptionDelegateErrorCode
                        userInfo:@{@"error" : str}];
    [_delegate peerConnection:_peerConnection
        didCreateSessionDescription:nil
                              error:err];
  }

 private:
  id<RTCSessionDescriptionDelegate> _delegate;
  RTCPeerConnection* _peerConnection;
};

class RTCSetSessionDescriptionObserver : public SetSessionDescriptionObserver {
 public:
  RTCSetSessionDescriptionObserver(id<RTCSessionDescriptionDelegate> delegate,
                                   RTCPeerConnection* peerConnection) {
    _delegate = delegate;
    _peerConnection = peerConnection;
  }

  virtual void OnSuccess() OVERRIDE {
    [_delegate peerConnection:_peerConnection
        didSetSessionDescriptionWithError:nil];
  }

  virtual void OnFailure(const std::string& error) OVERRIDE {
    NSString* str = @(error.c_str());
    NSError* err =
        [NSError errorWithDomain:kRTCSessionDescriptionDelegateErrorDomain
                            code:kRTCSessionDescriptionDelegateErrorCode
                        userInfo:@{@"error" : str}];
    [_delegate peerConnection:_peerConnection
        didSetSessionDescriptionWithError:err];
  }

 private:
  id<RTCSessionDescriptionDelegate> _delegate;
  RTCPeerConnection* _peerConnection;
};

class RTCStatsObserver : public StatsObserver {
 public:
  RTCStatsObserver(id<RTCStatsDelegate> delegate,
                   RTCPeerConnection* peerConnection) {
    _delegate = delegate;
    _peerConnection = peerConnection;
  }

  virtual void OnComplete(const std::vector<StatsReport>& reports) OVERRIDE {
    NSMutableArray* stats = [NSMutableArray arrayWithCapacity:reports.size()];
    std::vector<StatsReport>::const_iterator it = reports.begin();
    for (; it != reports.end(); ++it) {
      RTCStatsReport* statsReport =
          [[RTCStatsReport alloc] initWithStatsReport:*it];
      [stats addObject:statsReport];
    }
    [_delegate peerConnection:_peerConnection didGetStats:stats];
  }

 private:
  id<RTCStatsDelegate> _delegate;
  RTCPeerConnection* _peerConnection;
};
}

@implementation RTCPeerConnection {
  NSMutableArray* _localStreams;
  talk_base::scoped_ptr<webrtc::RTCPeerConnectionObserver> _observer;
  talk_base::scoped_refptr<webrtc::PeerConnectionInterface> _peerConnection;
}

- (BOOL)addICECandidate:(RTCICECandidate*)candidate {
  talk_base::scoped_ptr<const webrtc::IceCandidateInterface> iceCandidate(
      candidate.candidate);
  return self.peerConnection->AddIceCandidate(iceCandidate.get());
}

- (BOOL)addStream:(RTCMediaStream*)stream
      constraints:(RTCMediaConstraints*)constraints {
  BOOL ret = self.peerConnection->AddStream(stream.mediaStream,
                                            constraints.constraints);
  if (!ret) {
    return NO;
  }
  [_localStreams addObject:stream];
  return YES;
}

- (RTCDataChannel*)createDataChannelWithLabel:(NSString*)label
                                       config:(RTCDataChannelInit*)config {
  std::string labelString([label UTF8String]);
  talk_base::scoped_refptr<webrtc::DataChannelInterface> dataChannel =
      self.peerConnection->CreateDataChannel(labelString,
                                             config.dataChannelInit);
  return [[RTCDataChannel alloc] initWithDataChannel:dataChannel];
}

- (void)createAnswerWithDelegate:(id<RTCSessionDescriptionDelegate>)delegate
                     constraints:(RTCMediaConstraints*)constraints {
  talk_base::scoped_refptr<webrtc::RTCCreateSessionDescriptionObserver>
      observer(new talk_base::RefCountedObject<
          webrtc::RTCCreateSessionDescriptionObserver>(delegate, self));
  self.peerConnection->CreateAnswer(observer, constraints.constraints);
}

- (void)createOfferWithDelegate:(id<RTCSessionDescriptionDelegate>)delegate
                    constraints:(RTCMediaConstraints*)constraints {
  talk_base::scoped_refptr<webrtc::RTCCreateSessionDescriptionObserver>
      observer(new talk_base::RefCountedObject<
          webrtc::RTCCreateSessionDescriptionObserver>(delegate, self));
  self.peerConnection->CreateOffer(observer, constraints.constraints);
}

- (void)removeStream:(RTCMediaStream*)stream {
  self.peerConnection->RemoveStream(stream.mediaStream);
  [_localStreams removeObject:stream];
}

- (void)setLocalDescriptionWithDelegate:
            (id<RTCSessionDescriptionDelegate>)delegate
                     sessionDescription:(RTCSessionDescription*)sdp {
  talk_base::scoped_refptr<webrtc::RTCSetSessionDescriptionObserver> observer(
      new talk_base::RefCountedObject<webrtc::RTCSetSessionDescriptionObserver>(
          delegate, self));
  self.peerConnection->SetLocalDescription(observer, sdp.sessionDescription);
}

- (void)setRemoteDescriptionWithDelegate:
            (id<RTCSessionDescriptionDelegate>)delegate
                      sessionDescription:(RTCSessionDescription*)sdp {
  talk_base::scoped_refptr<webrtc::RTCSetSessionDescriptionObserver> observer(
      new talk_base::RefCountedObject<webrtc::RTCSetSessionDescriptionObserver>(
          delegate, self));
  self.peerConnection->SetRemoteDescription(observer, sdp.sessionDescription);
}

- (BOOL)updateICEServers:(NSArray*)servers
             constraints:(RTCMediaConstraints*)constraints {
  webrtc::PeerConnectionInterface::IceServers iceServers;
  for (RTCICEServer* server in servers) {
    iceServers.push_back(server.iceServer);
  }
  return self.peerConnection->UpdateIce(iceServers, constraints.constraints);
}

- (RTCSessionDescription*)localDescription {
  const webrtc::SessionDescriptionInterface* sdi =
      self.peerConnection->local_description();
  return sdi ? [[RTCSessionDescription alloc] initWithSessionDescription:sdi]
             : nil;
}

- (NSArray*)localStreams {
  return [_localStreams copy];
}

- (RTCSessionDescription*)remoteDescription {
  const webrtc::SessionDescriptionInterface* sdi =
      self.peerConnection->remote_description();
  return sdi ? [[RTCSessionDescription alloc] initWithSessionDescription:sdi]
             : nil;
}

- (RTCICEConnectionState)iceConnectionState {
  return [RTCEnumConverter
      convertIceConnectionStateToObjC:self.peerConnection
                                          ->ice_connection_state()];
}

- (RTCICEGatheringState)iceGatheringState {
  return [RTCEnumConverter
      convertIceGatheringStateToObjC:self.peerConnection
                                         ->ice_gathering_state()];
}

- (RTCSignalingState)signalingState {
  return [RTCEnumConverter
      convertSignalingStateToObjC:self.peerConnection->signaling_state()];
}

- (void)close {
  self.peerConnection->Close();
}

- (BOOL)getStatsWithDelegate:(id<RTCStatsDelegate>)delegate
            mediaStreamTrack:(RTCMediaStreamTrack*)mediaStreamTrack
            statsOutputLevel:(RTCStatsOutputLevel)statsOutputLevel {
  talk_base::scoped_refptr<webrtc::RTCStatsObserver> observer(
      new talk_base::RefCountedObject<webrtc::RTCStatsObserver>(delegate,
                                                                self));
  webrtc::PeerConnectionInterface::StatsOutputLevel nativeOutputLevel =
      [RTCEnumConverter convertStatsOutputLevelToNative:statsOutputLevel];
  return self.peerConnection->GetStats(
      observer, mediaStreamTrack.mediaTrack, nativeOutputLevel);
}

@end

@implementation RTCPeerConnection (Internal)

- (id)initWithPeerConnection:
          (talk_base::scoped_refptr<webrtc::PeerConnectionInterface>)
      peerConnection
                    observer:(webrtc::RTCPeerConnectionObserver*)observer {
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
