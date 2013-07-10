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

#import "RTCPeerConnectionSyncObserver.h"

#import "RTCMediaStream.h"

@implementation RTCPeerConnectionSyncObserver {
  int _expectedErrors;
  NSMutableArray *_expectedSignalingChanges;
  NSMutableArray *_expectedAddStreamLabels;
  NSMutableArray *_expectedRemoveStreamLabels;
  int _expectedICECandidates;
  NSMutableArray *_receivedICECandidates;
  NSMutableArray *_expectedICEConnectionChanges;
  NSMutableArray *_expectedICEGatheringChanges;
}

- (id)init {
  self = [super init];
  if (self) {
    _expectedSignalingChanges = [NSMutableArray array];
    _expectedSignalingChanges = [NSMutableArray array];
    _expectedAddStreamLabels = [NSMutableArray array];
    _expectedRemoveStreamLabels = [NSMutableArray array];
    _receivedICECandidates = [NSMutableArray array];
    _expectedICEConnectionChanges = [NSMutableArray array];
    _expectedICEGatheringChanges = [NSMutableArray array];
  }
  return self;
}

- (int)popFirstElementAsInt:(NSMutableArray *)array {
  NSAssert([array count] > 0, @"Empty array");
  NSNumber *boxedState = [array objectAtIndex:0];
  [array removeObjectAtIndex:0];
  return [boxedState intValue];
}

- (NSString *)popFirstElementAsNSString:(NSMutableArray *)array {
  NSAssert([array count] > 0, @"Empty expectation array");
  NSString *string = [array objectAtIndex:0];
  [array removeObjectAtIndex:0];
  return string;
}

- (BOOL)areAllExpectationsSatisfied {
  return _expectedICECandidates <= 0 &&  // See comment in gotICECandidate.
         _expectedErrors == 0 &&
         [_expectedSignalingChanges count] == 0 &&
         [_expectedICEConnectionChanges count] == 0 &&
         [_expectedICEGatheringChanges count] == 0 &&
         [_expectedAddStreamLabels count] == 0 &&
         [_expectedRemoveStreamLabels count] == 0;
  // TODO(hughv): Test video state here too.
}

- (NSArray *)releaseReceivedICECandidates {
  NSArray* ret = _receivedICECandidates;
  _receivedICECandidates = [NSMutableArray array];
  return ret;
}

- (void)expectError {
  ++_expectedErrors;
}

- (void)expectSignalingChange:(RTCSignalingState)state {
  [_expectedSignalingChanges addObject:@((int)state)];
}

- (void)expectAddStream:(NSString *)label {
  [_expectedAddStreamLabels addObject:label];
}

- (void)expectRemoveStream:(NSString *)label {
  [_expectedRemoveStreamLabels addObject:label];
}

- (void)expectICECandidates:(int)count {
  _expectedICECandidates += count;
}

- (void)expectICEConnectionChange:(RTCICEConnectionState)state {
  [_expectedICEConnectionChanges addObject:@((int)state)];
}

- (void)expectICEGatheringChange:(RTCICEGatheringState)state {
  [_expectedICEGatheringChanges addObject:@((int)state)];
}

- (void)waitForAllExpectationsToBeSatisfied {
  // TODO (fischman):  Revisit.  Keeping in sync with the Java version, but
  // polling is not optimal.
  // https://code.google.com/p/libjingle/source/browse/trunk/talk/app/webrtc/javatests/src/org/webrtc/PeerConnectionTest.java?line=212#212
  while (![self areAllExpectationsSatisfied]) {
    [[NSRunLoop currentRunLoop]
        runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1]];
  }
}

#pragma mark - RTCPeerConnectionDelegate methods

- (void)peerConnectionOnError:(RTCPeerConnection *)peerConnection {
  NSLog(@"RTCPeerConnectionDelegate::onError");
  NSAssert(--_expectedErrors >= 0, @"Unexpected error");
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    signalingStateChanged:(RTCSignalingState)stateChanged {
  int expectedState = [self popFirstElementAsInt:_expectedSignalingChanges];
  NSString *message = [NSString stringWithFormat: @"RTCPeerConnectionDelegate::"
      @"onSignalingStateChange [%d] expected[%d]", stateChanged, expectedState];
  NSAssert(expectedState == (int) stateChanged, message);
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
           addedStream:(RTCMediaStream *)stream {
  NSString *expectedLabel =
      [self popFirstElementAsNSString:_expectedAddStreamLabels];
  NSAssert([expectedLabel isEqual:stream.label], @"Stream not expected");
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
        removedStream:(RTCMediaStream *)stream {
  NSString *expectedLabel =
      [self popFirstElementAsNSString:_expectedRemoveStreamLabels];
  NSAssert([expectedLabel isEqual:stream.label], @"Stream not expected");
}

- (void)peerConnectionOnRenegotiationNeeded:
    (RTCPeerConnection *)peerConnection {
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
       gotICECandidate:(RTCICECandidate *)candidate {
  --_expectedICECandidates;
  // We don't assert expectedICECandidates >= 0 because it's hard to know
  // how many to expect, in general.  We only use expectICECandidates to
  // assert a minimal count.
  [_receivedICECandidates addObject:candidate];
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    iceGatheringChanged:(RTCICEGatheringState)newState {
  // It's fine to get a variable number of GATHERING messages before
  // COMPLETE fires (depending on how long the test runs) so we don't assert
  // any particular count.
  if (newState == RTCICEGatheringGathering) {
    return;
  }
  int expectedState = [self popFirstElementAsInt:_expectedICEGatheringChanges];
  NSAssert(expectedState == (int)newState, @"Empty expectation array");
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    iceConnectionChanged:(RTCICEConnectionState)newState {
  int expectedState = [self popFirstElementAsInt:_expectedICEConnectionChanges];
  NSAssert(expectedState == (int)newState, @"Empty expectation array");
}

@end
