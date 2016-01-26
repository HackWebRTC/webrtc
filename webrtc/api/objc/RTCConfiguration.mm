/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCConfiguration.h"

#import "webrtc/api/objc/RTCConfiguration+Private.h"
#import "webrtc/api/objc/RTCIceServer+Private.h"

@implementation RTCConfiguration

@synthesize iceServers = _iceServers;
@synthesize iceTransportPolicy = _iceTransportPolicy;
@synthesize bundlePolicy = _bundlePolicy;
@synthesize rtcpMuxPolicy = _rtcpMuxPolicy;
@synthesize tcpCandidatePolicy = _tcpCandidatePolicy;
@synthesize audioJitterBufferMaxPackets = _audioJitterBufferMaxPackets;
@synthesize iceConnectionReceivingTimeout = _iceConnectionReceivingTimeout;
@synthesize iceBackupCandidatePairPingInterval =
    _iceBackupCandidatePairPingInterval;

- (instancetype)init {
  if (self = [super init]) {
    _iceServers = [NSMutableArray array];
    // Copy defaults.
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    _iceTransportPolicy =
        [[self class] transportPolicyForTransportsType:config.type];
    _bundlePolicy =
        [[self class] bundlePolicyForNativePolicy:config.bundle_policy];
    _rtcpMuxPolicy =
        [[self class] rtcpMuxPolicyForNativePolicy:config.rtcp_mux_policy];
    _tcpCandidatePolicy = [[self class] tcpCandidatePolicyForNativePolicy:
        config.tcp_candidate_policy];
    _audioJitterBufferMaxPackets = config.audio_jitter_buffer_max_packets;
    _iceConnectionReceivingTimeout = config.ice_connection_receiving_timeout;
    _iceBackupCandidatePairPingInterval =
        config.ice_backup_candidate_pair_ping_interval;
  }
  return self;
}

- (instancetype)initWithIceServers:(NSArray<RTCIceServer *> *)iceServers
                    iceTransportPolicy:(RTCIceTransportPolicy)iceTransportPolicy
                          bundlePolicy:(RTCBundlePolicy)bundlePolicy
                         rtcpMuxPolicy:(RTCRtcpMuxPolicy)rtcpMuxPolicy
                    tcpCandidatePolicy:(RTCTcpCandidatePolicy)tcpCandidatePolicy
           audioJitterBufferMaxPackets:(int)audioJitterBufferMaxPackets
         iceConnectionReceivingTimeout:(int)iceConnectionReceivingTimeout
    iceBackupCandidatePairPingInterval:(int)iceBackupCandidatePairPingInterval {
  if (self = [self init]) {
    if (iceServers) {
      _iceServers = [iceServers copy];
    }
    _iceTransportPolicy = iceTransportPolicy;
    _bundlePolicy = bundlePolicy;
    _rtcpMuxPolicy = rtcpMuxPolicy;
    _tcpCandidatePolicy = tcpCandidatePolicy;
    _audioJitterBufferMaxPackets = audioJitterBufferMaxPackets;
    _iceConnectionReceivingTimeout = iceConnectionReceivingTimeout;
    _iceBackupCandidatePairPingInterval = iceBackupCandidatePairPingInterval;
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:
      @"RTCConfiguration: {\n%@\n%@\n%@\n%@\n%@\n%d\n%d\n%d\n}\n",
      _iceServers,
      [[self class] stringForTransportPolicy:_iceTransportPolicy],
      [[self class] stringForBundlePolicy:_bundlePolicy],
      [[self class] stringForRtcpMuxPolicy:_rtcpMuxPolicy],
      [[self class] stringForTcpCandidatePolicy:_tcpCandidatePolicy],
      _audioJitterBufferMaxPackets,
      _iceConnectionReceivingTimeout,
      _iceBackupCandidatePairPingInterval];
}

#pragma mark - Private

