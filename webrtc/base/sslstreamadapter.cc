/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/base/sslconfig.h"

#if SSL_USE_OPENSSL

#include "webrtc/base/opensslstreamadapter.h"

#endif  // SSL_USE_OPENSSL

///////////////////////////////////////////////////////////////////////////////

namespace rtc {

// TODO(guoweis): Move this to SDP layer and use int form internally.
// webrtc:5043.
const char CS_AES_CM_128_HMAC_SHA1_80[] = "AES_CM_128_HMAC_SHA1_80";
const char CS_AES_CM_128_HMAC_SHA1_32[] = "AES_CM_128_HMAC_SHA1_32";

int GetSrtpCryptoSuiteFromName(const std::string& cipher) {
  if (cipher == CS_AES_CM_128_HMAC_SHA1_32)
    return SRTP_AES128_CM_SHA1_32;
  if (cipher == CS_AES_CM_128_HMAC_SHA1_80)
    return SRTP_AES128_CM_SHA1_80;
  return 0;
}

SSLStreamAdapter* SSLStreamAdapter::Create(StreamInterface* stream) {
#if SSL_USE_OPENSSL
  return new OpenSSLStreamAdapter(stream);
#else  // !SSL_USE_OPENSSL
  return NULL;
#endif  // SSL_USE_OPENSSL
}

bool SSLStreamAdapter::GetSslCipherSuite(int* cipher) {
  return false;
}

bool SSLStreamAdapter::ExportKeyingMaterial(const std::string& label,
                                            const uint8_t* context,
                                            size_t context_len,
                                            bool use_context,
                                            uint8_t* result,
                                            size_t result_len) {
  return false;  // Default is unsupported
}

bool SSLStreamAdapter::SetDtlsSrtpCiphers(
    const std::vector<std::string>& ciphers) {
  return false;
}

bool SSLStreamAdapter::GetDtlsSrtpCipher(std::string* cipher) {
  return false;
}

#if SSL_USE_OPENSSL
bool SSLStreamAdapter::HaveDtls() {
  return OpenSSLStreamAdapter::HaveDtls();
}
bool SSLStreamAdapter::HaveDtlsSrtp() {
  return OpenSSLStreamAdapter::HaveDtlsSrtp();
}
bool SSLStreamAdapter::HaveExporter() {
  return OpenSSLStreamAdapter::HaveExporter();
}
int SSLStreamAdapter::GetDefaultSslCipherForTest(SSLProtocolVersion version,
                                                 KeyType key_type) {
  return OpenSSLStreamAdapter::GetDefaultSslCipherForTest(version, key_type);
}

std::string SSLStreamAdapter::GetSslCipherSuiteName(int cipher) {
  return OpenSSLStreamAdapter::GetSslCipherSuiteName(cipher);
}
#endif  // SSL_USE_OPENSSL

///////////////////////////////////////////////////////////////////////////////

}  // namespace rtc
