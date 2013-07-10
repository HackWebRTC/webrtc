/*
 * libjingle
 * Copyright 2012, Google Inc.
 * Copyright 2012, RTFM Inc.
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

#ifndef TALK_BASE_SSLFINGERPRINT_H_
#define TALK_BASE_SSLFINGERPRINT_H_

#include <ctype.h>
#include <string>

#include "talk/base/buffer.h"
#include "talk/base/helpers.h"
#include "talk/base/messagedigest.h"
#include "talk/base/sslidentity.h"
#include "talk/base/stringencode.h"

namespace talk_base {

struct SSLFingerprint {
  static SSLFingerprint* Create(const std::string& algorithm,
                                const talk_base::SSLIdentity* identity) {
    if (!identity) {
      return NULL;
    }

    uint8 digest_val[64];
    size_t digest_len;
    bool ret = identity->certificate().ComputeDigest(
        algorithm, digest_val, sizeof(digest_val), &digest_len);
    if (!ret) {
      return NULL;
    }

    return new SSLFingerprint(algorithm, digest_val, digest_len);
  }

  static SSLFingerprint* CreateFromRfc4572(const std::string& algorithm,
                                           const std::string& fingerprint) {
    if (algorithm.empty())
      return NULL;

    if (fingerprint.empty())
      return NULL;

    size_t value_len;
    char value[talk_base::MessageDigest::kMaxSize];
    value_len = talk_base::hex_decode_with_delimiter(value, sizeof(value),
                                                     fingerprint.c_str(),
                                                     fingerprint.length(),
                                                     ':');
    if (!value_len)
      return NULL;

    return new SSLFingerprint(algorithm,
                              reinterpret_cast<uint8*>(value),
                              value_len);
  }

  SSLFingerprint(const std::string& algorithm, const uint8* digest_in,
                 size_t digest_len) : algorithm(algorithm) {
    digest.SetData(digest_in, digest_len);
  }
  SSLFingerprint(const SSLFingerprint& from)
      : algorithm(from.algorithm), digest(from.digest) {}
  bool operator==(const SSLFingerprint& other) const {
    return algorithm == other.algorithm &&
           digest == other.digest;
  }

  std::string GetRfc4572Fingerprint() const {
    std::string fingerprint =
        talk_base::hex_encode_with_delimiter(
            digest.data(), digest.length(), ':');
    std::transform(fingerprint.begin(), fingerprint.end(),
                   fingerprint.begin(), ::toupper);
    return fingerprint;
  }

  std::string algorithm;
  talk_base::Buffer digest;
};

}  // namespace talk_base

#endif  // TALK_BASE_SSLFINGERPRINT_H_
