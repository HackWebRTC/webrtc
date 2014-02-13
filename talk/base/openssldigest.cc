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

#if HAVE_OPENSSL_SSL_H

#include "talk/base/openssldigest.h"

#include "talk/base/common.h"
#include "talk/base/openssl.h"

namespace talk_base {

OpenSSLDigest::OpenSSLDigest(const std::string& algorithm) {
  EVP_MD_CTX_init(&ctx_);
  if (GetDigestEVP(algorithm, &md_)) {
    EVP_DigestInit_ex(&ctx_, md_, NULL);
  } else {
    md_ = NULL;
  }
}

OpenSSLDigest::~OpenSSLDigest() {
  EVP_MD_CTX_cleanup(&ctx_);
}

size_t OpenSSLDigest::Size() const {
  if (!md_) {
    return 0;
  }
  return EVP_MD_size(md_);
}

void OpenSSLDigest::Update(const void* buf, size_t len) {
  if (!md_) {
    return;
  }
  EVP_DigestUpdate(&ctx_, buf, len);
}

size_t OpenSSLDigest::Finish(void* buf, size_t len) {
  if (!md_ || len < Size()) {
    return 0;
  }
  unsigned int md_len;
  EVP_DigestFinal_ex(&ctx_, static_cast<unsigned char*>(buf), &md_len);
  EVP_DigestInit_ex(&ctx_, md_, NULL);  // prepare for future Update()s
  ASSERT(md_len == Size());
  return md_len;
}

bool OpenSSLDigest::GetDigestEVP(const std::string& algorithm,
                                 const EVP_MD** mdp) {
  const EVP_MD* md;
  if (algorithm == DIGEST_MD5) {
    md = EVP_md5();
  } else if (algorithm == DIGEST_SHA_1) {
    md = EVP_sha1();
  } else if (algorithm == DIGEST_SHA_224) {
    md = EVP_sha224();
  } else if (algorithm == DIGEST_SHA_256) {
    md = EVP_sha256();
  } else if (algorithm == DIGEST_SHA_384) {
    md = EVP_sha384();
  } else if (algorithm == DIGEST_SHA_512) {
    md = EVP_sha512();
  } else {
    return false;
  }

  // Can't happen
  ASSERT(EVP_MD_size(md) >= 16);
  *mdp = md;
  return true;
}

bool OpenSSLDigest::GetDigestName(const EVP_MD* md,
                                  std::string* algorithm) {
  ASSERT(md != NULL);
  ASSERT(algorithm != NULL);

  int md_type = EVP_MD_type(md);
  if (md_type == NID_md5) {
    *algorithm = DIGEST_MD5;
  } else if (md_type == NID_sha1) {
    *algorithm = DIGEST_SHA_1;
  } else if (md_type == NID_sha224) {
    *algorithm = DIGEST_SHA_224;
  } else if (md_type == NID_sha256) {
    *algorithm = DIGEST_SHA_256;
  } else if (md_type == NID_sha384) {
    *algorithm = DIGEST_SHA_384;
  } else if (md_type == NID_sha512) {
    *algorithm = DIGEST_SHA_512;
  } else {
    algorithm->clear();
    return false;
  }

  return true;
}

bool OpenSSLDigest::GetDigestSize(const std::string& algorithm,
                                  size_t* length) {
  const EVP_MD *md;
  if (!GetDigestEVP(algorithm, &md))
    return false;

  *length = EVP_MD_size(md);
  return true;
}

}  // namespace talk_base

#endif  // HAVE_OPENSSL_SSL_H

