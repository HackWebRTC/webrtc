/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Handling of certificates and keypairs for SSLStreamAdapter's peer mode.
#if HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#include "webrtc/base/sslidentity.h"

#include <string>

#include "webrtc/base/base64.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/sslconfig.h"

#if SSL_USE_OPENSSL

#include "webrtc/base/opensslidentity.h"

#endif  // SSL_USE_OPENSSL

namespace rtc {

const char kPemTypeCertificate[] = "CERTIFICATE";
const char kPemTypeRsaPrivateKey[] = "RSA PRIVATE KEY";
const char kPemTypeEcPrivateKey[] = "EC PRIVATE KEY";

KeyParams::KeyParams(KeyType key_type) {
  if (key_type == KT_ECDSA) {
    type_ = KT_ECDSA;
    params_.curve = EC_NIST_P256;
  } else if (key_type == KT_RSA) {
    type_ = KT_RSA;
    params_.rsa.mod_size = kRsaDefaultModSize;
    params_.rsa.pub_exp = kRsaDefaultExponent;
  } else {
    RTC_NOTREACHED();
  }
}

// static
KeyParams KeyParams::RSA(int mod_size, int pub_exp) {
  KeyParams kt(KT_RSA);
  kt.params_.rsa.mod_size = mod_size;
  kt.params_.rsa.pub_exp = pub_exp;
  return kt;
}

// static
KeyParams KeyParams::ECDSA(ECCurve curve) {
  KeyParams kt(KT_ECDSA);
  kt.params_.curve = curve;
  return kt;
}

bool KeyParams::IsValid() const {
  if (type_ == KT_RSA) {
    return (params_.rsa.mod_size >= kRsaMinModSize &&
            params_.rsa.mod_size <= kRsaMaxModSize &&
            params_.rsa.pub_exp > params_.rsa.mod_size);
  } else if (type_ == KT_ECDSA) {
    return (params_.curve == EC_NIST_P256);
  }
  return false;
}

RSAParams KeyParams::rsa_params() const {
  RTC_DCHECK(type_ == KT_RSA);
  return params_.rsa;
}

ECCurve KeyParams::ec_curve() const {
  RTC_DCHECK(type_ == KT_ECDSA);
  return params_.curve;
}

KeyType IntKeyTypeFamilyToKeyType(int key_type_family) {
  return static_cast<KeyType>(key_type_family);
}

bool SSLIdentity::PemToDer(const std::string& pem_type,
                           const std::string& pem_string,
                           std::string* der) {
  // Find the inner body. We need this to fulfill the contract of
  // returning pem_length.
  size_t header = pem_string.find("-----BEGIN " + pem_type + "-----");
  if (header == std::string::npos)
    return false;

  size_t body = pem_string.find("\n", header);
  if (body == std::string::npos)
    return false;

  size_t trailer = pem_string.find("-----END " + pem_type + "-----");
  if (trailer == std::string::npos)
    return false;

  std::string inner = pem_string.substr(body + 1, trailer - (body + 1));

  *der = Base64::Decode(inner, Base64::DO_PARSE_WHITE |
                        Base64::DO_PAD_ANY |
                        Base64::DO_TERM_BUFFER);
  return true;
}

std::string SSLIdentity::DerToPem(const std::string& pem_type,
                                  const unsigned char* data,
                                  size_t length) {
  std::stringstream result;

  result << "-----BEGIN " << pem_type << "-----\n";

  std::string b64_encoded;
  Base64::EncodeFromArray(data, length, &b64_encoded);

  // Divide the Base-64 encoded data into 64-character chunks, as per
  // 4.3.2.4 of RFC 1421.
  static const size_t kChunkSize = 64;
  size_t chunks = (b64_encoded.size() + (kChunkSize - 1)) / kChunkSize;
  for (size_t i = 0, chunk_offset = 0; i < chunks;
       ++i, chunk_offset += kChunkSize) {
    result << b64_encoded.substr(chunk_offset, kChunkSize);
    result << "\n";
  }

  result << "-----END " << pem_type << "-----\n";

  return result.str();
}

SSLCertChain::SSLCertChain(const std::vector<SSLCertificate*>& certs) {
  ASSERT(!certs.empty());
  certs_.resize(certs.size());
  std::transform(certs.begin(), certs.end(), certs_.begin(), DupCert);
}

SSLCertChain::SSLCertChain(const SSLCertificate* cert) {
  certs_.push_back(cert->GetReference());
}

SSLCertChain::~SSLCertChain() {
  std::for_each(certs_.begin(), certs_.end(), DeleteCert);
}

#if SSL_USE_OPENSSL

SSLCertificate* SSLCertificate::FromPEMString(const std::string& pem_string) {
  return OpenSSLCertificate::FromPEMString(pem_string);
}

SSLIdentity* SSLIdentity::Generate(const std::string& common_name,
                                   const KeyParams& key_params) {
  return OpenSSLIdentity::Generate(common_name, key_params);
}

SSLIdentity* SSLIdentity::GenerateForTest(const SSLIdentityParams& params) {
  return OpenSSLIdentity::GenerateForTest(params);
}

SSLIdentity* SSLIdentity::FromPEMStrings(const std::string& private_key,
                                         const std::string& certificate) {
  return OpenSSLIdentity::FromPEMStrings(private_key, certificate);
}

#else  // !SSL_USE_OPENSSL

#error "No SSL implementation"

#endif  // SSL_USE_OPENSSL

}  // namespace rtc
