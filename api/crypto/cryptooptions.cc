/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/crypto/cryptooptions.h"
#include "rtc_base/sslstreamadapter.h"

namespace webrtc {

CryptoOptions::CryptoOptions() {}

CryptoOptions::CryptoOptions(const CryptoOptions& other) {
  enable_gcm_crypto_suites = other.enable_gcm_crypto_suites;
  enable_encrypted_rtp_header_extensions =
      other.enable_encrypted_rtp_header_extensions;
  srtp = other.srtp;
}

CryptoOptions::~CryptoOptions() {}

// static
CryptoOptions CryptoOptions::NoGcm() {
  CryptoOptions options;
  options.srtp.enable_gcm_crypto_suites = false;
  return options;
}

std::vector<int> CryptoOptions::GetSupportedDtlsSrtpCryptoSuites() const {
  std::vector<int> crypto_suites;
  if (srtp.enable_gcm_crypto_suites) {
    crypto_suites.push_back(rtc::SRTP_AEAD_AES_256_GCM);
    crypto_suites.push_back(rtc::SRTP_AEAD_AES_128_GCM);
  }
  // Note: SRTP_AES128_CM_SHA1_80 is what is required to be supported (by
  // draft-ietf-rtcweb-security-arch), but SRTP_AES128_CM_SHA1_32 is allowed as
  // well, and saves a few bytes per packet if it ends up selected.
  // As the cipher suite is potentially insecure, it will only be used if
  // enabled by both peers.
  if (srtp.enable_aes128_sha1_32_crypto_cipher) {
    crypto_suites.push_back(rtc::SRTP_AES128_CM_SHA1_32);
  }
  crypto_suites.push_back(rtc::SRTP_AES128_CM_SHA1_80);
  return crypto_suites;
}

}  // namespace webrtc
