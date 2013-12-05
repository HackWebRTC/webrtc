/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#if HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#include "talk/base/ssladapter.h"

#include "talk/base/sslconfig.h"

#if SSL_USE_SCHANNEL

#include "schanneladapter.h"

#elif SSL_USE_OPENSSL  // && !SSL_USE_SCHANNEL

#include "openssladapter.h"

#elif SSL_USE_NSS     // && !SSL_USE_CHANNEL && !SSL_USE_OPENSSL

#include "nssstreamadapter.h"

#endif  // SSL_USE_OPENSSL && !SSL_USE_SCHANNEL && !SSL_USE_NSS

///////////////////////////////////////////////////////////////////////////////

namespace talk_base {

SSLAdapter*
SSLAdapter::Create(AsyncSocket* socket) {
#if SSL_USE_SCHANNEL
  return new SChannelAdapter(socket);
#elif SSL_USE_OPENSSL  // && !SSL_USE_SCHANNEL
  return new OpenSSLAdapter(socket);
#else  // !SSL_USE_OPENSSL && !SSL_USE_SCHANNEL
  delete socket;
  return NULL;
#endif  // !SSL_USE_OPENSSL && !SSL_USE_SCHANNEL
}

///////////////////////////////////////////////////////////////////////////////

#if SSL_USE_OPENSSL

bool InitializeSSL(VerificationCallback callback) {
  return OpenSSLAdapter::InitializeSSL(callback);
}

bool InitializeSSLThread() {
  return OpenSSLAdapter::InitializeSSLThread();
}

bool CleanupSSL() {
  return OpenSSLAdapter::CleanupSSL();
}

#elif SSL_USE_NSS  // !SSL_USE_OPENSSL

bool InitializeSSL(VerificationCallback callback) {
  return NSSContext::InitializeSSL(callback);
}

bool InitializeSSLThread() {
  return NSSContext::InitializeSSLThread();
}

bool CleanupSSL() {
  return NSSContext::CleanupSSL();
}

#else  // !SSL_USE_OPENSSL && !SSL_USE_NSS

bool InitializeSSL(VerificationCallback callback) {
  return true;
}

bool InitializeSSLThread() {
  return true;
}

bool CleanupSSL() {
  return true;
}

#endif  // !SSL_USE_OPENSSL && !SSL_USE_NSS

///////////////////////////////////////////////////////////////////////////////

}  // namespace talk_base
