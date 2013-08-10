/*
 * libjingle
 * Copyright 2004, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Handling of certificates and keypairs for SSLStreamAdapter's peer mode.
#if HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#include "talk/base/sslidentity.h"

#include <string>

#include "talk/base/base64.h"
#include "talk/base/logging.h"
#include "talk/base/sslconfig.h"

#if SSL_USE_SCHANNEL

#elif SSL_USE_OPENSSL  // !SSL_USE_SCHANNEL

#include "talk/base/opensslidentity.h"

#elif SSL_USE_NSS  // !SSL_USE_SCHANNEL && !SSL_USE_OPENSSL

#include "talk/base/nssidentity.h"

#endif  // SSL_USE_SCHANNEL

namespace talk_base {

const char kPemTypeCertificate[] = "CERTIFICATE";
const char kPemTypeRsaPrivateKey[] = "RSA PRIVATE KEY";

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

#if SSL_USE_SCHANNEL

SSLCertificate* SSLCertificate::FromPEMString(const std::string& pem_string) {
  return NULL;
}

SSLIdentity* SSLIdentity::Generate(const std::string& common_name) {
  return NULL;
}

SSLIdentity* SSLIdentity::FromPEMStrings(const std::string& private_key,
                                         const std::string& certificate) {
  return NULL;
}

#elif SSL_USE_OPENSSL  // !SSL_USE_SCHANNEL

SSLCertificate* SSLCertificate::FromPEMString(const std::string& pem_string) {
  return OpenSSLCertificate::FromPEMString(pem_string);
}

SSLIdentity* SSLIdentity::Generate(const std::string& common_name) {
  return OpenSSLIdentity::Generate(common_name);
}

SSLIdentity* SSLIdentity::FromPEMStrings(const std::string& private_key,
                                         const std::string& certificate) {
  return OpenSSLIdentity::FromPEMStrings(private_key, certificate);
}

#elif SSL_USE_NSS  // !SSL_USE_OPENSSL && !SSL_USE_SCHANNEL

SSLCertificate* SSLCertificate::FromPEMString(const std::string& pem_string) {
  return NSSCertificate::FromPEMString(pem_string);
}

SSLIdentity* SSLIdentity::Generate(const std::string& common_name) {
  return NSSIdentity::Generate(common_name);
}

SSLIdentity* SSLIdentity::FromPEMStrings(const std::string& private_key,
                                         const std::string& certificate) {
  return NSSIdentity::FromPEMStrings(private_key, certificate);
}

#else  // !SSL_USE_OPENSSL && !SSL_USE_SCHANNEL && !SSL_USE_NSS

#error "No SSL implementation"

#endif  // SSL_USE_SCHANNEL

}  // namespace talk_base
