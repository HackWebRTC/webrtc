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

#import <AVFoundation/AVFoundation.h>

#import "APPRTCAppDelegate.h"

#import "APPRTCViewController.h"
#import "RTCICECandidate.h"
#import "RTCICEServer.h"
#import "RTCMediaConstraints.h"
#import "RTCMediaStream.h"
#import "RTCPair.h"
#import "RTCPeerConnection.h"
#import "RTCPeerConnectionDelegate.h"
#import "RTCPeerConnectionFactory.h"
#import "RTCSessionDescription.h"
#import "RTCStatsDelegate.h"
#import "RTCVideoRenderer.h"
#import "RTCVideoCapturer.h"
#import "RTCVideoTrack.h"
#import "APPRTCVideoView.h"

@interface PCObserver : NSObject<RTCPeerConnectionDelegate>

- (id)initWithDelegate:(id<APPRTCSendMessage>)delegate;

@property(nonatomic, strong) APPRTCVideoView* videoView;

@end

@implementation PCObserver {
  id<APPRTCSendMessage> _delegate;
}

- (id)initWithDelegate:(id<APPRTCSendMessage>)delegate {
  if (self = [super init]) {
    _delegate = delegate;
  }
  return self;
}

#pragma mark - RTCPeerConnectionDelegate

