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

#import <Foundation/Foundation.h>

#import "RTCICEServer.h"
#import "RTCMediaConstraints.h"
#import "RTCMediaStream.h"
#import "RTCPeerConnection.h"
#import "RTCPeerConnectionFactory.h"
#import "RTCPeerConnectionSyncObserver.h"
#import "RTCSessionDescription.h"
#import "RTCSessionDescriptionSyncObserver.h"
#import "RTCVideoRenderer.h"
#import "RTCVideoTrack.h"

#include "talk/base/gunit.h"
#include "talk/base/ssladapter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RTCPeerConnectionTest : NSObject

// Returns whether the two sessions are of the same type.
+ (BOOL)isSession:(RTCSessionDescription *)session1
    ofSameTypeAsSession:(RTCSessionDescription *)session2;

// Create and add tracks to pc, with the given source, label, and IDs
- (RTCMediaStream *)
    addTracksToPeerConnection:(RTCPeerConnection *)pc
                  withFactory:(RTCPeerConnectionFactory *)factory
                  videoSource:(RTCVideoSource *)videoSource
                  streamLabel:(NSString *)streamLabel
                 videoTrackID:(NSString *)videoTrackID
                 audioTrackID:(NSString *)audioTrackID;

- (void)testCompleteSession;

@end

@implementation RTCPeerConnectionTest

+ (BOOL)isSession:(RTCSessionDescription *)session1
    ofSameTypeAsSession:(RTCSessionDescription *)session2 {
  return [session1.type isEqual:session2.type];
}

- (RTCMediaStream *)
    addTracksToPeerConnection:(RTCPeerConnection *)pc
                  withFactory:(RTCPeerConnectionFactory *)factory
                  videoSource:(RTCVideoSource *)videoSource
                  streamLabel:(NSString *)streamLabel
                 videoTrackID:(NSString *)videoTrackID
                 audioTrackID:(NSString *)audioTrackID {
  RTCMediaStream *localMediaStream = [factory mediaStreamWithLabel:streamLabel];
  RTCVideoTrack *videoTrack =
      [factory videoTrackWithID:videoTrackID source:videoSource];
  RTCVideoRenderer *videoRenderer =
      [[RTCVideoRenderer alloc] initWithDelegate:nil];
  [videoTrack addRenderer:videoRenderer];
  [localMediaStream addVideoTrack:videoTrack];
  // Test that removal/re-add works.
  [localMediaStream removeVideoTrack:videoTrack];
  [localMediaStream addVideoTrack:videoTrack];
  RTCAudioTrack *audioTrack = [factory audioTrackWithID:audioTrackID];
  [localMediaStream addAudioTrack:audioTrack];
  RTCMediaConstraints *constraints = [[RTCMediaConstraints alloc] init];
  [pc addStream:localMediaStream constraints:constraints];
  return localMediaStream;
}

