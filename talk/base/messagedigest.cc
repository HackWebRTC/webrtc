/*
 * libjingle
 * Copyright 2011, Google Inc.
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
 *     derived from this software without specific prior written permission
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

#include "talk/base/messagedigest.h"

#include <string.h>

#include "talk/base/sslconfig.h"
#if SSL_USE_OPENSSL
#include "talk/base/openssldigest.h"
#else
#include "talk/base/md5digest.h"
#include "talk/base/sha1digest.h"
#endif
#include "talk/base/scoped_ptr.h"
#include "talk/base/stringencode.h"

namespace talk_base {

// From RFC 4572.
const char DIGEST_MD5[]     = "md5";
const char DIGEST_SHA_1[]   = "sha-1";
const char DIGEST_SHA_224[] = "sha-224";
const char DIGEST_SHA_256[] = "sha-256";
const char DIGEST_SHA_384[] = "sha-384";
const char DIGEST_SHA_512[] = "sha-512";

static const size_t kBlockSize = 64;  // valid for SHA-256 and down

MessageDigest* MessageDigestFactory::Create(const std::string& alg) {
#if SSL_USE_OPENSSL
  MessageDigest* digest = new OpenSSLDigest(alg);
  if (digest->Size() == 0) {  // invalid algorithm
    delete digest;
    digest = NULL;
  }
  return digest;
#else
  MessageDigest* digest = NULL;
  if (alg == DIGEST_MD5) {
    digest = new Md5Digest();
  } else if (alg == DIGEST_SHA_1) {
    digest = new Sha1Digest();
  }
  return digest;
#endif
}

bool IsFips180DigestAlgorithm(const std::string& alg) {
  // These are the FIPS 180 algorithms.  According to RFC 4572 Section 5,
  // "Self-signed certificates (for which legacy certificates are not a
  // consideration) MUST use one of the FIPS 180 algorithms (SHA-1,
  // SHA-224, SHA-256, SHA-384, or SHA-512) as their signature algorithm,
  // and thus also MUST use it to calculate certificate fingerprints."
  return alg == DIGEST_SHA_1 ||
         alg == DIGEST_SHA_224 ||
         alg == DIGEST_SHA_256 ||
         alg == DIGEST_SHA_384 ||
         alg == DIGEST_SHA_512;
}

size_t ComputeDigest(MessageDigest* digest, const void* input, size_t in_len,
                     void* output, size_t out_len) {
  digest->Update(input, in_len);
  return digest->Finish(output, out_len);
}

size_t ComputeDigest(const std::string& alg, const void* input, size_t in_len,
                     void* output, size_t out_len) {
  scoped_ptr<MessageDigest> digest(MessageDigestFactory::Create(alg));
  return (digest) ?
      ComputeDigest(digest.get(), input, in_len, output, out_len) :
      0;
}

std::string ComputeDigest(MessageDigest* digest, const std::string& input) {
  scoped_ptr<char[]> output(new char[digest->Size()]);
  ComputeDigest(digest, input.data(), input.size(),
                output.get(), digest->Size());
  return hex_encode(output.get(), digest->Size());
}

bool ComputeDigest(const std::string& alg, const std::string& input,
                   std::string* output) {
  scoped_ptr<MessageDigest> digest(MessageDigestFactory::Create(alg));
  if (!digest) {
    return false;
  }
  *output = ComputeDigest(digest.get(), input);
  return true;
}

std::string ComputeDigest(const std::string& alg, const std::string& input) {
  std::string output;
  ComputeDigest(alg, input, &output);
  return output;
}

// Compute a RFC 2104 HMAC: H(K XOR opad, H(K XOR ipad, text))
size_t ComputeHmac(MessageDigest* digest,
                   const void* key, size_t key_len,
                   const void* input, size_t in_len,
                   void* output, size_t out_len) {
  // We only handle algorithms with a 64-byte blocksize.
  // TODO: Add BlockSize() method to MessageDigest.
  size_t block_len = kBlockSize;
  if (digest->Size() > 32) {
    return 0;
  }
  // Copy the key to a block-sized buffer to simplify padding.
  // If the key is longer than a block, hash it and use the result instead.
  scoped_ptr<uint8[]> new_key(new uint8[block_len]);
  if (key_len > block_len) {
    ComputeDigest(digest, key, key_len, new_key.get(), block_len);
    memset(new_key.get() + digest->Size(), 0, block_len - digest->Size());
  } else {
    memcpy(new_key.get(), key, key_len);
    memset(new_key.get() + key_len, 0, block_len - key_len);
  }
  // Set up the padding from the key, salting appropriately for each padding.
  scoped_ptr<uint8[]> o_pad(new uint8[block_len]), i_pad(new uint8[block_len]);
  for (size_t i = 0; i < block_len; ++i) {
    o_pad[i] = 0x5c ^ new_key[i];
    i_pad[i] = 0x36 ^ new_key[i];
  }
  // Inner hash; hash the inner padding, and then the input buffer.
  scoped_ptr<uint8[]> inner(new uint8[digest->Size()]);
  digest->Update(i_pad.get(), block_len);
  digest->Update(input, in_len);
  digest->Finish(inner.get(), digest->Size());
  // Outer hash; hash the outer padding, and then the result of the inner hash.
  digest->Update(o_pad.get(), block_len);
  digest->Update(inner.get(), digest->Size());
  return digest->Finish(output, out_len);
}

size_t ComputeHmac(const std::string& alg, const void* key, size_t key_len,
                   const void* input, size_t in_len,
                   void* output, size_t out_len) {
  scoped_ptr<MessageDigest> digest(MessageDigestFactory::Create(alg));
  if (!digest) {
    return 0;
  }
  return ComputeHmac(digest.get(), key, key_len,
                     input, in_len, output, out_len);
}

std::string ComputeHmac(MessageDigest* digest, const std::string& key,
                        const std::string& input) {
  scoped_ptr<char[]> output(new char[digest->Size()]);
  ComputeHmac(digest, key.data(), key.size(),
              input.data(), input.size(), output.get(), digest->Size());
  return hex_encode(output.get(), digest->Size());
}

bool ComputeHmac(const std::string& alg, const std::string& key,
                 const std::string& input, std::string* output) {
  scoped_ptr<MessageDigest> digest(MessageDigestFactory::Create(alg));
  if (!digest) {
    return false;
  }
  *output = ComputeHmac(digest.get(), key, input);
  return true;
}

std::string ComputeHmac(const std::string& alg, const std::string& key,
                        const std::string& input) {
  std::string output;
  ComputeHmac(alg, key, input, &output);
  return output;
}

}  // namespace talk_base
