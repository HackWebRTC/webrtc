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

#if defined(HAVE_SRTP) && defined(ENABLE_EXTERNAL_AUTH)

#ifdef SRTP_RELATIVE_PATH
#include "srtp.h"  // NOLINT
#else
#include "third_party/libsrtp/include/srtp.h"
#endif  // SRTP_RELATIVE_PATH

#include "talk/session/media/external_hmac.h"

#include "talk/base/logging.h"

// The debug module for authentiation
debug_module_t mod_external_hmac = {
  0,                            // Debugging is off by default
  (char*)"external-hmac-sha-1"  // Printable name for module
};

extern auth_type_t external_hmac;

// Begin test case 0 */
uint8_t
external_hmac_test_case_0_key[20] = {
  0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
  0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
  0x0b, 0x0b, 0x0b, 0x0b
};

uint8_t
external_hmac_test_case_0_data[8] = {
  0x48, 0x69, 0x20, 0x54, 0x68, 0x65, 0x72, 0x65   // "Hi There"
};

uint8_t
external_hmac_fake_tag[10] = {
  0xba, 0xdd, 0xba, 0xdd, 0xba, 0xdd, 0xba, 0xdd, 0xba, 0xdd
};

auth_test_case_t
external_hmac_test_case_0 = {
  20,                                // Octets in key
  external_hmac_test_case_0_key,     // Key
  8,                                 // Octets in data
  external_hmac_test_case_0_data,    // Data
  10,                                // Octets in tag
  external_hmac_fake_tag,            // Tag
  NULL                               // Pointer to next testcase
};

err_status_t
external_hmac_alloc(auth_t** a, int key_len, int out_len) {
  uint8_t* pointer;

  // Check key length - note that we don't support keys larger
  // than 20 bytes yet
  if (key_len > 20)
    return err_status_bad_param;

  // Check output length - should be less than 20 bytes/
  if (out_len > 20)
    return err_status_bad_param;

  // Allocate memory for auth and hmac_ctx_t structures.
  pointer = reinterpret_cast<uint8_t*>(
      crypto_alloc(sizeof(external_hmac_ctx_t) + sizeof(auth_t)));
  if (pointer == NULL)
    return err_status_alloc_fail;

  // Set pointers
  *a = (auth_t *)pointer;
  (*a)->type = &external_hmac;
  (*a)->state = pointer + sizeof(auth_t);
  (*a)->out_len = out_len;
  (*a)->key_len = key_len;
  (*a)->prefix_len = 0;

  // Increment global count of all hmac uses.
  external_hmac.ref_count++;

  return err_status_ok;
}

err_status_t
external_hmac_dealloc(auth_t* a) {
  // Zeroize entire state
  octet_string_set_to_zero((uint8_t *)a,
         sizeof(external_hmac_ctx_t) + sizeof(auth_t));

  // Free memory
  crypto_free(a);

  // Decrement global count of all hmac uses.
  external_hmac.ref_count--;

  return err_status_ok;
}

err_status_t
external_hmac_init(external_hmac_ctx_t* state,
                   const uint8_t* key, int key_len) {
  if (key_len > HMAC_KEY_LENGTH)
    return err_status_bad_param;

  memset(state->key, 0, key_len);
  memcpy(state->key, key, key_len);
  state->key_length = key_len;
  return err_status_ok;
}

err_status_t
external_hmac_start(external_hmac_ctx_t* state) {
  return err_status_ok;
}

err_status_t
external_hmac_update(external_hmac_ctx_t* state, const uint8_t* message,
                     int msg_octets) {
  return err_status_ok;
}

err_status_t
external_hmac_compute(external_hmac_ctx_t* state, const void* message,
                      int msg_octets, int tag_len, uint8_t* result) {
  memcpy(result, external_hmac_fake_tag, tag_len);
  return err_status_ok;
}

char external_hmac_description[] = "external hmac sha-1 authentication";

 // auth_type_t external_hmac is the hmac metaobject

auth_type_t
external_hmac  = {
  (auth_alloc_func)      external_hmac_alloc,
  (auth_dealloc_func)    external_hmac_dealloc,
  (auth_init_func)       external_hmac_init,
  (auth_compute_func)    external_hmac_compute,
  (auth_update_func)     external_hmac_update,
  (auth_start_func)      external_hmac_start,
  (char *)               external_hmac_description,
  (int)                  0,  /* instance count */
  (auth_test_case_t *)   &external_hmac_test_case_0,
  (debug_module_t *)     &mod_external_hmac,
  (auth_type_id_t)       EXTERNAL_HMAC_SHA1
};

err_status_t
external_crypto_init() {
  err_status_t status = crypto_kernel_replace_auth_type(
      &external_hmac, EXTERNAL_HMAC_SHA1);
  if (status) {
    LOG(LS_ERROR) << "Error in replacing default auth module, error: "
                  << status;
    return err_status_fail;
  }
  return err_status_ok;
}

#endif  // defined(HAVE_SRTP) && defined(ENABLE_EXTERNAL_AUTH)
