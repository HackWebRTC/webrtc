/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCSSLConfig+Native.h"

#import "helpers/NSString+StdString.h"

@implementation RTCSSLConfig

@synthesize enableOCSPStapling = _enableOCSPStapling;
@synthesize enableSignedCertTimestamp = _enableSignedCertTimestamp;
@synthesize enableTlsChannelId = _enableTlsChannelId;
@synthesize enableGrease = _enableGrease;
@synthesize tlsCertPolicy = _tlsCertPolicy;
@synthesize maxSSLVersion = _maxSSLVersion;
@synthesize tlsALPNProtocols = _tlsALPNProtocols;
@synthesize tlsEllipticCurves = _tlsEllipticCurves;

- (instancetype)init {
  // Copy defaults
  rtc::SSLConfig config;
  return [self initWithNativeConfig:config];
}

- (instancetype)initWithNativeConfig:(const rtc::SSLConfig &)config {
  if (self = [super init]) {
    _enableOCSPStapling = config.enable_ocsp_stapling;
    _enableSignedCertTimestamp = config.enable_signed_cert_timestamp;
    _enableTlsChannelId = config.enable_tls_channel_id;
    _enableGrease = config.enable_grease;

    switch (config.tls_cert_policy) {
      case rtc::TlsCertPolicy::TLS_CERT_POLICY_SECURE:
        _tlsCertPolicy = RTCTlsCertPolicySecure;
        break;
      case rtc::TlsCertPolicy::TLS_CERT_POLICY_INSECURE_NO_CHECK:
        _tlsCertPolicy = RTCTlsCertPolicyInsecureNoCheck;
        break;
    }

    if (config.max_ssl_version) {
      _maxSSLVersion = [NSNumber numberWithInt:*config.max_ssl_version];
    }
    if (config.tls_alpn_protocols) {
      NSMutableArray *tlsALPNProtocols =
          [NSMutableArray arrayWithCapacity:config.tls_alpn_protocols.value().size()];
      for (auto const &proto : config.tls_alpn_protocols.value()) {
        [tlsALPNProtocols addObject:[NSString stringForStdString:proto]];
      }
      _tlsALPNProtocols = tlsALPNProtocols;
    }
    if (config.tls_elliptic_curves) {
      NSMutableArray *tlsEllipticCurves =
          [NSMutableArray arrayWithCapacity:config.tls_elliptic_curves.value().size()];
      for (auto const &curve : config.tls_elliptic_curves.value()) {
        [tlsEllipticCurves addObject:[NSString stringForStdString:curve]];
      }
      _tlsEllipticCurves = tlsEllipticCurves;
    }
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCSSLConfig:\n%d\n%d\n%d\n%d\n%@\n%@\n%@\n%@",
                                    _enableOCSPStapling,
                                    _enableSignedCertTimestamp,
                                    _enableTlsChannelId,
                                    _enableGrease,
                                    [self stringForTlsCertPolicy:_tlsCertPolicy],
                                    _maxSSLVersion,
                                    _tlsALPNProtocols,
                                    _tlsEllipticCurves];
}

#pragma mark - Private

- (NSString *)stringForTlsCertPolicy:(RTCTlsCertPolicy)tlsCertPolicy {
  switch (tlsCertPolicy) {
    case RTCTlsCertPolicySecure:
      return @"RTCTlsCertPolicySecure";
    case RTCTlsCertPolicyInsecureNoCheck:
      return @"RTCTlsCertPolicyInsecureNoCheck";
  }
}

- (rtc::SSLConfig)nativeConfig {
  __block rtc::SSLConfig sslConfig;

  sslConfig.enable_ocsp_stapling = _enableOCSPStapling;
  sslConfig.enable_signed_cert_timestamp = _enableSignedCertTimestamp;
  sslConfig.enable_tls_channel_id = _enableTlsChannelId;
  sslConfig.enable_grease = _enableGrease;

  switch (_tlsCertPolicy) {
    case RTCTlsCertPolicySecure:
      sslConfig.tls_cert_policy = rtc::TlsCertPolicy::TLS_CERT_POLICY_SECURE;
      break;
    case RTCTlsCertPolicyInsecureNoCheck:
      sslConfig.tls_cert_policy = rtc::TlsCertPolicy::TLS_CERT_POLICY_INSECURE_NO_CHECK;
      break;
  }

  if (_maxSSLVersion != nil) {
    sslConfig.max_ssl_version = absl::optional<int>(_maxSSLVersion.intValue);
  }

  if (_tlsALPNProtocols != nil) {
    __block std::vector<std::string> alpn_protocols;
    [_tlsALPNProtocols enumerateObjectsUsingBlock:^(NSString *proto, NSUInteger idx, BOOL *stop) {
      alpn_protocols.push_back(proto.stdString);
    }];
    sslConfig.tls_alpn_protocols = absl::optional<std::vector<std::string>>(alpn_protocols);
  }

  if (_tlsEllipticCurves != nil) {
    __block std::vector<std::string> elliptic_curves;
    [_tlsEllipticCurves enumerateObjectsUsingBlock:^(NSString *curve, NSUInteger idx, BOOL *stop) {
      elliptic_curves.push_back(curve.stdString);
    }];
    sslConfig.tls_elliptic_curves = absl::optional<std::vector<std::string>>(elliptic_curves);
  }

  return sslConfig;
}

@end
