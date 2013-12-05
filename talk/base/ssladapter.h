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

#ifndef TALK_BASE_SSLADAPTER_H_
#define TALK_BASE_SSLADAPTER_H_

#include "talk/base/asyncsocket.h"

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////

class SSLAdapter : public AsyncSocketAdapter {
 public:
  explicit SSLAdapter(AsyncSocket* socket)
    : AsyncSocketAdapter(socket), ignore_bad_cert_(false) { }

  bool ignore_bad_cert() const { return ignore_bad_cert_; }
  void set_ignore_bad_cert(bool ignore) { ignore_bad_cert_ = ignore; }

  // StartSSL returns 0 if successful.
  // If StartSSL is called while the socket is closed or connecting, the SSL
  // negotiation will begin as soon as the socket connects.
  virtual int StartSSL(const char* hostname, bool restartable) = 0;

  // Create the default SSL adapter for this platform. On failure, returns NULL
  // and deletes |socket|. Otherwise, the returned SSLAdapter takes ownership
  // of |socket|.
  static SSLAdapter* Create(AsyncSocket* socket);

 private:
  // If true, the server certificate need not match the configured hostname.
  bool ignore_bad_cert_;
};

///////////////////////////////////////////////////////////////////////////////

typedef bool (*VerificationCallback)(void* cert);

// Call this on the main thread, before using SSL.
// Call CleanupSSLThread when finished with SSL.
bool InitializeSSL(VerificationCallback callback = NULL);

// Call to initialize additional threads.
bool InitializeSSLThread();

// Call to cleanup additional threads, and also the main thread.
bool CleanupSSL();

///////////////////////////////////////////////////////////////////////////////

}  // namespace talk_base

#endif  // TALK_BASE_SSLADAPTER_H_
