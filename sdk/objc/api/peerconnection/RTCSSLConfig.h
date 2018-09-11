/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCMacros.h>

typedef NS_ENUM(NSUInteger, RTCTlsCertPolicy) {
  RTCTlsCertPolicySecure,
  RTCTlsCertPolicyInsecureNoCheck
};

NS_ASSUME_NONNULL_BEGIN

RTC_EXPORT
@interface RTCSSLConfig : NSObject

/** Indicates whether to enable OCSP stapling in TLS. */
@property(nonatomic) BOOL enableOCSPStapling;

/** Indicates whether to enable the signed certificate timestamp extension in TLS. */
@property(nonatomic) BOOL enableSignedCertTimestamp;

/** Indicates whether to enable the TLS Channel ID extension. */
@property(nonatomic) BOOL enableTlsChannelId;

/** Indicates whether to enable the TLS GREASE extension. */
@property(nonatomic) BOOL enableGrease;

/** Indicates how to process TURN server certificates */
@property(nonatomic) RTCTlsCertPolicy tlsCertPolicy;

/** Highest supported SSL version, as defined in the supported_versions TLS extension. */
@property(nonatomic, nullable) NSNumber *maxSSLVersion;

/** List of protocols to be used in the TLS ALPN extension. */
@property(nonatomic, copy, nullable) NSArray<NSString *> *tlsALPNProtocols;

/**
 List of elliptic curves to be used in the TLS elliptic curves extension.
 Only curve names supported by OpenSSL should be used (eg. "P-256","X25519").
 */
@property(nonatomic, copy, nullable) NSArray<NSString *> *tlsEllipticCurves;

- (instancetype)init;

@end

NS_ASSUME_NONNULL_END
