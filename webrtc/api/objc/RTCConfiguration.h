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

@class RTCIceServer;

/**
 * Represents the ice transport policy. This exposes the same states in C++,
 * which include one more state than what exists in the W3C spec.
 */
typedef NS_ENUM(NSInteger, RTCIceTransportPolicy) {
  RTCIceTransportPolicyNone,
  RTCIceTransportPolicyRelay,
  RTCIceTransportPolicyNoHost,
  RTCIceTransportPolicyAll
};

/** Represents the bundle policy. */
typedef NS_ENUM(NSInteger, RTCBundlePolicy) {
  RTCBundlePolicyBalanced,
  RTCBundlePolicyMaxCompat,
  RTCBundlePolicyMaxBundle
};

/** Represents the rtcp mux policy. */
typedef NS_ENUM(NSInteger, RTCRtcpMuxPolicy) {
  RTCRtcpMuxPolicyNegotiate,
  RTCRtcpMuxPolicyRequire
};

/** Represents the tcp candidate policy. */
typedef NS_ENUM(NSInteger, RTCTcpCandidatePolicy) {
  RTCTcpCandidatePolicyEnabled,
  RTCTcpCandidatePolicyDisabled
};

NS_ASSUME_NONNULL_BEGIN

@interface RTCConfiguration : NSObject

/** An array of Ice Servers available to be used by ICE. */
@property(nonatomic, copy) NSArray<RTCIceServer *> *iceServers;

/** Which candidates the ICE agent is allowed to use. The W3C calls it
 * |iceTransportPolicy|, while in C++ it is called |type|. */
@property(nonatomic, assign) RTCIceTransportPolicy iceTransportPolicy;

/** The media-bundling policy to use when gathering ICE candidates. */
@property(nonatomic, assign) RTCBundlePolicy bundlePolicy;

/** The rtcp-mux policy to use when gathering ICE candidates. */
@property(nonatomic, assign) RTCRtcpMuxPolicy rtcpMuxPolicy;
@property(nonatomic, assign) RTCTcpCandidatePolicy tcpCandidatePolicy;
@property(nonatomic, assign) int audioJitterBufferMaxPackets;
@property(nonatomic, assign) int iceConnectionReceivingTimeout;
@property(nonatomic, assign) int iceBackupCandidatePairPingInterval;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithIceServers:
    (nullable NSArray<RTCIceServer *> *)iceServers
                iceTransportPolicy:(RTCIceTransportPolicy)iceTransportPolicy
                      bundlePolicy:(RTCBundlePolicy)bundlePolicy
                     rtcpMuxPolicy:(RTCRtcpMuxPolicy)rtcpMuxPolicy
                tcpCandidatePolicy:(RTCTcpCandidatePolicy)tcpCandidatePolicy
       audioJitterBufferMaxPackets:(int)audioJitterBufferMaxPackets
     iceConnectionReceivingTimeout:(int)iceConnectionReceivingTimeout
iceBackupCandidatePairPingInterval:(int)iceBackupCandidatePairPingInterval;

@end

NS_ASSUME_NONNULL_END