- (webrtc::PeerConnectionInterface::RTCConfiguration)nativeConfiguration {
  webrtc::PeerConnectionInterface::RTCConfiguration nativeConfig;

  for (RTCIceServer *iceServer in _iceServers) {
    nativeConfig.servers.push_back(iceServer.iceServer);
  }
  nativeConfig.type =
      [[self class] nativeTransportsTypeForTransportPolicy:_iceTransportPolicy];
  nativeConfig.bundle_policy =
      [[self class] nativeBundlePolicyForPolicy:_bundlePolicy];
  nativeConfig.rtcp_mux_policy =
      [[self class] nativeRtcpMuxPolicyForPolicy:_rtcpMuxPolicy];
  nativeConfig.tcp_candidate_policy =
      [[self class] nativeTcpCandidatePolicyForPolicy:_tcpCandidatePolicy];
  nativeConfig.audio_jitter_buffer_max_packets = _audioJitterBufferMaxPackets;
  nativeConfig.ice_connection_receiving_timeout =
      _iceConnectionReceivingTimeout;
  nativeConfig.ice_backup_candidate_pair_ping_interval =
      _iceBackupCandidatePairPingInterval;

  return nativeConfig;
}

- (instancetype)initWithNativeConfiguration:
    (webrtc::PeerConnectionInterface::RTCConfiguration)nativeConfig {
  NSMutableArray *iceServers =
        [NSMutableArray arrayWithCapacity:nativeConfig.servers.size()];
  for (auto const &server : nativeConfig.servers) {
    RTCIceServer *iceServer =
        [[RTCIceServer alloc] initWithNativeServer:server];
    [iceServers addObject:iceServer];
  }

  if (self = [self init]) {
    if (iceServers) {
      _iceServers = [iceServers copy];
    }
    _iceTransportPolicy =
        [[self class] transportPolicyForTransportsType:nativeConfig.type];
    _bundlePolicy =
        [[self class] bundlePolicyForNativePolicy:nativeConfig.bundle_policy];
    _rtcpMuxPolicy = [[self class] rtcpMuxPolicyForNativePolicy:
        nativeConfig.rtcp_mux_policy];
    _tcpCandidatePolicy = [[self class] tcpCandidatePolicyForNativePolicy:
        nativeConfig.tcp_candidate_policy];
    _audioJitterBufferMaxPackets = nativeConfig.audio_jitter_buffer_max_packets;
    _iceConnectionReceivingTimeout =
        nativeConfig.ice_connection_receiving_timeout;
    _iceBackupCandidatePairPingInterval =
        nativeConfig.ice_backup_candidate_pair_ping_interval;
  }

  return self;
}

+ (webrtc::PeerConnectionInterface::IceTransportsType)
    nativeTransportsTypeForTransportPolicy:(RTCIceTransportPolicy)policy {
  switch (policy) {
    case RTCIceTransportPolicyNone:
      return webrtc::PeerConnectionInterface::kNone;
    case RTCIceTransportPolicyRelay:
      return webrtc::PeerConnectionInterface::kRelay;
    case RTCIceTransportPolicyNoHost:
      return webrtc::PeerConnectionInterface::kNoHost;
    case RTCIceTransportPolicyAll:
      return webrtc::PeerConnectionInterface::kAll;
  }
}

+ (RTCIceTransportPolicy)transportPolicyForTransportsType:
    (webrtc::PeerConnectionInterface::IceTransportsType)nativeType {
  switch (nativeType) {
    case webrtc::PeerConnectionInterface::kNone:
      return RTCIceTransportPolicyNone;
    case webrtc::PeerConnectionInterface::kRelay:
      return RTCIceTransportPolicyRelay;
    case webrtc::PeerConnectionInterface::kNoHost:
      return RTCIceTransportPolicyNoHost;
    case webrtc::PeerConnectionInterface::kAll:
      return RTCIceTransportPolicyAll;
  }
}

