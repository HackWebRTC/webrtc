/*
 * libjingle
 * Copyright 2004--2008, Google Inc.
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

#ifndef TALK_BASE_OPENSSLIDENTITY_H_
#define TALK_BASE_OPENSSLIDENTITY_H_

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <string>

#include "talk/base/common.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sslidentity.h"

typedef struct ssl_ctx_st SSL_CTX;

namespace talk_base {

// OpenSSLKeyPair encapsulates an OpenSSL EVP_PKEY* keypair object,
// which is reference counted inside the OpenSSL library.
class OpenSSLKeyPair {
 public:
  explicit OpenSSLKeyPair(EVP_PKEY* pkey) : pkey_(pkey) {
    ASSERT(pkey_ != NULL);
  }

  static OpenSSLKeyPair* Generate();

  virtual ~OpenSSLKeyPair();

  virtual OpenSSLKeyPair* GetReference() {
    AddReference();
    return new OpenSSLKeyPair(pkey_);
  }

  EVP_PKEY* pkey() const { return pkey_; }

 private:
  void AddReference();

  EVP_PKEY* pkey_;

  DISALLOW_EVIL_CONSTRUCTORS(OpenSSLKeyPair);
};

// OpenSSLCertificate encapsulates an OpenSSL X509* certificate object,
// which is also reference counted inside the OpenSSL library.
class OpenSSLCertificate : public SSLCertificate {
 public:
  // Caller retains ownership of the X509 object.
  explicit OpenSSLCertificate(X509* x509) : x509_(x509) {
    AddReference();
  }

  static OpenSSLCertificate* Generate(OpenSSLKeyPair* key_pair,
                                      const std::string& common_name);
  static OpenSSLCertificate* FromPEMString(const std::string& pem_string);

  virtual ~OpenSSLCertificate();

  virtual OpenSSLCertificate* GetReference() const {
    return new OpenSSLCertificate(x509_);
  }

  X509* x509() const { return x509_; }

  virtual std::string ToPEMString() const;

  virtual void ToDER(Buffer* der_buffer) const;

  // Compute the digest of the certificate given algorithm
  virtual bool ComputeDigest(const std::string &algorithm,
                             unsigned char *digest, std::size_t size,
                             std::size_t *length) const;

  // Compute the digest of a certificate as an X509 *
  static bool ComputeDigest(const X509 *x509,
                            const std::string &algorithm,
                            unsigned char *digest,
                            std::size_t size,
                            std::size_t *length);

  virtual bool GetChain(SSLCertChain** chain) const {
    // Chains are not yet supported when using OpenSSL.
    // OpenSSLStreamAdapter::SSLVerifyCallback currently requires the remote
    // certificate to be self-signed.
    return false;
  }

 private:
  void AddReference() const;

  X509* x509_;

  DISALLOW_EVIL_CONSTRUCTORS(OpenSSLCertificate);
};

// Holds a keypair and certificate together, and a method to generate
// them consistently.
class OpenSSLIdentity : public SSLIdentity {
 public:
  static OpenSSLIdentity* Generate(const std::string& common_name);
  static SSLIdentity* FromPEMStrings(const std::string& private_key,
                                     const std::string& certificate);
  virtual ~OpenSSLIdentity() { }

  virtual const OpenSSLCertificate& certificate() const {
    return *certificate_;
  }

  virtual OpenSSLIdentity* GetReference() const {
    return new OpenSSLIdentity(key_pair_->GetReference(),
                               certificate_->GetReference());
  }

  // Configure an SSL context object to use our key and certificate.
  bool ConfigureIdentity(SSL_CTX* ctx);

 private:
  OpenSSLIdentity(OpenSSLKeyPair* key_pair,
                  OpenSSLCertificate* certificate)
      : key_pair_(key_pair), certificate_(certificate) {
    ASSERT(key_pair != NULL);
    ASSERT(certificate != NULL);
  }

  scoped_ptr<OpenSSLKeyPair> key_pair_;
  scoped_ptr<OpenSSLCertificate> certificate_;

  DISALLOW_EVIL_CONSTRUCTORS(OpenSSLIdentity);
};


}  // namespace talk_base

#endif  // TALK_BASE_OPENSSLIDENTITY_H_
