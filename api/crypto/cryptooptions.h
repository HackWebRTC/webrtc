/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CRYPTO_CRYPTOOPTIONS_H_
#define API_CRYPTO_CRYPTOOPTIONS_H_

#include <vector>
#include "absl/types/optional.h"

namespace webrtc {

// CryptoOptions defines advanced cryptographic settings for native WebRTC.
// These settings must be passed into PeerConnectionFactoryInterface::Options
// and are only applicable to native use cases of WebRTC.
struct CryptoOptions {
  CryptoOptions();
  CryptoOptions(const CryptoOptions& other);
  ~CryptoOptions();

  // Helper method to return an instance of the CryptoOptions with GCM crypto
  // suites disabled. This method should be used instead of depending on current
  // default values set by the constructor.
  static CryptoOptions NoGcm();

  // Returns a list of the supported DTLS-SRTP Crypto suites based on this set
  // of crypto options.
  std::vector<int> GetSupportedDtlsSrtpCryptoSuites() const;

  // TODO(webrtc:9859) - Remove duplicates once chromium is fixed.
  // Will be removed once srtp.enable_gcm_crypto_suites is updated in Chrome.
  absl::optional<bool> enable_gcm_crypto_suites;
  // TODO(webrtc:9859) - Remove duplicates once chromium is fixed.
  // Will be removed once srtp.enable_encrypted_rtp_header_extensions is
  // updated in Chrome.
  absl::optional<bool> enable_encrypted_rtp_header_extensions;
  // Will be removed once srtp.enable_encrypted_rtp_header_extensions is
  // updated in Tacl.
  absl::optional<bool> enable_aes128_sha1_32_crypto_cipher;

  // SRTP Related Peer Connection options.
  struct Srtp {
    // Enable GCM crypto suites from RFC 7714 for SRTP. GCM will only be used
    // if both sides enable it.
    bool enable_gcm_crypto_suites = false;

    // If set to true, the (potentially insecure) crypto cipher
    // SRTP_AES128_CM_SHA1_32 will be included in the list of supported ciphers
    // during negotiation. It will only be used if both peers support it and no
    // other ciphers get preferred.
    bool enable_aes128_sha1_32_crypto_cipher = false;

    // If set to true, encrypted RTP header extensions as defined in RFC 6904
    // will be negotiated. They will only be used if both peers support them.
    bool enable_encrypted_rtp_header_extensions = false;
  } srtp;
};

}  // namespace webrtc

#endif  // API_CRYPTO_CRYPTOOPTIONS_H_