- (void)peerConnectionOnError:(RTCPeerConnection*)peerConnection {
  dispatch_async(dispatch_get_main_queue(), ^(void) {
      NSLog(@"PCO onError.");
      NSAssert(NO, @"PeerConnection failed.");
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
    signalingStateChanged:(RTCSignalingState)stateChanged {
  dispatch_async(dispatch_get_main_queue(), ^(void) {
      NSLog(@"PCO onSignalingStateChange: %d", stateChanged);
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
           addedStream:(RTCMediaStream*)stream {
  dispatch_async(dispatch_get_main_queue(), ^(void) {
      NSLog(@"PCO onAddStream.");
      NSAssert([stream.audioTracks count] <= 1,
               @"Expected at most 1 audio stream");
      NSAssert([stream.videoTracks count] <= 1,
               @"Expected at most 1 video stream");
      if ([stream.videoTracks count] != 0) {
        [self.videoView
            renderVideoTrackInterface:[stream.videoTracks objectAtIndex:0]];
      }
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
         removedStream:(RTCMediaStream*)stream {
  dispatch_async(dispatch_get_main_queue(),
                 ^(void) { NSLog(@"PCO onRemoveStream."); });
}

- (void)peerConnectionOnRenegotiationNeeded:(RTCPeerConnection*)peerConnection {
  dispatch_async(dispatch_get_main_queue(), ^(void) {
      NSLog(@"PCO onRenegotiationNeeded - ignoring because AppRTC has a "
             "predefined negotiation strategy");
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
       gotICECandidate:(RTCICECandidate*)candidate {
  dispatch_async(dispatch_get_main_queue(), ^(void) {
      NSLog(@"PCO onICECandidate.\n  Mid[%@] Index[%d] Sdp[%@]",
            candidate.sdpMid,
            candidate.sdpMLineIndex,
            candidate.sdp);
      NSDictionary* json = @{
        @"type" : @"candidate",
        @"label" : [NSNumber numberWithInt:candidate.sdpMLineIndex],
        @"id" : candidate.sdpMid,
        @"candidate" : candidate.sdp
      };
      NSError* error;
      NSData* data =
          [NSJSONSerialization dataWithJSONObject:json options:0 error:&error];
      if (!error) {
        [_delegate sendData:data];
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
                 ^(void) { NSLog(@"PCO onIceGatheringChange. %d", newState); });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
    iceConnectionChanged:(RTCICEConnectionState)newState {
  dispatch_async(dispatch_get_main_queue(), ^(void) {
      NSLog(@"PCO onIceConnectionChange. %d", newState);
      if (newState == RTCICEConnectionConnected)
        [self displayLogMessage:@"ICE Connection Connected."];
      NSAssert(newState != RTCICEConnectionFailed, @"ICE Connection failed!");
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
    didOpenDataChannel:(RTCDataChannel*)dataChannel {
  NSAssert(NO, @"AppRTC doesn't use DataChannels");
}

#pragma mark - Private

- (void)displayLogMessage:(NSString*)message {
  [_delegate displayLogMessage:message];
}

@end

@interface APPRTCAppDelegate () <RTCStatsDelegate>

@property(nonatomic, strong) APPRTCAppClient* client;
@property(nonatomic, strong) PCObserver* pcObserver;
@property(nonatomic, strong) RTCPeerConnection* peerConnection;
@property(nonatomic, strong) RTCPeerConnectionFactory* peerConnectionFactory;
@property(nonatomic, strong) NSMutableArray* queuedRemoteCandidates;

@end

@implementation APPRTCAppDelegate {
  NSTimer* _statsTimer;
}

#pragma mark - UIApplicationDelegate methods

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  [RTCPeerConnectionFactory initializeSSL];
  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  self.viewController =
      [[APPRTCViewController alloc] initWithNibName:@"APPRTCViewController"
                                             bundle:nil];
  self.window.rootViewController = self.viewController;
  _statsTimer =
      [NSTimer scheduledTimerWithTimeInterval:10
                                       target:self
                                     selector:@selector(didFireStatsTimer:)
                                     userInfo:nil
                                      repeats:YES];
  [self.window makeKeyAndVisible];
  return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application {
  [self displayLogMessage:@"Application lost focus, connection broken."];
  [self closeVideoUI];
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
}

- (void)applicationWillTerminate:(UIApplication*)application {
}

- (BOOL)application:(UIApplication*)application
              openURL:(NSURL*)url
    sourceApplication:(NSString*)sourceApplication
           annotation:(id)annotation {
  if (self.client) {
    return NO;
  }
  self.client = [[APPRTCAppClient alloc] initWithICEServerDelegate:self
                                                    messageHandler:self];
  [self.client connectToRoom:url];
  return YES;
}

- (void)displayLogMessage:(NSString*)message {
  NSAssert([NSThread isMainThread], @"Called off main thread!");
  NSLog(@"%@", message);
  [self.viewController displayText:message];
}

#pragma mark - RTCSendMessage method

- (void)sendData:(NSData*)data {
  [self.client sendData:data];
}

#pragma mark - ICEServerDelegate method

- (void)onICEServers:(NSArray*)servers {
  self.queuedRemoteCandidates = [NSMutableArray array];
  self.peerConnectionFactory = [[RTCPeerConnectionFactory alloc] init];
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
  self.pcObserver = [[PCObserver alloc] initWithDelegate:self];
  self.peerConnection =
      [self.peerConnectionFactory peerConnectionWithICEServers:servers
                                                   constraints:constraints
                                                      delegate:self.pcObserver];
  RTCMediaStream* lms =
      [self.peerConnectionFactory mediaStreamWithLabel:@"ARDAMS"];

  // The iOS simulator doesn't provide any sort of camera capture
  // support or emulation (http://goo.gl/rHAnC1) so don't bother
  // trying to open a local stream.
  RTCVideoTrack* localVideoTrack;
#if !TARGET_IPHONE_SIMULATOR
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
#endif

  [self.viewController.localVideoView
      renderVideoTrackInterface:localVideoTrack];

  self.pcObserver.videoView = self.viewController.remoteVideoView;

  [lms addAudioTrack:[self.peerConnectionFactory audioTrackWithID:@"ARDAMSa0"]];
  [self.peerConnection addStream:lms constraints:constraints];
  [self displayLogMessage:@"onICEServers - added local stream."];
}

#pragma mark - GAEMessageHandler methods

- (void)onOpen {
  if (!self.client.initiator) {
    [self displayLogMessage:@"Callee; waiting for remote offer"];
    return;
  }
  [self displayLogMessage:@"GAE onOpen - create offer."];
  RTCPair* audio =
      [[RTCPair alloc] initWithKey:@"OfferToReceiveAudio" value:@"true"];
  RTCPair* video =
      [[RTCPair alloc] initWithKey:@"OfferToReceiveVideo" value:@"true"];
  NSArray* mandatory = @[ audio, video ];
  RTCMediaConstraints* constraints =
      [[RTCMediaConstraints alloc] initWithMandatoryConstraints:mandatory
                                            optionalConstraints:nil];
  [self.peerConnection createOfferWithDelegate:self constraints:constraints];
  [self displayLogMessage:@"PC - createOffer."];
}

- (void)onMessage:(NSDictionary*)messageData {
  NSString* type = messageData[@"type"];
  NSAssert(type, @"Missing type: %@", messageData);
  [self displayLogMessage:[NSString stringWithFormat:@"GAE onMessage type - %@",
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
                 sdp:[APPRTCAppDelegate preferISAC:sdpString]];
    [self.peerConnection setRemoteDescriptionWithDelegate:self
                                       sessionDescription:sdp];
    [self displayLogMessage:@"PC - setRemoteDescription."];
  } else if ([type isEqualToString:@"bye"]) {
    [self closeVideoUI];
    UIAlertView* alertView =
        [[UIAlertView alloc] initWithTitle:@"Remote end hung up"
                                   message:@"dropping PeerConnection"
                                  delegate:nil
                         cancelButtonTitle:@"OK"
                         otherButtonTitles:nil];
    [alertView show];
  } else {
    NSAssert(NO, @"Invalid message: %@", messageData);
  }
}

- (void)onClose {
  [self displayLogMessage:@"GAE onClose."];
  [self closeVideoUI];
}

- (void)onError:(int)code withDescription:(NSString*)description {
  [self displayLogMessage:[NSString stringWithFormat:@"GAE onError: %d, %@",
                                    code, description]];
  [self closeVideoUI];
}

#pragma mark - RTCSessionDescriptionDelegate methods

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

- (void)peerConnection:(RTCPeerConnection*)peerConnection
    didCreateSessionDescription:(RTCSessionDescription*)origSdp
                          error:(NSError*)error {
  dispatch_async(dispatch_get_main_queue(), ^(void) {
      if (error) {
        [self displayLogMessage:@"SDP onFailure."];
        NSAssert(NO, error.description);
        return;
      }
      [self displayLogMessage:@"SDP onSuccess(SDP) - set local description."];
      RTCSessionDescription* sdp = [[RTCSessionDescription alloc]
          initWithType:origSdp.type
                   sdp:[APPRTCAppDelegate preferISAC:origSdp.description]];
      [self.peerConnection setLocalDescriptionWithDelegate:self
                                        sessionDescription:sdp];

      [self displayLogMessage:@"PC setLocalDescription."];
      NSDictionary* json = @{@"type" : sdp.type, @"sdp" : sdp.description};
      NSError* error;
      NSData* data =
          [NSJSONSerialization dataWithJSONObject:json options:0 error:&error];
      NSAssert(!error,
               @"%@",
               [NSString stringWithFormat:@"Error: %@", error.description]);
      [self sendData:data];
  });
}

- (void)peerConnection:(RTCPeerConnection*)peerConnection
    didSetSessionDescriptionWithError:(NSError*)error {
  dispatch_async(dispatch_get_main_queue(), ^(void) {
      if (error) {
        [self displayLogMessage:@"SDP onFailure."];
        NSAssert(NO, error.description);
        return;
      }

      [self displayLogMessage:@"SDP onSuccess() - possibly drain candidates"];
      if (!self.client.initiator) {
        if (self.peerConnection.remoteDescription &&
            !self.peerConnection.localDescription) {
          [self displayLogMessage:@"Callee, setRemoteDescription succeeded"];
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
          [self displayLogMessage:@"PC - createAnswer."];
        } else {
          [self displayLogMessage:@"SDP onSuccess - drain candidates"];
          [self drainRemoteCandidates];
        }
      } else {
        if (self.peerConnection.remoteDescription) {
          [self displayLogMessage:@"SDP onSuccess - drain candidates"];
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
      [self displayLogMessage:message];
  });
}

#pragma mark - internal methods

- (void)disconnect {
  [self.client
      sendData:[@"{\"type\": \"bye\"}" dataUsingEncoding:NSUTF8StringEncoding]];
  [self.peerConnection close];
  self.peerConnection = nil;
  self.pcObserver = nil;
  self.client = nil;
  self.videoSource = nil;
  self.peerConnectionFactory = nil;
  [RTCPeerConnectionFactory deinitializeSSL];
}

- (void)drainRemoteCandidates {
  for (RTCICECandidate* candidate in self.queuedRemoteCandidates) {
    [self.peerConnection addICECandidate:candidate];
  }
  self.queuedRemoteCandidates = nil;
}

- (NSString*)unHTMLifyString:(NSString*)base {
  // TODO(hughv): Investigate why percent escapes are being added.  Removing
  // them isn't necessary on Android.
  // convert HTML escaped characters to UTF8.
  NSString* removePercent =
      [base stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
  // remove leading and trailing ".
  NSRange range;
  range.length = [removePercent length] - 2;
  range.location = 1;
  NSString* removeQuotes = [removePercent substringWithRange:range];
  // convert \" to ".
  NSString* removeEscapedQuotes =
      [removeQuotes stringByReplacingOccurrencesOfString:@"\\\""
                                              withString:@"\""];
  // convert \\ to \.
  NSString* removeBackslash =
      [removeEscapedQuotes stringByReplacingOccurrencesOfString:@"\\\\"
                                                     withString:@"\\"];
  return removeBackslash;
}

- (void)didFireStatsTimer:(NSTimer *)timer {
  if (self.peerConnection) {
    [self.peerConnection getStatsWithDelegate:self
                             mediaStreamTrack:nil
                             statsOutputLevel:RTCStatsOutputLevelDebug];
  }
}

#pragma mark - public methods

- (void)closeVideoUI {
  [self.viewController resetUI];
  [self disconnect];
}

@end
