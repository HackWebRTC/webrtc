/*
 * libjingle
 * Copyright 2004--2008, Google Inc.
 * Copyright 2012, RTFM, Inc.
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


#include <vector>

#if HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#include "talk/base/sslstreamadapterhelper.h"

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/stream.h"

namespace talk_base {

void SSLStreamAdapterHelper::SetIdentity(SSLIdentity* identity) {
  ASSERT(identity_.get() == NULL);
  identity_.reset(identity);
}

void SSLStreamAdapterHelper::SetServerRole(SSLRole role) {
  role_ = role;
}

int SSLStreamAdapterHelper::StartSSLWithServer(const char* server_name) {
  ASSERT(server_name != NULL && server_name[0] != '\0');
  ssl_server_name_ = server_name;
  return StartSSL();
}

int SSLStreamAdapterHelper::StartSSLWithPeer() {
  ASSERT(ssl_server_name_.empty());
  // It is permitted to specify peer_certificate_ only later.
  return StartSSL();
}

void SSLStreamAdapterHelper::SetMode(SSLMode mode) {
  ASSERT(state_ == SSL_NONE);
  ssl_mode_ = mode;
}

StreamState SSLStreamAdapterHelper::GetState() const {
  switch (state_) {
    case SSL_WAIT:
    case SSL_CONNECTING:
      return SS_OPENING;
    case SSL_CONNECTED:
      return SS_OPEN;
    default:
      return SS_CLOSED;
  };
  // not reached
}

void SSLStreamAdapterHelper::SetPeerCertificate(SSLCertificate* cert) {
  ASSERT(peer_certificate_.get() == NULL);
  ASSERT(peer_certificate_digest_algorithm_.empty());
  ASSERT(ssl_server_name_.empty());
  peer_certificate_.reset(cert);
}

bool SSLStreamAdapterHelper::GetPeerCertificate(SSLCertificate** cert) const {
  if (!peer_certificate_)
    return false;

  *cert = peer_certificate_->GetReference();
  return true;
}

bool SSLStreamAdapterHelper::SetPeerCertificateDigest(
    const std::string &digest_alg,
    const unsigned char* digest_val,
    size_t digest_len) {
  ASSERT(peer_certificate_.get() == NULL);
  ASSERT(peer_certificate_digest_algorithm_.empty());
  ASSERT(ssl_server_name_.empty());
  size_t expected_len;

  if (!GetDigestLength(digest_alg, &expected_len)) {
    LOG(LS_WARNING) << "Unknown digest algorithm: " << digest_alg;
    return false;
  }
  if (expected_len != digest_len)
    return false;

  peer_certificate_digest_value_.SetData(digest_val, digest_len);
  peer_certificate_digest_algorithm_ = digest_alg;

  return true;
}

void SSLStreamAdapterHelper::Error(const char* context, int err, bool signal) {
  LOG(LS_WARNING) << "SSLStreamAdapterHelper::Error("
                  << context << ", " << err << "," << signal << ")";
  state_ = SSL_ERROR;
  ssl_error_code_ = err;
  Cleanup();
  if (signal)
    StreamAdapterInterface::OnEvent(stream(), SE_CLOSE, err);
}

void SSLStreamAdapterHelper::Close() {
  Cleanup();
  ASSERT(state_ == SSL_CLOSED || state_ == SSL_ERROR);
  StreamAdapterInterface::Close();
}

int SSLStreamAdapterHelper::StartSSL() {
  ASSERT(state_ == SSL_NONE);

  if (StreamAdapterInterface::GetState() != SS_OPEN) {
    state_ = SSL_WAIT;
    return 0;
  }

  state_ = SSL_CONNECTING;
  int err = BeginSSL();
  if (err) {
    Error("BeginSSL", err, false);
    return err;
  }

  return 0;
}

}  // namespace talk_base

