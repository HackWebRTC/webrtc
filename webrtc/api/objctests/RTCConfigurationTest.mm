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
- (void)testInitFromNativeConfiguration;
@end

@implementation RTCConfigurationTest

- (void)testConversionToNativeConfiguration {
  NSArray *urlStrings = @[ @"stun:stun1.example.net" ];
  RTCIceServer *server = [[RTCIceServer alloc] initWithURLStrings:urlStrings];

  RTCConfiguration *config =
      [[RTCConfiguration alloc] initWithIceServers:@[ server ]
                                iceTransportPolicy:RTCIceTransportPolicyRelay
                                      bundlePolicy:RTCBundlePolicyMaxBundle
                                     rtcpMuxPolicy:RTCRtcpMuxPolicyNegotiate
                                tcpCandidatePolicy:RTCTcpCandidatePolicyDisabled
                       audioJitterBufferMaxPackets:60
                     iceConnectionReceivingTimeout:1
                iceBackupCandidatePairPingInterval:2];

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
  EXPECT_EQ(60, nativeConfig.audio_jitter_buffer_max_packets);
  EXPECT_EQ(1, nativeConfig.ice_connection_receiving_timeout);
  EXPECT_EQ(2, nativeConfig.ice_backup_candidate_pair_ping_interval);
}

- (void)testInitFromNativeConfiguration {
  webrtc::PeerConnectionInterface::RTCConfiguration nativeConfig;

  webrtc::PeerConnectionInterface::IceServer nativeServer;
  nativeServer.username = "username";
  nativeServer.password = "password";
  nativeServer.urls.push_back("stun:stun.example.net");
  webrtc::PeerConnectionInterface::IceServers servers { nativeServer };

  nativeConfig.servers = servers;
  nativeConfig.type = webrtc::PeerConnectionInterface::kNoHost;
  nativeConfig.bundle_policy =
      webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat;
  nativeConfig.rtcp_mux_policy =
      webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire;
  nativeConfig.tcp_candidate_policy =
      webrtc::PeerConnectionInterface::kTcpCandidatePolicyEnabled;
  nativeConfig.audio_jitter_buffer_max_packets = 40;
  nativeConfig.ice_connection_receiving_timeout =
      webrtc::PeerConnectionInterface::RTCConfiguration::kUndefined;
  nativeConfig.ice_backup_candidate_pair_ping_interval =
      webrtc::PeerConnectionInterface::RTCConfiguration::kUndefined;

  RTCConfiguration *config =
      [[RTCConfiguration alloc] initWithNativeConfiguration:nativeConfig];

  EXPECT_EQ(1u, config.iceServers.count);
  RTCIceServer *server = config.iceServers.firstObject;
  EXPECT_EQ(1u, server.urlStrings.count);
  EXPECT_TRUE([@"stun:stun.example.net" isEqualToString:
      server.urlStrings.firstObject]);

  EXPECT_EQ(RTCIceTransportPolicyNoHost, config.iceTransportPolicy);
  EXPECT_EQ(RTCBundlePolicyMaxCompat, config.bundlePolicy);
  EXPECT_EQ(RTCRtcpMuxPolicyRequire, config.rtcpMuxPolicy);
  EXPECT_EQ(RTCTcpCandidatePolicyEnabled, config.tcpCandidatePolicy);
  EXPECT_EQ(40, config.audioJitterBufferMaxPackets);
  EXPECT_EQ(-1, config.iceConnectionReceivingTimeout);
  EXPECT_EQ(-1, config.iceBackupCandidatePairPingInterval);
}

@end

TEST(RTCConfigurationTest, NativeConfigurationConversionTest) {
  @autoreleasepool {
    RTCConfigurationTest *test = [[RTCConfigurationTest alloc] init];
    [test testConversionToNativeConfiguration];
  }
}

TEST(RTCConfigurationTest, InitFromConfigurationTest) {
  @autoreleasepool {
    RTCConfigurationTest *test = [[RTCConfigurationTest alloc] init];
    [test testInitFromNativeConfiguration];
  }
}
