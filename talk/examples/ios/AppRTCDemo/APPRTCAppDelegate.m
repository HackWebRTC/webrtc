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

@interface PCObserver : NSObject<RTCPeerConnectionDelegate>

- (id)initWithDelegate:(id<APPRTCSendMessage>)delegate;

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

- (void)peerConnectionOnError:(RTCPeerConnection *)peerConnection {
  NSLog(@"PCO onError.");
  NSAssert(NO, @"PeerConnection failed.");
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    signalingStateChanged:(RTCSignalingState)stateChanged {
  NSLog(@"PCO onSignalingStateChange.");
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
             addedStream:(RTCMediaStream *)stream {
  NSLog(@"PCO onAddStream.");
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    NSAssert([stream.audioTracks count] >= 1,
             @"Expected at least 1 audio stream");
    //NSAssert([stream.videoTracks count] >= 1,
    //         @"Expected at least 1 video stream");
    // TODO(hughv): Add video support
  });
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
         removedStream:(RTCMediaStream *)stream {
  NSLog(@"PCO onRemoveStream.");
  // TODO(hughv): Remove video track.
}

- (void)
    peerConnectionOnRenegotiationNeeded:(RTCPeerConnection *)peerConnection {
  NSLog(@"PCO onRenegotiationNeeded.");
  // TODO(hughv): Handle this.
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
       gotICECandidate:(RTCICECandidate *)candidate {
  NSLog(@"PCO onICECandidate.\n  Mid[%@] Index[%d] Sdp[%@]",
        candidate.sdpMid,
        candidate.sdpMLineIndex,
        candidate.sdp);
  NSDictionary *json =
      @{ @"type" : @"candidate",
         @"label" : [NSNumber numberWithInt:candidate.sdpMLineIndex],
         @"id" : candidate.sdpMid,
         @"candidate" : candidate.sdp };
  NSError *error;
  NSData *data =
      [NSJSONSerialization dataWithJSONObject:json options:0 error:&error];
  if (!error) {
    [_delegate sendData:data];
  } else {
    NSAssert(NO, @"Unable to serialize JSON object with error: %@",
             error.localizedDescription);
  }
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    iceGatheringChanged:(RTCICEGatheringState)newState {
  NSLog(@"PCO onIceGatheringChange. %d", newState);
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    iceConnectionChanged:(RTCICEConnectionState)newState {
  NSLog(@"PCO onIceConnectionChange. %d", newState);
}

@end

@interface APPRTCAppDelegate ()

@property(nonatomic, strong) APPRTCAppClient *client;
@property(nonatomic, strong) PCObserver *pcObserver;
@property(nonatomic, strong) RTCPeerConnection *peerConnection;
@property(nonatomic, strong) RTCPeerConnectionFactory *peerConnectionFactory;
@property(nonatomic, strong) NSMutableArray *queuedRemoteCandidates;

@end

@implementation APPRTCAppDelegate

#pragma mark - UIApplicationDelegate methods

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  [RTCPeerConnectionFactory initializeSSL];
  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  self.viewController =
      [[APPRTCViewController alloc] initWithNibName:@"APPRTCViewController"
                                             bundle:nil];
  self.window.rootViewController = self.viewController;
  [self.window makeKeyAndVisible];
  return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application {
  [self displayLogMessage:@"Application lost focus, connection broken."];
  [self disconnect];
  [self.viewController resetUI];
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
}

- (void)applicationWillTerminate:(UIApplication *)application {
}

- (BOOL)application:(UIApplication *)application
              openURL:(NSURL *)url
    sourceApplication:(NSString *)sourceApplication
           annotation:(id)annotation {
  if (self.client) {
    return NO;
  }
  self.client = [[APPRTCAppClient alloc] init];
  self.client.ICEServerDelegate = self;
  self.client.messageHandler = self;
  [self.client connectToRoom:url];
  return YES;
}

- (void)displayLogMessage:(NSString *)message {
  NSLog(@"%@", message);
  [self.viewController displayText:message];
}

#pragma mark - RTCSendMessage method

- (void)sendData:(NSData *)data {
  [self.client sendData:data];
}

#pragma mark - ICEServerDelegate method

- (void)onICEServers:(NSArray *)servers {
  self.queuedRemoteCandidates = [NSMutableArray array];
  self.peerConnectionFactory = [[RTCPeerConnectionFactory alloc] init];
  RTCMediaConstraints *constraints = [[RTCMediaConstraints alloc] init];
  self.pcObserver = [[PCObserver alloc] initWithDelegate:self];
  self.peerConnection =
      [self.peerConnectionFactory peerConnectionWithICEServers:servers
                                                   constraints:constraints
                                                      delegate:self.pcObserver];
  RTCMediaStream *lms =
      [self.peerConnectionFactory mediaStreamWithLabel:@"ARDAMS"];
  // TODO(hughv): Add video.
  [lms addAudioTrack:[self.peerConnectionFactory audioTrackWithID:@"ARDAMSa0"]];
  [self.peerConnection addStream:lms constraints:constraints];
  [self displayLogMessage:@"onICEServers - add local stream."];
}

#pragma mark - GAEMessageHandler methods

- (void)onOpen {
  [self displayLogMessage:@"GAE onOpen - create offer."];
  RTCPair *audio =
      [[RTCPair alloc] initWithKey:@"OfferToReceiveAudio" value:@"true"];
  // TODO(hughv): Add video.
  //  RTCPair *video = [[RTCPair alloc] initWithKey:@"OfferToReceiveVideo"
  //                                          value:@"true"];
  NSArray *mandatory = @[ audio /*, video*/ ];
  RTCMediaConstraints *constraints =
      [[RTCMediaConstraints alloc] initWithMandatoryConstraints:mandatory
                                            optionalConstraints:nil];
  [self.peerConnection createOfferWithDelegate:self constraints:constraints];
  [self displayLogMessage:@"PC - createOffer."];
}

- (void)onMessage:(NSString *)data {
  NSString *message = [self unHTMLifyString:data];
  NSError *error;
  NSDictionary *objects = [NSJSONSerialization
      JSONObjectWithData:[message dataUsingEncoding:NSUTF8StringEncoding]
                 options:0
                   error:&error];
  NSAssert(!error,
           @"%@",
           [NSString stringWithFormat:@"Error: %@", error.description]);
  NSAssert([objects count] > 0, @"Invalid JSON object");
  NSString *value = [objects objectForKey:@"type"];
  [self displayLogMessage:
          [NSString stringWithFormat:@"GAE onMessage type - %@", value]];
  if ([value compare:@"candidate"] == NSOrderedSame) {
    NSString *mid = [objects objectForKey:@"id"];
    NSNumber *sdpLineIndex = [objects objectForKey:@"label"];
    NSString *sdp = [objects objectForKey:@"candidate"];
    RTCICECandidate *candidate =
        [[RTCICECandidate alloc] initWithMid:mid
                                       index:sdpLineIndex.intValue
                                         sdp:sdp];
    if (self.queuedRemoteCandidates) {
      [self.queuedRemoteCandidates addObject:candidate];
    } else {
      [self.peerConnection addICECandidate:candidate];
    }
  } else if (([value compare:@"offer"] == NSOrderedSame) ||
             ([value compare:@"answer"] == NSOrderedSame)) {
    NSString *sdpString = [objects objectForKey:@"sdp"];
    RTCSessionDescription *sdp =
        [[RTCSessionDescription alloc] initWithType:value sdp:sdpString];
    [self.peerConnection setRemoteDescriptionWithDelegate:self
                                       sessionDescription:sdp];
    [self displayLogMessage:@"PC - setRemoteDescription."];
  } else if ([value compare:@"bye"] == NSOrderedSame) {
    [self disconnect];
  } else {
    NSAssert(NO, @"Invalid message: %@", data);
  }
}

- (void)onClose {
  [self displayLogMessage:@"GAE onClose."];
  [self disconnect];
}

- (void)onError:(int)code withDescription:(NSString *)description {
  [self displayLogMessage:
          [NSString stringWithFormat:@"GAE onError:  %@", description]];
  [self disconnect];
}

#pragma mark - RTCSessionDescriptonDelegate methods

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didCreateSessionDescription:(RTCSessionDescription *)sdp
                          error:(NSError *)error {
  if (error) {
    [self displayLogMessage:@"SDP onFailure."];
    NSAssert(NO, error.description);
    return;
  }

  [self displayLogMessage:@"SDP onSuccess(SDP) - set local description."];
  [self.peerConnection setLocalDescriptionWithDelegate:self
                                    sessionDescription:sdp];
  [self displayLogMessage:@"PC setLocalDescription."];
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    NSDictionary *json = @{ @"type" : sdp.type, @"sdp" : sdp.description };
    NSError *error;
    NSData *data =
        [NSJSONSerialization dataWithJSONObject:json options:0 error:&error];
    NSAssert(!error,
             @"%@",
             [NSString stringWithFormat:@"Error: %@", error.description]);
    [self sendData:data];
  });
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didSetSessionDescriptionWithError:(NSError *)error {
  if (error) {
    [self displayLogMessage:@"SDP onFailure."];
    NSAssert(NO, error.description);
    return;
  }

  [self displayLogMessage:@"SDP onSuccess() - possibly drain candidates"];
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    // TODO(hughv): Handle non-initiator case.  http://s10/46622051
    if (self.peerConnection.remoteDescription) {
      [self displayLogMessage:@"SDP onSuccess - drain candidates"];
      [self drainRemoteCandidates];
    }
  });
}

#pragma mark - internal methods

- (void)disconnect {
  [self.client
      sendData:[@"{\"type\": \"bye\"}" dataUsingEncoding:NSUTF8StringEncoding]];
  self.peerConnection = nil;
  self.peerConnectionFactory = nil;
  self.pcObserver = nil;
  self.client.ICEServerDelegate = nil;
  self.client.messageHandler = nil;
  self.client = nil;
  [RTCPeerConnectionFactory deinitializeSSL];
}

- (void)drainRemoteCandidates {
  for (RTCICECandidate *candidate in self.queuedRemoteCandidates) {
    [self.peerConnection addICECandidate:candidate];
  }
  self.queuedRemoteCandidates = nil;
}

- (NSString *)unHTMLifyString:(NSString *)base {
  // TODO(hughv): Investigate why percent escapes are being added.  Removing
  // them isn't necessary on Android.
  // convert HTML escaped characters to UTF8.
  NSString *removePercent =
      [base stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
  // remove leading and trailing ".
  NSRange range;
  range.length = [removePercent length] - 2;
  range.location = 1;
  NSString *removeQuotes = [removePercent substringWithRange:range];
  // convert \" to ".
  NSString *removeEscapedQuotes =
      [removeQuotes stringByReplacingOccurrencesOfString:@"\\\""
                                              withString:@"\""];
  // convert \\ to \.
  NSString *removeBackslash =
      [removeEscapedQuotes stringByReplacingOccurrencesOfString:@"\\\\"
                                                     withString:@"\\"];
  return removeBackslash;
}

@end