- (void)testCompleteSession {
  RTCPeerConnectionFactory *factory = [[RTCPeerConnectionFactory alloc] init];
  NSString *stunURL = @"stun:stun.l.google.com:19302";
  RTCICEServer *stunServer =
      [[RTCICEServer alloc] initWithURI:[NSURL URLWithString:stunURL]
                               username:@""
                               password:@""];
  NSArray *iceServers = @[stunServer];

  RTCMediaConstraints *constraints = [[RTCMediaConstraints alloc] init];
  RTCPeerConnectionSyncObserver *offeringExpectations =
      [[RTCPeerConnectionSyncObserver alloc] init];
  RTCPeerConnection *pcOffer =
      [factory peerConnectionWithICEServers:iceServers
                                constraints:constraints
                                   delegate:offeringExpectations];

  RTCPeerConnectionSyncObserver *answeringExpectations =
      [[RTCPeerConnectionSyncObserver alloc] init];
  RTCPeerConnection *pcAnswer =
      [factory peerConnectionWithICEServers:iceServers
                                constraints:constraints
                                   delegate:answeringExpectations];

  // TODO(hughv): Create video capturer
  RTCVideoCapturer *capturer = nil;
  RTCVideoSource *videoSource =
      [factory videoSourceWithCapturer:capturer constraints:constraints];

  // Here and below, "oLMS" refers to offerer's local media stream, and "aLMS"
  // refers to the answerer's local media stream, with suffixes of "a0" and "v0"
  // for audio and video tracks, resp.  These mirror chrome historical naming.
  RTCMediaStream *oLMSUnused =
      [self addTracksToPeerConnection:pcOffer
                          withFactory:factory
                          videoSource:videoSource
                          streamLabel:@"oLMS"
                         videoTrackID:@"oLMSv0"
                         audioTrackID:@"oLMSa0"];
  RTCSessionDescriptionSyncObserver *sdpObserver =
      [[RTCSessionDescriptionSyncObserver alloc] init];
  [pcOffer createOfferWithDelegate:sdpObserver constraints:constraints];
  [sdpObserver wait];
  EXPECT_TRUE(sdpObserver.success);
  RTCSessionDescription *offerSDP = sdpObserver.sessionDescription;
  EXPECT_EQ([@"offer" compare:offerSDP.type options:NSCaseInsensitiveSearch],
            NSOrderedSame);
  EXPECT_GT([offerSDP.description length], 0);

  sdpObserver = [[RTCSessionDescriptionSyncObserver alloc] init];
  [answeringExpectations
      expectSignalingChange:RTCSignalingHaveRemoteOffer];
  [answeringExpectations expectAddStream:@"oLMS"];
  [pcAnswer setRemoteDescriptionWithDelegate:sdpObserver
                          sessionDescription:offerSDP];
  [sdpObserver wait];

  RTCMediaStream *aLMSUnused =
      [self addTracksToPeerConnection:pcAnswer
                          withFactory:factory
                          videoSource:videoSource
                          streamLabel:@"aLMS"
                         videoTrackID:@"aLMSv0"
                         audioTrackID:@"aLMSa0"];

  sdpObserver = [[RTCSessionDescriptionSyncObserver alloc] init];
  [pcAnswer createAnswerWithDelegate:sdpObserver constraints:constraints];
  [sdpObserver wait];
  EXPECT_TRUE(sdpObserver.success);
  RTCSessionDescription *answerSDP = sdpObserver.sessionDescription;
  EXPECT_EQ([@"answer" compare:answerSDP.type options:NSCaseInsensitiveSearch],
            NSOrderedSame);
  EXPECT_GT([answerSDP.description length], 0);

  [offeringExpectations expectICECandidates:2];
  [answeringExpectations expectICECandidates:2];

  sdpObserver = [[RTCSessionDescriptionSyncObserver alloc] init];
  [answeringExpectations expectSignalingChange:RTCSignalingStable];
  [pcAnswer setLocalDescriptionWithDelegate:sdpObserver
                         sessionDescription:answerSDP];
  [sdpObserver wait];
  EXPECT_TRUE(sdpObserver.sessionDescription == NULL);

  sdpObserver = [[RTCSessionDescriptionSyncObserver alloc] init];
  [offeringExpectations expectSignalingChange:RTCSignalingHaveLocalOffer];
  [pcOffer setLocalDescriptionWithDelegate:sdpObserver
                        sessionDescription:offerSDP];
  [sdpObserver wait];
  EXPECT_TRUE(sdpObserver.sessionDescription == NULL);

  [offeringExpectations expectICEConnectionChange:RTCICEConnectionChecking];
  [offeringExpectations expectICEConnectionChange:RTCICEConnectionConnected];
  [answeringExpectations expectICEConnectionChange:RTCICEConnectionChecking];
  [answeringExpectations expectICEConnectionChange:RTCICEConnectionConnected];

  [offeringExpectations expectICEGatheringChange:RTCICEGatheringComplete];
  [answeringExpectations expectICEGatheringChange:RTCICEGatheringComplete];

  sdpObserver = [[RTCSessionDescriptionSyncObserver alloc] init];
  [offeringExpectations expectSignalingChange:RTCSignalingStable];
  [offeringExpectations expectAddStream:@"aLMS"];
  [pcOffer setRemoteDescriptionWithDelegate:sdpObserver
                         sessionDescription:answerSDP];
  [sdpObserver wait];
  EXPECT_TRUE(sdpObserver.sessionDescription == NULL);

  EXPECT_TRUE([offerSDP.type isEqual:pcOffer.localDescription.type]);
  EXPECT_TRUE([answerSDP.type isEqual:pcOffer.remoteDescription.type]);
  EXPECT_TRUE([offerSDP.type isEqual:pcAnswer.remoteDescription.type]);
  EXPECT_TRUE([answerSDP.type isEqual:pcAnswer.localDescription.type]);

  for (RTCICECandidate *candidate in
       offeringExpectations.releaseReceivedICECandidates) {
    [pcAnswer addICECandidate:candidate];
  }
  for (RTCICECandidate *candidate in
       answeringExpectations.releaseReceivedICECandidates) {
    [pcOffer addICECandidate:candidate];
  }

  [offeringExpectations waitForAllExpectationsToBeSatisfied];
  [answeringExpectations waitForAllExpectationsToBeSatisfied];

  // Let the audio feedback run for 10s to allow human testing and to ensure
  // things stabilize.  TODO(fischman): replace seconds with # of video frames,
  // when we have video flowing.
  [[NSRunLoop currentRunLoop]
      runUntilDate:[NSDate dateWithTimeIntervalSinceNow:10]];

  // TODO(hughv): Implement orderly shutdown.
}

@end

// TODO(fischman): move {Initialize,Cleanup}SSL into alloc/dealloc of
// RTCPeerConnectionTest and avoid the appearance of RTCPeerConnectionTest being
// a TestBase since it's not.
TEST(RTCPeerConnectionTest, SessionTest) {
  talk_base::InitializeSSL();
  RTCPeerConnectionTest *pcTest = [[RTCPeerConnectionTest alloc] init];
  [pcTest testCompleteSession];
  talk_base::CleanupSSL();
}
