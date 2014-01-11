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

#ifndef TALK_BASE_MESSAGEDIGEST_H_
#define TALK_BASE_MESSAGEDIGEST_H_

#include <string>

namespace talk_base {

// Definitions for the digest algorithms.
extern const char DIGEST_MD5[];
extern const char DIGEST_SHA_1[];
extern const char DIGEST_SHA_224[];
extern const char DIGEST_SHA_256[];
extern const char DIGEST_SHA_384[];
extern const char DIGEST_SHA_512[];

// A general class for computing hashes.
class MessageDigest {
 public:
  enum { kMaxSize = 64 };  // Maximum known size (SHA-512)
  virtual ~MessageDigest() {}
  // Returns the digest output size (e.g. 16 bytes for MD5).
  virtual size_t Size() const = 0;
  // Updates the digest with |len| bytes from |buf|.
  virtual void Update(const void* buf, size_t len) = 0;
  // Outputs the digest value to |buf| with length |len|.
  // Returns the number of bytes written, i.e., Size().
  virtual size_t Finish(void* buf, size_t len) = 0;
};

// A factory class for creating digest objects.
class MessageDigestFactory {
 public:
  static MessageDigest* Create(const std::string& alg);
};

// A whitelist of approved digest algorithms from RFC 4572 (FIPS 180).
bool IsFips180DigestAlgorithm(const std::string& alg);

// Functions to create hashes.

// Computes the hash of |in_len| bytes of |input|, using the |digest| hash
// implementation, and outputs the hash to the buffer |output|, which is
// |out_len| bytes long. Returns the number of bytes written to |output| if
// successful, or 0 if |out_len| was too small.
size_t ComputeDigest(MessageDigest* digest, const void* input, size_t in_len,
                     void* output, size_t out_len);
// Like the previous function, but creates a digest implementation based on
// the desired digest name |alg|, e.g. DIGEST_SHA_1. Returns 0 if there is no
// digest with the given name.
size_t ComputeDigest(const std::string& alg, const void* input, size_t in_len,
                     void* output, size_t out_len);
// Computes the hash of |input| using the |digest| hash implementation, and
// returns it as a hex-encoded string.
std::string ComputeDigest(MessageDigest* digest, const std::string& input);
// Like the previous function, but creates a digest implementation based on
// the desired digest name |alg|, e.g. DIGEST_SHA_1. Returns empty string if
// there is no digest with the given name.
std::string ComputeDigest(const std::string& alg, const std::string& input);
// Like the previous function, but returns an explicit result code.
bool ComputeDigest(const std::string& alg, const std::string& input,
                   std::string* output);

// Shorthand way to compute a hex-encoded hash using MD5.
inline std::string MD5(const std::string& input) {
  return ComputeDigest(DIGEST_MD5, input);
}

// Functions to compute RFC 2104 HMACs.

// Computes the HMAC of |in_len| bytes of |input|, using the |digest| hash
// implementation and |key_len| bytes of |key| to key the HMAC, and outputs
// the HMAC to the buffer |output|, which is |out_len| bytes long. Returns the
// number of bytes written to |output| if successful, or 0 if |out_len| was too
// small.
size_t ComputeHmac(MessageDigest* digest, const void* key, size_t key_len,
                   const void* input, size_t in_len,
                   void* output, size_t out_len);
// Like the previous function, but creates a digest implementation based on
// the desired digest name |alg|, e.g. DIGEST_SHA_1. Returns 0 if there is no
// digest with the given name.
size_t ComputeHmac(const std::string& alg, const void* key, size_t key_len,
                   const void* input, size_t in_len,
                   void* output, size_t out_len);
// Computes the HMAC of |input| using the |digest| hash implementation and |key|
// to key the HMAC, and returns it as a hex-encoded string.
std::string ComputeHmac(MessageDigest* digest, const std::string& key,
                        const std::string& input);
// Like the previous function, but creates a digest implementation based on
// the desired digest name |alg|, e.g. DIGEST_SHA_1. Returns empty string if
// there is no digest with the given name.
std::string ComputeHmac(const std::string& alg, const std::string& key,
                        const std::string& input);
// Like the previous function, but returns an explicit result code.
bool ComputeHmac(const std::string& alg, const std::string& key,
                 const std::string& input, std::string* output);

}  // namespace talk_base

#endif  // TALK_BASE_MESSAGEDIGEST_H_
