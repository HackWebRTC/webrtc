/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/openssl_key_derivation_hkdf.h"

#include <algorithm>
#include <utility>

#include <openssl/ossl_typ.h>
#ifdef OPENSSL_IS_BORINGSSL
#include <openssl/digest.h>
#include <openssl/hkdf.h>
#else
#include <openssl/evp.h>
#include <openssl/kdf.h>
#endif
#include <openssl/err.h>
#include <openssl/sha.h>

#include "rtc_base/buffer.h"
#include "rtc_base/openssl.h"

namespace rtc {

#ifndef OPENSSL_IS_BORINGSSL
namespace {

// HKDF is static within OpenSSL and hence not accessible to the caller.
// This internal implementation allows for compatibility with BoringSSL.
static int HKDF(uint8_t* out_key,
                size_t out_len,
                const EVP_MD* digest,
                const uint8_t* secret,
                size_t secret_len,
                const uint8_t* salt,
                size_t salt_len,
                const uint8_t* info,
                size_t info_len) {
  EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);

  if (EVP_PKEY_derive_init(pctx) <= 0 ||
      EVP_PKEY_CTX_set_hkdf_md(pctx, digest) <= 0 ||
      EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt, salt_len) <= 0 ||
      EVP_PKEY_CTX_set1_hkdf_key(pctx, secret, secret_len) <= 0 ||
      EVP_PKEY_CTX_add1_hkdf_info(pctx, info, info_len) <= 0 ||
      EVP_PKEY_derive(pctx, out_key, &out_len) <= 0) {
    EVP_PKEY_CTX_free(pctx);
    return 0;
  }
  EVP_PKEY_CTX_free(pctx);
  return 1;
}

}  // namespace
#endif

OpenSSLKeyDerivationHKDF::OpenSSLKeyDerivationHKDF() = default;
OpenSSLKeyDerivationHKDF::~OpenSSLKeyDerivationHKDF() = default;

const size_t OpenSSLKeyDerivationHKDF::kMinKeyByteSize = 16;
const size_t OpenSSLKeyDerivationHKDF::kMaxKeyByteSize =
    255 * SHA256_DIGEST_LENGTH;
const size_t OpenSSLKeyDerivationHKDF::kMinSecretByteSize = 16;

absl::optional<ZeroOnFreeBuffer<uint8_t>> OpenSSLKeyDerivationHKDF::DeriveKey(
    rtc::ArrayView<const uint8_t> secret,
    rtc::ArrayView<const uint8_t> salt,
    rtc::ArrayView<const uint8_t> label,
    size_t derived_key_byte_size) {
  // Prevent deriving less than 128 bits of key material or more than the max.
  if (derived_key_byte_size < kMinKeyByteSize ||
      derived_key_byte_size > kMaxKeyByteSize) {
    return absl::nullopt;
  }
  // The secret must reach the minimum number of bits to be secure.
  if (secret.data() == nullptr || secret.size() < kMinSecretByteSize) {
    return absl::nullopt;
  }
  // Empty labels are always invalid in derivation.
  if (label.data() == nullptr || label.size() == 0) {
    return absl::nullopt;
  }
  // If a random salt is not provided use all zeros.
  rtc::Buffer salt_buffer;
  if (salt.data() == nullptr || salt.size() == 0) {
    salt_buffer.SetSize(SHA256_DIGEST_LENGTH);
    std::fill(salt_buffer.begin(), salt_buffer.end(), 0);
    salt = salt_buffer;
  }
  // This buffer will erase itself on release.
  ZeroOnFreeBuffer<uint8_t> derived_key_buffer(derived_key_byte_size, 0);
  if (!HKDF(derived_key_buffer.data(), derived_key_buffer.size(), EVP_sha256(),
            secret.data(), secret.size(), salt.data(), salt.size(),
            label.data(), label.size())) {
    return absl::nullopt;
  }
  return absl::optional<ZeroOnFreeBuffer<uint8_t>>(
      std::move(derived_key_buffer));
}

}  // namespace rtc
