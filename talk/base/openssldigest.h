/*
 * libjingle
 * Copyright 2004--2012, Google Inc.
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

#ifndef TALK_BASE_OPENSSLDIGEST_H_
#define TALK_BASE_OPENSSLDIGEST_H_

#include <openssl/evp.h>

#include "talk/base/messagedigest.h"

namespace talk_base {

// An implementation of the digest class that uses OpenSSL.
class OpenSSLDigest : public MessageDigest {
 public:
  // Creates an OpenSSLDigest with |algorithm| as the hash algorithm.
  explicit OpenSSLDigest(const std::string& algorithm);
  ~OpenSSLDigest();
  // Returns the digest output size (e.g. 16 bytes for MD5).
  virtual size_t Size() const;
  // Updates the digest with |len| bytes from |buf|.
  virtual void Update(const void* buf, size_t len);
  // Outputs the digest value to |buf| with length |len|.
  virtual size_t Finish(void* buf, size_t len);

  // Helper function to look up a digest's EVP by name.
  static bool GetDigestEVP(const std::string &algorithm,
                           const EVP_MD** md);
  // Helper function to look up a digest's name by EVP.
  static bool GetDigestName(const EVP_MD* md,
                            std::string* algorithm);
  // Helper function to get the length of a digest.
  static bool GetDigestSize(const std::string &algorithm,
                            size_t* len);

 private:
  EVP_MD_CTX ctx_;
  const EVP_MD* md_;
};

}  // namespace talk_base

#endif  // TALK_BASE_OPENSSLDIGEST_H_
