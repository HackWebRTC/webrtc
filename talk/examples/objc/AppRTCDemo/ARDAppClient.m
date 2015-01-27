/*
 * libjingle
 * Copyright 2014 Google Inc.
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

#import "ARDAppClient+Internal.h"

#import <AVFoundation/AVFoundation.h>

#import "ARDAppEngineClient.h"
#import "ARDCEODTURNClient.h"
#import "ARDJoinResponse.h"
#import "ARDMessageResponse.h"
#import "ARDSignalingMessage.h"
#import "ARDUtilities.h"
#import "ARDWebSocketChannel.h"
#import "RTCICECandidate+JSON.h"
#import "RTCICEServer.h"
#import "RTCMediaConstraints.h"
#import "RTCMediaStream.h"
#import "RTCPair.h"
#import "RTCSessionDescription+JSON.h"
#import "RTCVideoCapturer.h"
#import "RTCVideoTrack.h"

static NSString *kARDDefaultSTUNServerUrl =
    @"stun:stun.l.google.com:19302";
// TODO(tkchin): figure out a better username for CEOD statistics.
static NSString *kARDTurnRequestUrl =
    @"https://computeengineondemand.appspot.com"
    @"/turn?username=iapprtc&key=4080218913";

static NSString *kARDAppClientErrorDomain = @"ARDAppClient";
static NSInteger kARDAppClientErrorUnknown = -1;
static NSInteger kARDAppClientErrorRoomFull = -2;
static NSInteger kARDAppClientErrorCreateSDP = -3;
static NSInteger kARDAppClientErrorSetSDP = -4;
static NSInteger kARDAppClientErrorInvalidClient = -5;
static NSInteger kARDAppClientErrorInvalidRoom = -6;

@implementation ARDAppClient

@synthesize delegate = _delegate;
@synthesize state = _state;
@synthesize roomServerClient = _roomServerClient;
@synthesize channel = _channel;
@synthesize turnClient = _turnClient;
@synthesize peerConnection = _peerConnection;
@synthesize factory = _factory;
@synthesize messageQueue = _messageQueue;
@synthesize isTurnComplete = _isTurnComplete;
@synthesize hasReceivedSdp  = _hasReceivedSdp;
@synthesize roomId = _roomId;
@synthesize clientId = _clientId;
@synthesize isInitiator = _isInitiator;
@synthesize iceServers = _iceServers;
@synthesize webSocketURL = _websocketURL;
@synthesize webSocketRestURL = _websocketRestURL;
@synthesize defaultPeerConnectionConstraints =
    _defaultPeerConnectionConstraints;

- (instancetype)initWithDelegate:(id<ARDAppClientDelegate>)delegate {
  if (self = [super init]) {
    _roomServerClient = [[ARDAppEngineClient alloc] init];
    _delegate = delegate;
    NSURL *turnRequestURL = [NSURL URLWithString:kARDTurnRequestUrl];
    _turnClient = [[ARDCEODTURNClient alloc] initWithURL:turnRequestURL];
    [self configure];
  }
  return self;
}

// TODO(tkchin): Provide signaling channel factory interface so we can recreate
// channel if we need to on network failure. Also, make this the default public
// constructor.
- (instancetype)initWithRoomServerClient:(id<ARDRoomServerClient>)rsClient
                        signalingChannel:(id<ARDSignalingChannel>)channel
                              turnClient:(id<ARDTURNClient>)turnClient
                                delegate:(id<ARDAppClientDelegate>)delegate {
  NSParameterAssert(rsClient);
  NSParameterAssert(channel);
  NSParameterAssert(turnClient);
  if (self = [super init]) {
    _roomServerClient = rsClient;
    _channel = channel;
    _turnClient = turnClient;
    _delegate = delegate;
    [self configure];
  }
  return self;
}

- (void)configure {
  _factory = [[RTCPeerConnectionFactory alloc] init];
  _messageQueue = [NSMutableArray array];
  _iceServers = [NSMutableArray arrayWithObject:[self defaultSTUNServer]];
}

- (void)dealloc {
  [self disconnect];
}

- (void)setState:(ARDAppClientState)state {
  if (_state == state) {
    return;
  }
  _state = state;
  [_delegate appClient:self didChangeState:_state];
}

- (void)connectToRoomWithId:(NSString *)roomId
                    options:(NSDictionary *)options {
  NSParameterAssert(roomId.length);
  NSParameterAssert(_state == kARDAppClientStateDisconnected);
  self.state = kARDAppClientStateConnecting;

  // Request TURN.
  __weak ARDAppClient *weakSelf = self;
  [_turnClient requestServersWithCompletionHandler:^(NSArray *turnServers,
                                                     NSError *error) {
    if (error) {
      NSLog(@"Error retrieving TURN servers: %@", error);
    }
    ARDAppClient *strongSelf = weakSelf;
    [strongSelf.iceServers addObjectsFromArray:turnServers];
    strongSelf.isTurnComplete = YES;
    [strongSelf startSignalingIfReady];
  }];

  // Join room on room server.
  [_roomServerClient joinRoomWithRoomId:roomId
      completionHandler:^(ARDJoinResponse *response, NSError *error) {
    ARDAppClient *strongSelf = weakSelf;
    if (error) {
      [strongSelf.delegate appClient:strongSelf didError:error];
      return;
    }
    NSError *joinError =
        [[strongSelf class] errorForJoinResultType:response.result];
    if (joinError) {
      NSLog(@"Failed to join room:%@ on room server.", roomId);
      [strongSelf disconnect];
      [strongSelf.delegate appClient:strongSelf didError:joinError];
      return;
    }
    NSLog(@"Joined room:%@ on room server.", roomId);
    strongSelf.roomId = response.roomId;
    strongSelf.clientId = response.clientId;
    strongSelf.isInitiator = response.isInitiator;
    for (ARDSignalingMessage *message in response.messages) {
      if (message.type == kARDSignalingMessageTypeOffer ||
          message.type == kARDSignalingMessageTypeAnswer) {
        strongSelf.hasReceivedSdp = YES;
        [strongSelf.messageQueue insertObject:message atIndex:0];
      } else {
        [strongSelf.messageQueue addObject:message];
      }
    }
    strongSelf.webSocketURL = response.webSocketURL;
    strongSelf.webSocketRestURL = response.webSocketRestURL;
    [strongSelf registerWithColliderIfReady];
    [strongSelf startSignalingIfReady];
  }];
}

- (void)disconnect {
  if (_state == kARDAppClientStateDisconnected) {
    return;
  }
  if (self.hasJoinedRoomServerRoom) {
    [_roomServerClient leaveRoomWithRoomId:_roomId
                                  clientId:_clientId
                         completionHandler:nil];
  }
  if (_channel) {
    if (_channel.state == kARDSignalingChannelStateRegistered) {
      // Tell the other client we're hanging up.
      ARDByeMessage *byeMessage = [[ARDByeMessage alloc] init];
      [_channel sendMessage:byeMessage];
    }
    // Disconnect from collider.
    _channel = nil;
  }
  _clientId = nil;
  _roomId = nil;
  _isInitiator = NO;
  _hasReceivedSdp = NO;
  _messageQueue = [NSMutableArray array];
  _peerConnection = nil;
  self.state = kARDAppClientStateDisconnected;
}

#pragma mark - ARDSignalingChannelDelegate

- (void)channel:(id<ARDSignalingChannel>)channel
    didReceiveMessage:(ARDSignalingMessage *)message {
  switch (message.type) {
    case kARDSignalingMessageTypeOffer:
    case kARDSignalingMessageTypeAnswer:
      _hasReceivedSdp = YES;
      [_messageQueue insertObject:message atIndex:0];
      break;
    case kARDSignalingMessageTypeCandidate:
      [_messageQueue addObject:message];
      break;
    case kARDSignalingMessageTypeBye:
      [self processSignalingMessage:message];
      return;
  }
  [self drainMessageQueueIfReady];
}

- (void)channel:(id<ARDSignalingChannel>)channel
    didChangeState:(ARDSignalingChannelState)state {
  switch (state) {
    case kARDSignalingChannelStateOpen:
      break;
    case kARDSignalingChannelStateRegistered:
      break;
    case kARDSignalingChannelStateClosed:
    case kARDSignalingChannelStateError:
      // TODO(tkchin): reconnection scenarios. Right now we just disconnect
      // completely if the websocket connection fails.
      [self disconnect];
      break;
  }
}

#pragma mark - RTCPeerConnectionDelegate

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    signalingStateChanged:(RTCSignalingState)stateChanged {
  NSLog(@"Signaling state changed: %d", stateChanged);
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
           addedStream:(RTCMediaStream *)stream {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSLog(@"Received %lu video tracks and %lu audio tracks",
        (unsigned long)stream.videoTracks.count,
        (unsigned long)stream.audioTracks.count);
    if (stream.videoTracks.count) {
      RTCVideoTrack *videoTrack = stream.videoTracks[0];
      [_delegate appClient:self didReceiveRemoteVideoTrack:videoTrack];
    }
  });
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
        removedStream:(RTCMediaStream *)stream {
  NSLog(@"Stream was removed.");
}

- (void)peerConnectionOnRenegotiationNeeded:
    (RTCPeerConnection *)peerConnection {
  NSLog(@"WARNING: Renegotiation needed but unimplemented.");
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    iceConnectionChanged:(RTCICEConnectionState)newState {
  NSLog(@"ICE state changed: %d", newState);
  dispatch_async(dispatch_get_main_queue(), ^{
    [_delegate appClient:self didChangeConnectionState:newState];
  });
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    iceGatheringChanged:(RTCICEGatheringState)newState {
  NSLog(@"ICE gathering state changed: %d", newState);
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
       gotICECandidate:(RTCICECandidate *)candidate {
  dispatch_async(dispatch_get_main_queue(), ^{
    ARDICECandidateMessage *message =
        [[ARDICECandidateMessage alloc] initWithCandidate:candidate];
    [self sendSignalingMessage:message];
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
    didOpenDataChannel:(RTCDataChannel*)dataChannel {
}

#pragma mark - RTCSessionDescriptionDelegate

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didCreateSessionDescription:(RTCSessionDescription *)sdp
                          error:(NSError *)error {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (error) {
      NSLog(@"Failed to create session description. Error: %@", error);
      [self disconnect];
      NSDictionary *userInfo = @{
        NSLocalizedDescriptionKey: @"Failed to create session description.",
      };
      NSError *sdpError =
          [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                     code:kARDAppClientErrorCreateSDP
                                 userInfo:userInfo];
      [_delegate appClient:self didError:sdpError];
      return;
    }
    [_peerConnection setLocalDescriptionWithDelegate:self
                                  sessionDescription:sdp];
    ARDSessionDescriptionMessage *message =
        [[ARDSessionDescriptionMessage alloc] initWithDescription:sdp];
    [self sendSignalingMessage:message];
  });
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didSetSessionDescriptionWithError:(NSError *)error {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (error) {
      NSLog(@"Failed to set session description. Error: %@", error);
      [self disconnect];
      NSDictionary *userInfo = @{
        NSLocalizedDescriptionKey: @"Failed to set session description.",
      };
      NSError *sdpError =
          [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                     code:kARDAppClientErrorSetSDP
                                 userInfo:userInfo];
      [_delegate appClient:self didError:sdpError];
      return;
    }
    // If we're answering and we've just set the remote offer we need to create
    // an answer and set the local description.
    if (!_isInitiator && !_peerConnection.localDescription) {
      RTCMediaConstraints *constraints = [self defaultAnswerConstraints];
      [_peerConnection createAnswerWithDelegate:self
                                    constraints:constraints];

    }
  });
}

#pragma mark - Private

- (BOOL)hasJoinedRoomServerRoom {
  return _clientId.length;
}

- (void)startSignalingIfReady {
  if (!_isTurnComplete || !self.hasJoinedRoomServerRoom) {
    return;
  }
  self.state = kARDAppClientStateConnected;

  // Create peer connection.
  RTCMediaConstraints *constraints = [self defaultPeerConnectionConstraints];
  _peerConnection = [_factory peerConnectionWithICEServers:_iceServers
                                               constraints:constraints
                                                  delegate:self];
  RTCMediaStream *localStream = [self createLocalMediaStream];
  [_peerConnection addStream:localStream];
  if (_isInitiator) {
    [self sendOffer];
  } else {
    [self waitForAnswer];
  }
}

- (void)sendOffer {
  [_peerConnection createOfferWithDelegate:self
                               constraints:[self defaultOfferConstraints]];
}

- (void)waitForAnswer {
  [self drainMessageQueueIfReady];
}

- (void)drainMessageQueueIfReady {
  if (!_peerConnection || !_hasReceivedSdp) {
    return;
  }
  for (ARDSignalingMessage *message in _messageQueue) {
    [self processSignalingMessage:message];
  }
  [_messageQueue removeAllObjects];
}

- (void)processSignalingMessage:(ARDSignalingMessage *)message {
  NSParameterAssert(_peerConnection ||
      message.type == kARDSignalingMessageTypeBye);
  switch (message.type) {
    case kARDSignalingMessageTypeOffer:
    case kARDSignalingMessageTypeAnswer: {
      ARDSessionDescriptionMessage *sdpMessage =
          (ARDSessionDescriptionMessage *)message;
      RTCSessionDescription *description = sdpMessage.sessionDescription;
      [_peerConnection setRemoteDescriptionWithDelegate:self
                                     sessionDescription:description];
      break;
    }
    case kARDSignalingMessageTypeCandidate: {
      ARDICECandidateMessage *candidateMessage =
          (ARDICECandidateMessage *)message;
      [_peerConnection addICECandidate:candidateMessage.candidate];
      break;
    }
    case kARDSignalingMessageTypeBye:
      // Other client disconnected.
      // TODO(tkchin): support waiting in room for next client. For now just
      // disconnect.
      [self disconnect];
      break;
  }
}

- (void)sendSignalingMessage:(ARDSignalingMessage *)message {
  if (_isInitiator) {
    __weak ARDAppClient *weakSelf = self;
    [_roomServerClient sendMessage:message
                         forRoomId:_roomId
                          clientId:_clientId
                 completionHandler:^(ARDMessageResponse *response,
                                     NSError *error) {
      ARDAppClient *strongSelf = weakSelf;
      if (error) {
        [strongSelf.delegate appClient:strongSelf didError:error];
        return;
      }
      NSError *messageError =
          [[strongSelf class] errorForMessageResultType:response.result];
      if (messageError) {
        [strongSelf.delegate appClient:strongSelf didError:messageError];
        return;
      }
    }];
  } else {
    [_channel sendMessage:message];
  }
}

- (RTCMediaStream *)createLocalMediaStream {
  RTCMediaStream* localStream = [_factory mediaStreamWithLabel:@"ARDAMS"];
  RTCVideoTrack* localVideoTrack = nil;

  // The iOS simulator doesn't provide any sort of camera capture
  // support or emulation (http://goo.gl/rHAnC1) so don't bother
  // trying to open a local stream.
  // TODO(tkchin): local video capture for OSX. See
  // https://code.google.com/p/webrtc/issues/detail?id=3417.
#if !TARGET_IPHONE_SIMULATOR && TARGET_OS_IPHONE
  NSString *cameraID = nil;
  for (AVCaptureDevice *captureDevice in
       [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo]) {
    if (captureDevice.position == AVCaptureDevicePositionFront) {
      cameraID = [captureDevice localizedName];
      break;
    }
  }
  NSAssert(cameraID, @"Unable to get the front camera id");

  RTCVideoCapturer *capturer =
      [RTCVideoCapturer capturerWithDeviceName:cameraID];
  RTCMediaConstraints *mediaConstraints = [self defaultMediaStreamConstraints];
  RTCVideoSource *videoSource =
      [_factory videoSourceWithCapturer:capturer
                            constraints:mediaConstraints];
  localVideoTrack =
      [_factory videoTrackWithID:@"ARDAMSv0" source:videoSource];
  if (localVideoTrack) {
    [localStream addVideoTrack:localVideoTrack];
  }
  [_delegate appClient:self didReceiveLocalVideoTrack:localVideoTrack];
#endif
  [localStream addAudioTrack:[_factory audioTrackWithID:@"ARDAMSa0"]];
  return localStream;
}

#pragma mark - Collider methods

- (void)registerWithColliderIfReady {
  if (!self.hasJoinedRoomServerRoom) {
    return;
  }
  // Open WebSocket connection.
  if (!_channel) {
    _channel =
        [[ARDWebSocketChannel alloc] initWithURL:_websocketURL
                                         restURL:_websocketRestURL
                                        delegate:self];
  }
  [_channel registerForRoomId:_roomId clientId:_clientId];
}

#pragma mark - Defaults

- (RTCMediaConstraints *)defaultMediaStreamConstraints {
  RTCMediaConstraints* constraints =
      [[RTCMediaConstraints alloc]
          initWithMandatoryConstraints:nil
                   optionalConstraints:nil];
  return constraints;
}

- (RTCMediaConstraints *)defaultAnswerConstraints {
  return [self defaultOfferConstraints];
}

- (RTCMediaConstraints *)defaultOfferConstraints {
  NSArray *mandatoryConstraints = @[
      [[RTCPair alloc] initWithKey:@"OfferToReceiveAudio" value:@"true"],
      [[RTCPair alloc] initWithKey:@"OfferToReceiveVideo" value:@"true"]
  ];
  RTCMediaConstraints* constraints =
      [[RTCMediaConstraints alloc]
          initWithMandatoryConstraints:mandatoryConstraints
                   optionalConstraints:nil];
  return constraints;
}

- (RTCMediaConstraints *)defaultPeerConnectionConstraints {
  if (_defaultPeerConnectionConstraints) {
    return _defaultPeerConnectionConstraints;
  }
  NSArray *optionalConstraints = @[
      [[RTCPair alloc] initWithKey:@"DtlsSrtpKeyAgreement" value:@"true"]
  ];
  RTCMediaConstraints* constraints =
      [[RTCMediaConstraints alloc]
          initWithMandatoryConstraints:nil
                   optionalConstraints:optionalConstraints];
  return constraints;
}

- (RTCICEServer *)defaultSTUNServer {
  NSURL *defaultSTUNServerURL = [NSURL URLWithString:kARDDefaultSTUNServerUrl];
  return [[RTCICEServer alloc] initWithURI:defaultSTUNServerURL
                                  username:@""
                                  password:@""];
}

#pragma mark - Errors

+ (NSError *)errorForJoinResultType:(ARDJoinResultType)resultType {
  NSError *error = nil;
  switch (resultType) {
    case kARDJoinResultTypeSuccess:
      break;
    case kARDJoinResultTypeUnknown: {
      error = [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                         code:kARDAppClientErrorUnknown
                                     userInfo:@{
        NSLocalizedDescriptionKey: @"Unknown error.",
      }];
      break;
    }
    case kARDJoinResultTypeFull: {
      error = [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                         code:kARDAppClientErrorRoomFull
                                     userInfo:@{
        NSLocalizedDescriptionKey: @"Room is full.",
      }];
      break;
    }
  }
  return error;
}

+ (NSError *)errorForMessageResultType:(ARDMessageResultType)resultType {
  NSError *error = nil;
  switch (resultType) {
    case kARDMessageResultTypeSuccess:
      break;
    case kARDMessageResultTypeUnknown:
      error = [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                         code:kARDAppClientErrorUnknown
                                     userInfo:@{
        NSLocalizedDescriptionKey: @"Unknown error.",
      }];
      break;
    case kARDMessageResultTypeInvalidClient:
      error = [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                         code:kARDAppClientErrorInvalidClient
                                     userInfo:@{
        NSLocalizedDescriptionKey: @"Invalid client.",
      }];
      break;
    case kARDMessageResultTypeInvalidRoom:
      error = [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                         code:kARDAppClientErrorInvalidRoom
                                     userInfo:@{
        NSLocalizedDescriptionKey: @"Invalid room.",
      }];
      break;
  }
  return error;
}

@end