+ (NSString *)stringForTransportPolicy:(RTCIceTransportPolicy)policy {
  switch (policy) {
    case RTCIceTransportPolicyNone:
      return @"NONE";
    case RTCIceTransportPolicyRelay:
      return @"RELAY";
    case RTCIceTransportPolicyNoHost:
      return @"NO_HOST";
    case RTCIceTransportPolicyAll:
      return @"ALL";
  }
}

+ (webrtc::PeerConnectionInterface::BundlePolicy)nativeBundlePolicyForPolicy:
    (RTCBundlePolicy)policy {
  switch (policy) {
    case RTCBundlePolicyBalanced:
      return webrtc::PeerConnectionInterface::kBundlePolicyBalanced;
    case RTCBundlePolicyMaxCompat:
      return webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat;
    case RTCBundlePolicyMaxBundle:
      return webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;
  }
}

+ (RTCBundlePolicy)bundlePolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::BundlePolicy)nativePolicy {
  switch (nativePolicy) {
    case webrtc::PeerConnectionInterface::kBundlePolicyBalanced:
      return RTCBundlePolicyBalanced;
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat:
      return RTCBundlePolicyMaxCompat;
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle:
      return RTCBundlePolicyMaxBundle;
  }
}

+ (NSString *)stringForBundlePolicy:(RTCBundlePolicy)policy {
  switch (policy) {
    case RTCBundlePolicyBalanced:
      return @"BALANCED";
    case RTCBundlePolicyMaxCompat:
      return @"MAX_COMPAT";
    case RTCBundlePolicyMaxBundle:
      return @"MAX_BUNDLE";
  }
}

+ (webrtc::PeerConnectionInterface::RtcpMuxPolicy)nativeRtcpMuxPolicyForPolicy:
    (RTCRtcpMuxPolicy)policy {
  switch (policy) {
    case RTCRtcpMuxPolicyNegotiate:
      return webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
    case RTCRtcpMuxPolicyRequire:
      return webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire;
  }
}

+ (RTCRtcpMuxPolicy)rtcpMuxPolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::RtcpMuxPolicy)nativePolicy {
  switch (nativePolicy) {
    case webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate:
      return RTCRtcpMuxPolicyNegotiate;
    case webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire:
      return RTCRtcpMuxPolicyRequire;
  }
}

+ (NSString *)stringForRtcpMuxPolicy:(RTCRtcpMuxPolicy)policy {
  switch (policy) {
    case RTCRtcpMuxPolicyNegotiate:
      return @"NEGOTIATE";
    case RTCRtcpMuxPolicyRequire:
      return @"REQUIRE";
  }
}

+ (webrtc::PeerConnectionInterface::TcpCandidatePolicy)
    nativeTcpCandidatePolicyForPolicy:(RTCTcpCandidatePolicy)policy {
  switch (policy) {
    case RTCTcpCandidatePolicyEnabled:
      return webrtc::PeerConnectionInterface::kTcpCandidatePolicyEnabled;
    case RTCTcpCandidatePolicyDisabled:
      return webrtc::PeerConnectionInterface::kTcpCandidatePolicyDisabled;
  }
}

+ (RTCTcpCandidatePolicy)tcpCandidatePolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::TcpCandidatePolicy)nativePolicy {
  switch (nativePolicy) {
    case webrtc::PeerConnectionInterface::kTcpCandidatePolicyEnabled:
      return RTCTcpCandidatePolicyEnabled;
    case webrtc::PeerConnectionInterface::kTcpCandidatePolicyDisabled:
      return RTCTcpCandidatePolicyDisabled;
  }
}

+ (NSString *)stringForTcpCandidatePolicy:(RTCTcpCandidatePolicy)policy {
  switch (policy) {
    case RTCTcpCandidatePolicyEnabled:
      return @"TCP_ENABLED";
    case RTCTcpCandidatePolicyDisabled:
      return @"TCP_DISABLED";
  }
}

@end
