/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#include <vector>

#include "webrtc/base/gunit.h"

#import "webrtc/api/objc/RTCConfiguration.h"
#import "webrtc/api/objc/RTCConfiguration+Private.h"
#import "webrtc/api/objc/RTCIceServer.h"
#import "webrtc/base/objc/NSString+StdString.h"

@interface RTCConfigurationTest : NSObject
- (void)testConversionToNativeConfiguration;
@end

@implementation RTCConfigurationTest

- (void)testConversionToNativeConfiguration {
  NSArray *urlStrings = @[ @"stun:stun1.example.net" ];
  RTCIceServer *server = [[RTCIceServer alloc] initWithURLStrings:urlStrings];

  RTCConfiguration *config = [[RTCConfiguration alloc] init];
  config.iceServers = @[ server ];
  config.iceTransportPolicy = RTCIceTransportPolicyRelay;
  config.bundlePolicy = RTCBundlePolicyMaxBundle;
  config.rtcpMuxPolicy = RTCRtcpMuxPolicyNegotiate;
  config.tcpCandidatePolicy = RTCTcpCandidatePolicyDisabled;
  const int maxPackets = 60;
  const int timeout = 1;
  const int interval = 2;
  config.audioJitterBufferMaxPackets = maxPackets;
  config.iceConnectionReceivingTimeout = timeout;
  config.iceBackupCandidatePairPingInterval = interval;

  webrtc::PeerConnectionInterface::RTCConfiguration nativeConfig =
      config.nativeConfiguration;
  EXPECT_EQ(1u, nativeConfig.servers.size());
  webrtc::PeerConnectionInterface::IceServer nativeServer =
      nativeConfig.servers.front();
  EXPECT_EQ(1u, nativeServer.urls.size());
  EXPECT_EQ("stun:stun1.example.net", nativeServer.urls.front());

  EXPECT_EQ(webrtc::PeerConnectionInterface::kRelay, nativeConfig.type);
  EXPECT_EQ(webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle,
            nativeConfig.bundle_policy);
  EXPECT_EQ(webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate,
            nativeConfig.rtcp_mux_policy);
  EXPECT_EQ(webrtc::PeerConnectionInterface::kTcpCandidatePolicyDisabled,
            nativeConfig.tcp_candidate_policy);
  EXPECT_EQ(maxPackets, nativeConfig.audio_jitter_buffer_max_packets);
  EXPECT_EQ(timeout, nativeConfig.ice_connection_receiving_timeout);
  EXPECT_EQ(interval, nativeConfig.ice_backup_candidate_pair_ping_interval);
}

@end

TEST(RTCConfigurationTest, NativeConfigurationConversionTest) {
  @autoreleasepool {
    RTCConfigurationTest *test = [[RTCConfigurationTest alloc] init];
    [test testConversionToNativeConfiguration];
  }
}

