/*
 * libjingle
 * Copyright 2012, The Libjingle Authors.
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

#ifndef TALK_BASE_FAKESSLIDENTITY_H_
#define TALK_BASE_FAKESSLIDENTITY_H_

#include <algorithm>
#include <vector>

#include "talk/base/messagedigest.h"
#include "talk/base/sslidentity.h"

namespace talk_base {

class FakeSSLCertificate : public talk_base::SSLCertificate {
 public:
  explicit FakeSSLCertificate(const std::string& data) : data_(data) {}
  explicit FakeSSLCertificate(const std::vector<std::string>& certs)
      : data_(certs.front()) {
    std::vector<std::string>::const_iterator it;
    // Skip certs[0].
    for (it = certs.begin() + 1; it != certs.end(); ++it) {
      certs_.push_back(FakeSSLCertificate(*it));
    }
  }
  virtual FakeSSLCertificate* GetReference() const {
    return new FakeSSLCertificate(*this);
  }
  virtual std::string ToPEMString() const {
    return data_;
  }
  virtual void ToDER(Buffer* der_buffer) const {
    std::string der_string;
    VERIFY(SSLIdentity::PemToDer(kPemTypeCertificate, data_, &der_string));
    der_buffer->SetData(der_string.c_str(), der_string.size());
  }
  virtual bool ComputeDigest(const std::string &algorithm,
                             unsigned char *digest, std::size_t size,
                             std::size_t *length) const {
    *length = talk_base::ComputeDigest(algorithm, data_.c_str(), data_.size(),
                                       digest, size);
    return (*length != 0);
  }
  virtual bool GetChain(SSLCertChain** chain) const {
    if (certs_.empty())
      return false;
    std::vector<SSLCertificate*> new_certs(certs_.size());
    std::transform(certs_.begin(), certs_.end(), new_certs.begin(), DupCert);
    *chain = new SSLCertChain(new_certs);
    return true;
  }

 private:
  static FakeSSLCertificate* DupCert(FakeSSLCertificate cert) {
    return cert.GetReference();
  }
  std::string data_;
  std::vector<FakeSSLCertificate> certs_;
};

class FakeSSLIdentity : public talk_base::SSLIdentity {
 public:
  explicit FakeSSLIdentity(const std::string& data) : cert_(data) {}
  explicit FakeSSLIdentity(const FakeSSLCertificate& cert) : cert_(cert) {}
  virtual FakeSSLIdentity* GetReference() const {
    return new FakeSSLIdentity(*this);
  }
  virtual const FakeSSLCertificate& certificate() const { return cert_; }
 private:
  FakeSSLCertificate cert_;
};

}  // namespace talk_base

#endif  // TALK_BASE_FAKESSLIDENTITY_H_
