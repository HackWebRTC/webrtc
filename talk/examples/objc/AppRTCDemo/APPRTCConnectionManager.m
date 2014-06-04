/*
 * libjingle
 * Copyright 2014, Google Inc.
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

#import "APPRTCConnectionManager.h"

#import <AVFoundation/AVFoundation.h>
#import "APPRTCAppClient.h"
#import "GAEChannelClient.h"
#import "RTCICECandidate.h"
#import "RTCMediaConstraints.h"
#import "RTCMediaStream.h"
#import "RTCPair.h"
#import "RTCPeerConnection.h"
#import "RTCPeerConnectionDelegate.h"
#import "RTCPeerConnectionFactory.h"
#import "RTCSessionDescription.h"
#import "RTCSessionDescriptionDelegate.h"
#import "RTCStatsDelegate.h"
#import "RTCVideoCapturer.h"
#import "RTCVideoSource.h"

@interface APPRTCConnectionManager ()
    <APPRTCAppClientDelegate, GAEMessageHandler, RTCPeerConnectionDelegate,
     RTCSessionDescriptionDelegate, RTCStatsDelegate>

@property(nonatomic, strong) APPRTCAppClient* client;
@property(nonatomic, strong) RTCPeerConnection* peerConnection;
@property(nonatomic, strong) RTCPeerConnectionFactory* peerConnectionFactory;
@property(nonatomic, strong) RTCVideoSource* videoSource;
@property(nonatomic, strong) NSMutableArray* queuedRemoteCandidates;

@end

@implementation APPRTCConnectionManager {
  NSTimer* _statsTimer;
}

- (instancetype)initWithDelegate:(id<APPRTCConnectionManagerDelegate>)delegate
                          logger:(id<APPRTCLogger>)logger {
  if (self = [super init]) {
    self.delegate = delegate;
    self.logger = logger;
    self.peerConnectionFactory = [[RTCPeerConnectionFactory alloc] init];
    // TODO(tkchin): turn this into a button.
    // Uncomment for stat logs.
    // _statsTimer =
    //     [NSTimer scheduledTimerWithTimeInterval:10
    //                                      target:self
    //                                    selector:@selector(didFireStatsTimer:)
    //                                    userInfo:nil
    //                                     repeats:YES];
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

- (BOOL)connectToRoomWithURL:(NSURL*)url {
  if (self.client) {
    // Already have a connection.
    return NO;
  }
  self.client = [[APPRTCAppClient alloc] initWithDelegate:self
                                           messageHandler:self];
  [self.client connectToRoom:url];
  return YES;
}

- (void)disconnect {
  if (!self.client) {
    return;
  }
  [self.client
      sendData:[@"{\"type\": \"bye\"}" dataUsingEncoding:NSUTF8StringEncoding]];
  [self.peerConnection close];
  self.peerConnection = nil;
  self.client = nil;
  self.videoSource = nil;
  self.queuedRemoteCandidates = nil;
}

#pragma mark - APPRTCAppClientDelegate

- (void)appClient:(APPRTCAppClient*)appClient
    didErrorWithMessage:(NSString*)message {
  [self.delegate connectionManager:self
               didErrorWithMessage:message];
}

- (void)appClient:(APPRTCAppClient*)appClient
    didReceiveICEServers:(NSArray*)servers {
  self.queuedRemoteCandidates = [NSMutableArray array];
  RTCMediaConstraints* constraints = [[RTCMediaConstraints alloc]
      initWithMandatoryConstraints:
          @[
            [[RTCPair alloc] initWithKey:@"OfferToReceiveAudio" value:@"true"],
            [[RTCPair alloc] initWithKey:@"OfferToReceiveVideo" value:@"true"]
          ]
               optionalConstraints:
                   @[
                     [[RTCPair alloc] initWithKey:@"internalSctpDataChannels"
                                            value:@"true"],
                     [[RTCPair alloc] initWithKey:@"DtlsSrtpKeyAgreement"
                                            value:@"true"]
                   ]];
  self.peerConnection =
      [self.peerConnectionFactory peerConnectionWithICEServers:servers
                                                   constraints:constraints
                                                      delegate:self];
  RTCMediaStream* lms =
      [self.peerConnectionFactory mediaStreamWithLabel:@"ARDAMS"];

  // The iOS simulator doesn't provide any sort of camera capture
  // support or emulation (http://goo.gl/rHAnC1) so don't bother
  // trying to open a local stream.
  RTCVideoTrack* localVideoTrack;

  // TODO(tkchin): local video capture for OSX. See
  // https://code.google.com/p/webrtc/issues/detail?id=3417.
#if !TARGET_IPHONE_SIMULATOR && TARGET_OS_IPHONE
  NSString* cameraID = nil;
  for (AVCaptureDevice* captureDevice in
       [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo]) {
    if (captureDevice.position == AVCaptureDevicePositionFront) {
      cameraID = [captureDevice localizedName];
      break;
    }
  }
  NSAssert(cameraID, @"Unable to get the front camera id");

  RTCVideoCapturer* capturer =
      [RTCVideoCapturer capturerWithDeviceName:cameraID];
  self.videoSource = [self.peerConnectionFactory
      videoSourceWithCapturer:capturer
                  constraints:self.client.videoConstraints];
  localVideoTrack =
      [self.peerConnectionFactory videoTrackWithID:@"ARDAMSv0"
                                            source:self.videoSource];
  if (localVideoTrack) {
    [lms addVideoTrack:localVideoTrack];
  }
  [self.delegate connectionManager:self
         didReceiveLocalVideoTrack:localVideoTrack];
#endif

  [lms addAudioTrack:[self.peerConnectionFactory audioTrackWithID:@"ARDAMSa0"]];
  [self.peerConnection addStream:lms constraints:constraints];
  [self.logger logMessage:@"onICEServers - added local stream."];
}

#pragma mark - GAEMessageHandler methods

- (void)onOpen {
  if (!self.client.initiator) {
    [self.logger logMessage:@"Callee; waiting for remote offer"];
    return;
  }
  [self.logger logMessage:@"GAE onOpen - create offer."];
  RTCPair* audio =
      [[RTCPair alloc] initWithKey:@"OfferToReceiveAudio" value:@"true"];
  RTCPair* video =
      [[RTCPair alloc] initWithKey:@"OfferToReceiveVideo" value:@"true"];
  NSArray* mandatory = @[ audio, video ];
  RTCMediaConstraints* constraints =
      [[RTCMediaConstraints alloc] initWithMandatoryConstraints:mandatory
                                            optionalConstraints:nil];
  [self.peerConnection createOfferWithDelegate:self constraints:constraints];
  [self.logger logMessage:@"PC - createOffer."];
}

- (void)onMessage:(NSDictionary*)messageData {
  NSString* type = messageData[@"type"];
  NSAssert(type, @"Missing type: %@", messageData);
  [self.logger logMessage:[NSString stringWithFormat:@"GAE onMessage type - %@",
                                                      type]];
  if ([type isEqualToString:@"candidate"]) {
    NSString* mid = messageData[@"id"];
    NSNumber* sdpLineIndex = messageData[@"label"];
    NSString* sdp = messageData[@"candidate"];
    RTCICECandidate* candidate =
        [[RTCICECandidate alloc] initWithMid:mid
                                       index:sdpLineIndex.intValue
                                         sdp:sdp];
    if (self.queuedRemoteCandidates) {
      [self.queuedRemoteCandidates addObject:candidate];
    } else {
      [self.peerConnection addICECandidate:candidate];
    }
  } else if ([type isEqualToString:@"offer"] ||
             [type isEqualToString:@"answer"]) {
    NSString* sdpString = messageData[@"sdp"];
    RTCSessionDescription* sdp = [[RTCSessionDescription alloc]
        initWithType:type
                 sdp:[[self class] preferISAC:sdpString]];
    [self.peerConnection setRemoteDescriptionWithDelegate:self
                                       sessionDescription:sdp];
    [self.logger logMessage:@"PC - setRemoteDescription."];
  } else if ([type isEqualToString:@"bye"]) {
    [self.delegate connectionManagerDidReceiveHangup:self];
  } else {
    NSAssert(NO, @"Invalid message: %@", messageData);
  }
}

- (void)onClose {
  [self.logger logMessage:@"GAE onClose."];
  [self.delegate connectionManagerDidReceiveHangup:self];
}

- (void)onError:(int)code withDescription:(NSString*)description {
  NSString* message = [NSString stringWithFormat:@"GAE onError: %d, %@",
                                code, description];
  [self.logger logMessage:message];
  [self.delegate connectionManager:self
               didErrorWithMessage:message];
}

#pragma mark - RTCPeerConnectionDelegate

- (void)peerConnectionOnError:(RTCPeerConnection*)peerConnection {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSString* message = @"PeerConnection error";
    NSLog(@"%@", message);
    NSAssert(NO, @"PeerConnection failed.");
    [self.delegate connectionManager:self
                 didErrorWithMessage:message];
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
    signalingStateChanged:(RTCSignalingState)stateChanged {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSLog(@"PCO onSignalingStateChange: %d", stateChanged);
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
           addedStream:(RTCMediaStream*)stream {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSLog(@"PCO onAddStream.");
    NSAssert([stream.audioTracks count] == 1 || [stream.videoTracks count] == 1,
             @"Expected audio or video track");
    NSAssert([stream.audioTracks count] <= 1,
             @"Expected at most 1 audio stream");
    NSAssert([stream.videoTracks count] <= 1,
             @"Expected at most 1 video stream");
    if ([stream.videoTracks count] != 0) {
      [self.delegate connectionManager:self
            didReceiveRemoteVideoTrack:stream.videoTracks[0]];
    }
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
         removedStream:(RTCMediaStream*)stream {
  dispatch_async(dispatch_get_main_queue(),
                 ^{ NSLog(@"PCO onRemoveStream."); });
}

- (void)peerConnectionOnRenegotiationNeeded:(RTCPeerConnection*)peerConnection {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSLog(@"PCO onRenegotiationNeeded - ignoring because AppRTC has a "
           "predefined negotiation strategy");
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
       gotICECandidate:(RTCICECandidate*)candidate {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSLog(@"PCO onICECandidate.\n  Mid[%@] Index[%li] Sdp[%@]",
          candidate.sdpMid,
          (long)candidate.sdpMLineIndex,
          candidate.sdp);
    NSDictionary* json = @{
      @"type" : @"candidate",
      @"label" : @(candidate.sdpMLineIndex),
      @"id" : candidate.sdpMid,
      @"candidate" : candidate.sdp
    };
    NSError* error;
    NSData* data =
        [NSJSONSerialization dataWithJSONObject:json options:0 error:&error];
    if (!error) {
      [self.client sendData:data];
    } else {
      NSAssert(NO,
               @"Unable to serialize JSON object with error: %@",
               error.localizedDescription);
    }
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
    iceGatheringChanged:(RTCICEGatheringState)newState {
  dispatch_async(dispatch_get_main_queue(),
                 ^{ NSLog(@"PCO onIceGatheringChange. %d", newState); });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
    iceConnectionChanged:(RTCICEConnectionState)newState {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSLog(@"PCO onIceConnectionChange. %d", newState);
    if (newState == RTCICEConnectionConnected)
      [self.logger logMessage:@"ICE Connection Connected."];
    NSAssert(newState != RTCICEConnectionFailed, @"ICE Connection failed!");
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
    didOpenDataChannel:(RTCDataChannel*)dataChannel {
  NSAssert(NO, @"AppRTC doesn't use DataChannels");
}

#pragma mark - RTCSessionDescriptionDelegate

- (void)peerConnection:(RTCPeerConnection*)peerConnection
    didCreateSessionDescription:(RTCSessionDescription*)origSdp
                          error:(NSError*)error {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (error) {
      [self.logger logMessage:@"SDP onFailure."];
      NSAssert(NO, error.description);
      return;
    }
    [self.logger logMessage:@"SDP onSuccess(SDP) - set local description."];
    RTCSessionDescription* sdp = [[RTCSessionDescription alloc]
        initWithType:origSdp.type
                 sdp:[[self class] preferISAC:origSdp.description]];
    [self.peerConnection setLocalDescriptionWithDelegate:self
                                      sessionDescription:sdp];
    [self.logger logMessage:@"PC setLocalDescription."];
    NSDictionary* json = @{@"type" : sdp.type, @"sdp" : sdp.description};
    NSError* jsonError;
    NSData* data = [NSJSONSerialization dataWithJSONObject:json
                                                   options:0
                                                     error:&jsonError];
    NSAssert(!jsonError, @"Error: %@", jsonError.description);
    [self.client sendData:data];
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
    didSetSessionDescriptionWithError:(NSError*)error {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (error) {
      [self.logger logMessage:@"SDP onFailure."];
      NSAssert(NO, error.description);
      return;
    }
    [self.logger logMessage:@"SDP onSuccess() - possibly drain candidates"];
    if (!self.client.initiator) {
      if (self.peerConnection.remoteDescription &&
          !self.peerConnection.localDescription) {
        [self.logger logMessage:@"Callee, setRemoteDescription succeeded"];
        RTCPair* audio = [[RTCPair alloc] initWithKey:@"OfferToReceiveAudio"
                                                value:@"true"];
        RTCPair* video = [[RTCPair alloc] initWithKey:@"OfferToReceiveVideo"
                                                value:@"true"];
        NSArray* mandatory = @[ audio, video ];
        RTCMediaConstraints* constraints = [[RTCMediaConstraints alloc]
            initWithMandatoryConstraints:mandatory
                     optionalConstraints:nil];
        [self.peerConnection createAnswerWithDelegate:self
                                          constraints:constraints];
        [self.logger logMessage:@"PC - createAnswer."];
      } else {
        [self.logger logMessage:@"SDP onSuccess - drain candidates"];
        [self drainRemoteCandidates];
      }
    } else {
      if (self.peerConnection.remoteDescription) {
        [self.logger logMessage:@"SDP onSuccess - drain candidates"];
        [self drainRemoteCandidates];
      }
    }
  });
}

#pragma mark - RTCStatsDelegate methods

- (void)peerConnection:(RTCPeerConnection*)peerConnection
           didGetStats:(NSArray*)stats {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSString* message = [NSString stringWithFormat:@"Stats:\n %@", stats];
    [self.logger logMessage:message];
  });
}

#pragma mark - Private

// Match |pattern| to |string| and return the first group of the first
// match, or nil if no match was found.
+ (NSString*)firstMatch:(NSRegularExpression*)pattern
             withString:(NSString*)string {
  NSTextCheckingResult* result =
      [pattern firstMatchInString:string
                          options:0
                            range:NSMakeRange(0, [string length])];
  if (!result)
    return nil;
  return [string substringWithRange:[result rangeAtIndex:1]];
}

// Mangle |origSDP| to prefer the ISAC/16k audio codec.
+ (NSString*)preferISAC:(NSString*)origSDP {
  int mLineIndex = -1;
  NSString* isac16kRtpMap = nil;
  NSArray* lines = [origSDP componentsSeparatedByString:@"\n"];
  NSRegularExpression* isac16kRegex = [NSRegularExpression
      regularExpressionWithPattern:@"^a=rtpmap:(\\d+) ISAC/16000[\r]?$"
                           options:0
                             error:nil];
  for (int i = 0;
       (i < [lines count]) && (mLineIndex == -1 || isac16kRtpMap == nil);
       ++i) {
    NSString* line = [lines objectAtIndex:i];
    if ([line hasPrefix:@"m=audio "]) {
      mLineIndex = i;
      continue;
    }
    isac16kRtpMap = [self firstMatch:isac16kRegex withString:line];
  }
  if (mLineIndex == -1) {
    NSLog(@"No m=audio line, so can't prefer iSAC");
    return origSDP;
  }
  if (isac16kRtpMap == nil) {
    NSLog(@"No ISAC/16000 line, so can't prefer iSAC");
    return origSDP;
  }
  NSArray* origMLineParts =
      [[lines objectAtIndex:mLineIndex] componentsSeparatedByString:@" "];
  NSMutableArray* newMLine =
      [NSMutableArray arrayWithCapacity:[origMLineParts count]];
  int origPartIndex = 0;
  // Format is: m=<media> <port> <proto> <fmt> ...
  [newMLine addObject:[origMLineParts objectAtIndex:origPartIndex++]];
  [newMLine addObject:[origMLineParts objectAtIndex:origPartIndex++]];
  [newMLine addObject:[origMLineParts objectAtIndex:origPartIndex++]];
  [newMLine addObject:isac16kRtpMap];
  for (; origPartIndex < [origMLineParts count]; ++origPartIndex) {
    if (![isac16kRtpMap
            isEqualToString:[origMLineParts objectAtIndex:origPartIndex]]) {
      [newMLine addObject:[origMLineParts objectAtIndex:origPartIndex]];
    }
  }
  NSMutableArray* newLines = [NSMutableArray arrayWithCapacity:[lines count]];
  [newLines addObjectsFromArray:lines];
  [newLines replaceObjectAtIndex:mLineIndex
                      withObject:[newMLine componentsJoinedByString:@" "]];
  return [newLines componentsJoinedByString:@"\n"];
}

- (void)drainRemoteCandidates {
  for (RTCICECandidate* candidate in self.queuedRemoteCandidates) {
    [self.peerConnection addICECandidate:candidate];
  }
  self.queuedRemoteCandidates = nil;
}

- (void)didFireStatsTimer:(NSTimer*)timer {
  if (self.peerConnection) {
    [self.peerConnection getStatsWithDelegate:self
                             mediaStreamTrack:nil
                             statsOutputLevel:RTCStatsOutputLevelDebug];
  }
}

@end
