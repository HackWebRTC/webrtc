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

// Handling of certificates and keypairs for SSLStreamAdapter's peer mode.
#if HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#include "talk/base/sslidentity.h"

#include <string>

#include "talk/base/sslconfig.h"

#if SSL_USE_SCHANNEL

#elif SSL_USE_OPENSSL  // !SSL_USE_SCHANNEL

#include "talk/base/opensslidentity.h"

#elif SSL_USE_NSS  // !SSL_USE_SCHANNEL && !SSL_USE_OPENSSL

#include "talk/base/nssidentity.h"

#endif  // SSL_USE_SCHANNEL

namespace talk_base {

#if SSL_USE_SCHANNEL

SSLCertificate* SSLCertificate::FromPEMString(const std::string& pem_string) {
  return NULL;
}

SSLIdentity* SSLIdentity::Generate(const std::string& common_name) {
  return NULL;
}

SSLIdentity* SSLIdentity::FromPEMStrings(const std::string& private_key,
                                         const std::string& certificate) {
  return NULL;
}

#elif SSL_USE_OPENSSL  // !SSL_USE_SCHANNEL

SSLCertificate* SSLCertificate::FromPEMString(const std::string& pem_string) {
  return OpenSSLCertificate::FromPEMString(pem_string);
}

SSLIdentity* SSLIdentity::Generate(const std::string& common_name) {
  return OpenSSLIdentity::Generate(common_name);
}

SSLIdentity* SSLIdentity::FromPEMStrings(const std::string& private_key,
                                         const std::string& certificate) {
  return OpenSSLIdentity::FromPEMStrings(private_key, certificate);
}

#elif SSL_USE_NSS  // !SSL_USE_OPENSSL && !SSL_USE_SCHANNEL

SSLCertificate* SSLCertificate::FromPEMString(const std::string& pem_string) {
  return NSSCertificate::FromPEMString(pem_string);
}

SSLIdentity* SSLIdentity::Generate(const std::string& common_name) {
  return NSSIdentity::Generate(common_name);
}

SSLIdentity* SSLIdentity::FromPEMStrings(const std::string& private_key,
                                         const std::string& certificate) {
  return NSSIdentity::FromPEMStrings(private_key, certificate);
}

#else  // !SSL_USE_OPENSSL && !SSL_USE_SCHANNEL && !SSL_USE_NSS

#error "No SSL implementation"

#endif  // SSL_USE_SCHANNEL

}  // namespace talk_base
