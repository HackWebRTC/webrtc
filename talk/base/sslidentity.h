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

#ifndef TALK_BASE_SSLIDENTITY_H_
#define TALK_BASE_SSLIDENTITY_H_

#include <string>
#include "talk/base/messagedigest.h"

namespace talk_base {

// Abstract interface overridden by SSL library specific
// implementations.

// A somewhat opaque type used to encapsulate a certificate.
// Wraps the SSL library's notion of a certificate, with reference counting.
// The SSLCertificate object is pretty much immutable once created.
// (The OpenSSL implementation only does reference counting and
// possibly caching of intermediate results.)
class SSLCertificate {
 public:
  // Parses and build a certificate from a PEM encoded string.
  // Returns NULL on failure.
  // The length of the string representation of the certificate is
  // stored in *pem_length if it is non-NULL, and only if
  // parsing was successful.
  // Caller is responsible for freeing the returned object.
  static SSLCertificate* FromPEMString(const std::string& pem_string);
  virtual ~SSLCertificate() {}

  // Returns a new SSLCertificate object instance wrapping the same
  // underlying certificate.
  // Caller is responsible for freeing the returned object.
  virtual SSLCertificate* GetReference() const = 0;

  // Returns a PEM encoded string representation of the certificate.
  virtual std::string ToPEMString() const = 0;

  // Compute the digest of the certificate given algorithm
  virtual bool ComputeDigest(const std::string &algorithm,
                             unsigned char* digest, std::size_t size,
                             std::size_t* length) const = 0;
};

// Our identity in an SSL negotiation: a keypair and certificate (both
// with the same public key).
// This too is pretty much immutable once created.
class SSLIdentity {
 public:
  // Generates an identity (keypair and self-signed certificate). If
  // common_name is non-empty, it will be used for the certificate's
  // subject and issuer name, otherwise a random string will be used.
  // Returns NULL on failure.
  // Caller is responsible for freeing the returned object.
  static SSLIdentity* Generate(const std::string& common_name);

  // Construct an identity from a private key and a certificate.
  static SSLIdentity* FromPEMStrings(const std::string& private_key,
                                     const std::string& certificate);

  virtual ~SSLIdentity() {}

  // Returns a new SSLIdentity object instance wrapping the same
  // identity information.
  // Caller is responsible for freeing the returned object.
  virtual SSLIdentity* GetReference() const = 0;

  // Returns a temporary reference to the certificate.
  virtual const SSLCertificate& certificate() const = 0;

  // Helpers for parsing converting between PEM and DER format.
  static bool PemToDer(const std::string& pem_type,
                       const std::string& pem_string,
                       std::string* der);
  static std::string DerToPem(const std::string& pem_type,
                              const unsigned char* data,
                              size_t length);
};

extern const char kPemTypeCertificate[];
extern const char kPemTypeRsaPrivateKey[];

}  // namespace talk_base

#endif  // TALK_BASE_SSLIDENTITY_H__
