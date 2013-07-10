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

#ifndef TALK_BASE_NSSIDENTITY_H_
#define TALK_BASE_NSSIDENTITY_H_

#include <string>

#include "cert.h"
#include "nspr.h"
#include "hasht.h"
#include "keythi.h"

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sslidentity.h"

namespace talk_base {

class NSSKeyPair {
 public:
  NSSKeyPair(SECKEYPrivateKey* privkey, SECKEYPublicKey* pubkey) :
      privkey_(privkey), pubkey_(pubkey) {}
  ~NSSKeyPair();

  // Generate a 1024-bit RSA key pair.
  static NSSKeyPair* Generate();
  NSSKeyPair* GetReference();

  SECKEYPrivateKey* privkey() const { return privkey_; }
  SECKEYPublicKey * pubkey() const { return pubkey_; }

 private:
  SECKEYPrivateKey* privkey_;
  SECKEYPublicKey* pubkey_;

  DISALLOW_EVIL_CONSTRUCTORS(NSSKeyPair);
};


class NSSCertificate : public SSLCertificate {
 public:
  static NSSCertificate* FromPEMString(const std::string& pem_string);
  explicit NSSCertificate(CERTCertificate* cert) : certificate_(cert) {}
  virtual ~NSSCertificate() {
    if (certificate_)
      CERT_DestroyCertificate(certificate_);
  }

  virtual NSSCertificate* GetReference() const;

  virtual std::string ToPEMString() const;

  virtual bool ComputeDigest(const std::string& algorithm,
                             unsigned char* digest, std::size_t size,
                             std::size_t* length) const;

  CERTCertificate* certificate() { return certificate_; }

  // Helper function to get the length of a digest
  static bool GetDigestLength(const std::string& algorithm,
                              std::size_t* length);

  // Comparison
  bool Equals(const NSSCertificate* tocompare) const;

 private:
  static bool GetDigestObject(const std::string& algorithm,
                              const SECHashObject** hash_object);

  CERTCertificate* certificate_;

  DISALLOW_EVIL_CONSTRUCTORS(NSSCertificate);
};

// Represents a SSL key pair and certificate for NSS.
class NSSIdentity : public SSLIdentity {
 public:
  static NSSIdentity* Generate(const std::string& common_name);
  static SSLIdentity* FromPEMStrings(const std::string& private_key,
                                     const std::string& certificate);
  virtual ~NSSIdentity() {
    LOG(LS_INFO) << "Destroying NSS identity";
  }

  virtual NSSIdentity* GetReference() const;
  virtual NSSCertificate& certificate() const;

  NSSKeyPair* keypair() const { return keypair_.get(); }

 private:
  NSSIdentity(NSSKeyPair* keypair, NSSCertificate* cert) :
      keypair_(keypair), certificate_(cert) {}

  talk_base::scoped_ptr<NSSKeyPair> keypair_;
  talk_base::scoped_ptr<NSSCertificate> certificate_;

  DISALLOW_EVIL_CONSTRUCTORS(NSSIdentity);
};

}  // namespace talk_base

#endif  // TALK_BASE_NSSIDENTITY_H_
