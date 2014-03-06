/*
 * libjingle
 * Copyright 2014 Google Inc.
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

#ifndef TALK_SESSION_MEDIA_EXTERNAL_HMAC_H_
#define TALK_SESSION_MEDIA_EXTERNAL_HMAC_H_

// External libsrtp HMAC auth module which implements methods defined in
// auth_type_t.
// The default auth module will be replaced only when the ENABLE_EXTERNAL_AUTH
// flag is enabled. This allows us to access to authentication keys,
// as the default auth implementation doesn't provide access and avoids
// hashing each packet twice.

// How will libsrtp select this module?
// Libsrtp defines authentication function types identified by an unsigned
// integer, e.g. HMAC_SHA1 is 3. Using authentication ids, the application
// can plug any desired authentication modules into libsrtp.
// libsrtp also provides a mechanism to select different auth functions for
// individual streams. This can be done by setting the right value in
// the auth_type of srtp_policy_t. The application must first register auth
// functions and the corresponding authentication id using
// crypto_kernel_replace_auth_type function.
#if defined(HAVE_SRTP) && defined(ENABLE_EXTERNAL_AUTH)

#include "talk/base/basictypes.h"
#ifdef SRTP_RELATIVE_PATH
#include "auth.h"  // NOLINT
#else
#include "third_party/libsrtp/crypto/include/auth.h"
#endif  // SRTP_RELATIVE_PATH

#define EXTERNAL_HMAC_SHA1 HMAC_SHA1 + 1
#define HMAC_KEY_LENGTH 20

// The HMAC context structure used to store authentication keys.
// The pointer to the key  will be allocated in the external_hmac_init function.
// This pointer is owned by srtp_t in a template context.
typedef struct {
  uint8 key[HMAC_KEY_LENGTH];
  int key_length;
} external_hmac_ctx_t;

err_status_t
external_hmac_alloc(auth_t** a, int key_len, int out_len);

err_status_t
external_hmac_dealloc(auth_t* a);

err_status_t
external_hmac_init(external_hmac_ctx_t* state,
                   const uint8_t* key, int key_len);

err_status_t
external_hmac_start(external_hmac_ctx_t* state);

err_status_t
external_hmac_update(external_hmac_ctx_t* state, const uint8_t* message,
                     int msg_octets);

err_status_t
external_hmac_compute(external_hmac_ctx_t* state, const void* message,
                      int msg_octets, int tag_len, uint8_t* result);

err_status_t
external_crypto_init();

#endif  // defined(HAVE_SRTP) && defined(ENABLE_EXTERNAL_AUTH)
#endif  // TALK_SESSION_MEDIA_EXTERNAL_HMAC_H_
