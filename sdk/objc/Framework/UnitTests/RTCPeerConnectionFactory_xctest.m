/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <WebRTC/RTCConfiguration.h>
#import <WebRTC/RTCMediaConstraints.h>
#import <WebRTC/RTCPeerConnection.h>
#import <WebRTC/RTCPeerConnectionFactory.h>

#import <XCTest/XCTest.h>

@interface RTCPeerConnectionFactoryTests : XCTestCase
- (void)testPeerConnectionLifetime;
@end

@implementation RTCPeerConnectionFactoryTests

- (void)testPeerConnectionLifetime {
  @autoreleasepool {
    RTCConfiguration *config = [[RTCConfiguration alloc] init];

    RTCMediaConstraints *contraints =
        [[RTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];

    RTCPeerConnectionFactory *factory;
    RTCPeerConnection *peerConnection;

    @autoreleasepool {
      factory = [[RTCPeerConnectionFactory alloc] init];
      peerConnection =
          [factory peerConnectionWithConfiguration:config constraints:contraints delegate:nil];
      [peerConnection close];
      factory = nil;
    }
    peerConnection = nil;
  }

  XCTAssertTrue(true, @"Expect test does not crash");
}

- (void)testMediaStreamLifetime {
  @autoreleasepool {
    RTCPeerConnectionFactory *factory;
    RTCMediaStream *mediaStream;

    @autoreleasepool {
      factory = [[RTCPeerConnectionFactory alloc] init];
      mediaStream = [factory mediaStreamWithStreamId:@"mediaStream"];
      factory = nil;
    }
    mediaStream = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

@end
