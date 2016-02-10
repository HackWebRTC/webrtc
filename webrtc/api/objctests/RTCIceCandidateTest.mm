/*
 * libjingle
 * Copyright 2015 Google Inc.
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

#include "webrtc/base/gunit.h"

#import "webrtc/api/objc/RTCIceCandidate.h"
#import "webrtc/api/objc/RTCIceCandidate+Private.h"
#import "webrtc/base/objc/NSString+StdString.h"

@interface RTCIceCandidateTest : NSObject
- (void)testCandidate;
- (void)testInitFromNativeCandidate;
@end

@implementation RTCIceCandidateTest

- (void)testCandidate {
  NSString *sdp = @"candidate:4025901590 1 udp 2122265343 "
                   "fdff:2642:12a6:fe38:c001:beda:fcf9:51aa "
                   "59052 typ host generation 0";

  RTCIceCandidate *candidate = [[RTCIceCandidate alloc] initWithSdp:sdp
                                                      sdpMLineIndex:0
                                                             sdpMid:@"audio"];

  rtc::scoped_ptr<webrtc::IceCandidateInterface> nativeCandidate =
      candidate.nativeCandidate;
  EXPECT_EQ("audio", nativeCandidate->sdp_mid());
  EXPECT_EQ(0, nativeCandidate->sdp_mline_index());

  std::string sdpString;
  nativeCandidate->ToString(&sdpString);
  EXPECT_EQ(sdp.stdString, sdpString);
}

- (void)testInitFromNativeCandidate {
  std::string sdp("candidate:4025901590 1 udp 2122265343 "
                  "fdff:2642:12a6:fe38:c001:beda:fcf9:51aa "
                  "59052 typ host generation 0");
  webrtc::IceCandidateInterface *nativeCandidate =
      webrtc::CreateIceCandidate("audio", 0, sdp, nullptr);

  RTCIceCandidate *iceCandidate =
      [[RTCIceCandidate alloc] initWithNativeCandidate:nativeCandidate];
  EXPECT_TRUE([@"audio" isEqualToString:iceCandidate.sdpMid]);
  EXPECT_EQ(0, iceCandidate.sdpMLineIndex);

  EXPECT_EQ(sdp, iceCandidate.sdp.stdString);
}

@end

TEST(RTCIceCandidateTest, CandidateTest) {
  @autoreleasepool {
    RTCIceCandidateTest *test = [[RTCIceCandidateTest alloc] init];
    [test testCandidate];
  }
}

TEST(RTCIceCandidateTest, InitFromCandidateTest) {
  @autoreleasepool {
    RTCIceCandidateTest *test = [[RTCIceCandidateTest alloc] init];
    [test testInitFromNativeCandidate];
  }
}
