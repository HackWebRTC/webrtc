/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCIceServer+Private.h"
#import "RTCSSLConfig+Native.h"

#import "helpers/NSString+StdString.h"

@implementation RTCIceServer

@synthesize urlStrings = _urlStrings;
@synthesize username = _username;
@synthesize credential = _credential;
@synthesize tlsCertPolicy = _tlsCertPolicy;
@synthesize hostname = _hostname;
@synthesize tlsAlpnProtocols = _tlsAlpnProtocols;
@synthesize tlsEllipticCurves = _tlsEllipticCurves;
@synthesize sslConfig = _sslConfig;

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings {
  return [self initWithURLStrings:urlStrings
                         username:nil
                       credential:nil];
}

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings
                          username:(NSString *)username
                        credential:(NSString *)credential {
  return [self initWithURLStrings:urlStrings
                         username:username
                       credential:credential
                    tlsCertPolicy:RTCTlsCertPolicySecure];
}

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings
                          username:(NSString *)username
                        credential:(NSString *)credential
                     tlsCertPolicy:(RTCTlsCertPolicy)tlsCertPolicy {
  return [self initWithURLStrings:urlStrings
                         username:username
                       credential:credential
                    tlsCertPolicy:tlsCertPolicy
                         hostname:nil];
}

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings
                          username:(NSString *)username
                        credential:(NSString *)credential
                     tlsCertPolicy:(RTCTlsCertPolicy)tlsCertPolicy
                          hostname:(NSString *)hostname {
  return [self initWithURLStrings:urlStrings
                         username:username
                       credential:credential
                    tlsCertPolicy:tlsCertPolicy
                         hostname:hostname
                 tlsAlpnProtocols:[NSArray array]];
}

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings
                          username:(NSString *)username
                        credential:(NSString *)credential
                     tlsCertPolicy:(RTCTlsCertPolicy)tlsCertPolicy
                          hostname:(NSString *)hostname
                  tlsAlpnProtocols:(NSArray<NSString *> *)tlsAlpnProtocols {
  return [self initWithURLStrings:urlStrings
                         username:username
                       credential:credential
                    tlsCertPolicy:tlsCertPolicy
                         hostname:hostname
                 tlsAlpnProtocols:tlsAlpnProtocols
                tlsEllipticCurves:[NSArray array]];
}

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings
                          username:(NSString *)username
                        credential:(NSString *)credential
                     tlsCertPolicy:(RTCTlsCertPolicy)tlsCertPolicy
                          hostname:(NSString *)hostname
                  tlsAlpnProtocols:(NSArray<NSString *> *)tlsAlpnProtocols
                 tlsEllipticCurves:(NSArray<NSString *> *)tlsEllipticCurves {
  RTCSSLConfig *sslConfig = [[RTCSSLConfig alloc] init];
  sslConfig.tlsCertPolicy = tlsCertPolicy;
  sslConfig.tlsALPNProtocols = [[NSArray alloc] initWithArray:tlsAlpnProtocols copyItems:YES];
  sslConfig.tlsEllipticCurves = [[NSArray alloc] initWithArray:tlsEllipticCurves copyItems:YES];
  return [self initWithURLStrings:urlStrings
                         username:username
                       credential:credential
                         hostname:hostname
                        sslConfig:sslConfig];
}

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings
                          username:(NSString *)username
                        credential:(NSString *)credential
                          hostname:(NSString *)hostname
                         sslConfig:(RTCSSLConfig *)sslConfig {
  NSParameterAssert(urlStrings.count);
  if (self = [super init]) {
    _urlStrings = [[NSArray alloc] initWithArray:urlStrings copyItems:YES];
    _username = [username copy];
    _credential = [credential copy];
    _hostname = [hostname copy];
    _sslConfig = sslConfig;

    // TODO(diogor, webrtc:9673): Remove these duplicate assignments.
    _tlsCertPolicy = sslConfig.tlsCertPolicy;
    if (sslConfig.tlsALPNProtocols) {
      _tlsAlpnProtocols = [[NSArray alloc] initWithArray:sslConfig.tlsALPNProtocols copyItems:YES];
    }
    if (sslConfig.tlsEllipticCurves) {
      _tlsEllipticCurves =
          [[NSArray alloc] initWithArray:sslConfig.tlsEllipticCurves copyItems:YES];
    }
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCIceServer:\n%@\n%@\n%@\n%@\n%@",
                                    _urlStrings,
                                    _username,
                                    _credential,
                                    _hostname,
                                    _sslConfig];
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

- (webrtc::PeerConnectionInterface::IceServer)nativeServer {
  __block webrtc::PeerConnectionInterface::IceServer iceServer;

  iceServer.username = [NSString stdStringForString:_username];
  iceServer.password = [NSString stdStringForString:_credential];
  iceServer.hostname = [NSString stdStringForString:_hostname];

  [_tlsAlpnProtocols enumerateObjectsUsingBlock:^(NSString *proto, NSUInteger idx, BOOL *stop) {
    iceServer.tls_alpn_protocols.push_back(proto.stdString);
  }];

  [_tlsEllipticCurves enumerateObjectsUsingBlock:^(NSString *curve, NSUInteger idx, BOOL *stop) {
    iceServer.tls_elliptic_curves.push_back(curve.stdString);
  }];

  [_urlStrings enumerateObjectsUsingBlock:^(NSString *url,
                                            NSUInteger idx,
                                            BOOL *stop) {
    iceServer.urls.push_back(url.stdString);
  }];

  switch (_tlsCertPolicy) {
    case RTCTlsCertPolicySecure:
      iceServer.tls_cert_policy =
          webrtc::PeerConnectionInterface::kTlsCertPolicySecure;
      break;
    case RTCTlsCertPolicyInsecureNoCheck:
      iceServer.tls_cert_policy =
          webrtc::PeerConnectionInterface::kTlsCertPolicyInsecureNoCheck;
      break;
  }

  iceServer.ssl_config = [_sslConfig nativeConfig];
  return iceServer;
}

- (instancetype)initWithNativeServer:
    (webrtc::PeerConnectionInterface::IceServer)nativeServer {
  NSMutableArray *urls =
      [NSMutableArray arrayWithCapacity:nativeServer.urls.size()];
  for (auto const &url : nativeServer.urls) {
    [urls addObject:[NSString stringForStdString:url]];
  }
  NSString *username = [NSString stringForStdString:nativeServer.username];
  NSString *credential = [NSString stringForStdString:nativeServer.password];
  NSString *hostname = [NSString stringForStdString:nativeServer.hostname];
  RTCSSLConfig *sslConfig = [[RTCSSLConfig alloc] initWithNativeConfig:nativeServer.ssl_config];

  if (!nativeServer.ssl_config.tls_alpn_protocols.has_value() &&
      !nativeServer.tls_alpn_protocols.empty()) {
    NSMutableArray *tlsALPNProtocols =
        [NSMutableArray arrayWithCapacity:nativeServer.tls_alpn_protocols.size()];
    for (auto const &proto : nativeServer.tls_alpn_protocols) {
      [tlsALPNProtocols addObject:[NSString stringForStdString:proto]];
    }
    sslConfig.tlsALPNProtocols = tlsALPNProtocols;
  }

  if (!nativeServer.ssl_config.tls_elliptic_curves.has_value() &&
      !nativeServer.tls_elliptic_curves.empty()) {
    NSMutableArray *tlsEllipticCurves =
        [NSMutableArray arrayWithCapacity:nativeServer.tls_elliptic_curves.size()];
    for (auto const &curve : nativeServer.tls_elliptic_curves) {
      [tlsEllipticCurves addObject:[NSString stringForStdString:curve]];
    }
    sslConfig.tlsEllipticCurves = tlsEllipticCurves;
  }

  if (nativeServer.tls_cert_policy ==
      webrtc::PeerConnectionInterface::kTlsCertPolicyInsecureNoCheck) {
    sslConfig.tlsCertPolicy = RTCTlsCertPolicyInsecureNoCheck;
  }

  self = [self initWithURLStrings:urls
                         username:username
                       credential:credential
                         hostname:hostname
                        sslConfig:sslConfig];
  return self;
}

@end
