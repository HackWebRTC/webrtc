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

#include "xmppsocket.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include "talk/base/basicdefs.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"
#ifdef FEATURE_ENABLE_SSL
#include "talk/base/ssladapter.h"
#endif

#ifdef USE_SSLSTREAM
#include "talk/base/socketstream.h"
#ifdef FEATURE_ENABLE_SSL
#include "talk/base/sslstreamadapter.h"
#endif  // FEATURE_ENABLE_SSL
#endif  // USE_SSLSTREAM

namespace buzz {

XmppSocket::XmppSocket(buzz::TlsOptions tls) : cricket_socket_(NULL),
                                               tls_(tls) {
  state_ = buzz::AsyncSocket::STATE_CLOSED;
}

void XmppSocket::CreateCricketSocket(int family) {
  talk_base::Thread* pth = talk_base::Thread::Current();
  if (family == AF_UNSPEC) {
    family = AF_INET;
  }
  talk_base::AsyncSocket* socket =
      pth->socketserver()->CreateAsyncSocket(family, SOCK_STREAM);
#ifndef USE_SSLSTREAM
#ifdef FEATURE_ENABLE_SSL
  if (tls_ != buzz::TLS_DISABLED) {
    socket = talk_base::SSLAdapter::Create(socket);
  }
#endif  // FEATURE_ENABLE_SSL
  cricket_socket_ = socket;
  cricket_socket_->SignalReadEvent.connect(this, &XmppSocket::OnReadEvent);
  cricket_socket_->SignalWriteEvent.connect(this, &XmppSocket::OnWriteEvent);
  cricket_socket_->SignalConnectEvent.connect(this,
                                              &XmppSocket::OnConnectEvent);
  cricket_socket_->SignalCloseEvent.connect(this, &XmppSocket::OnCloseEvent);
#else  // USE_SSLSTREAM
  cricket_socket_ = socket;
  stream_ = new talk_base::SocketStream(cricket_socket_);
#ifdef FEATURE_ENABLE_SSL
  if (tls_ != buzz::TLS_DISABLED)
    stream_ = talk_base::SSLStreamAdapter::Create(stream_);
#endif  // FEATURE_ENABLE_SSL
  stream_->SignalEvent.connect(this, &XmppSocket::OnEvent);
#endif  // USE_SSLSTREAM
}

XmppSocket::~XmppSocket() {
  Close();
#ifndef USE_SSLSTREAM
  delete cricket_socket_;
#else  // USE_SSLSTREAM
  delete stream_;
#endif  // USE_SSLSTREAM
}

#ifndef USE_SSLSTREAM
void XmppSocket::OnReadEvent(talk_base::AsyncSocket * socket) {
  SignalRead();
}

void XmppSocket::OnWriteEvent(talk_base::AsyncSocket * socket) {
  // Write bytes if there are any
  while (buffer_.Length() != 0) {
    int written = cricket_socket_->Send(buffer_.Data(), buffer_.Length());
    if (written > 0) {
      buffer_.Consume(written);
      continue;
    }
    if (!cricket_socket_->IsBlocking())
      LOG(LS_ERROR) << "Send error: " << cricket_socket_->GetError();
    return;
  }
}

void XmppSocket::OnConnectEvent(talk_base::AsyncSocket * socket) {
#if defined(FEATURE_ENABLE_SSL)
  if (state_ == buzz::AsyncSocket::STATE_TLS_CONNECTING) {
    state_ = buzz::AsyncSocket::STATE_TLS_OPEN;
    SignalSSLConnected();
    OnWriteEvent(cricket_socket_);
    return;
  }
#endif  // !defined(FEATURE_ENABLE_SSL)
  state_ = buzz::AsyncSocket::STATE_OPEN;
  SignalConnected();
}

void XmppSocket::OnCloseEvent(talk_base::AsyncSocket * socket, int error) {
  SignalCloseEvent(error);
}

#else  // USE_SSLSTREAM

void XmppSocket::OnEvent(talk_base::StreamInterface* stream,
                         int events, int err) {
  if ((events & talk_base::SE_OPEN)) {
#if defined(FEATURE_ENABLE_SSL)
    if (state_ == buzz::AsyncSocket::STATE_TLS_CONNECTING) {
      state_ = buzz::AsyncSocket::STATE_TLS_OPEN;
      SignalSSLConnected();
      events |= talk_base::SE_WRITE;
    } else
#endif
    {
      state_ = buzz::AsyncSocket::STATE_OPEN;
      SignalConnected();
    }
  }
  if ((events & talk_base::SE_READ))
    SignalRead();
  if ((events & talk_base::SE_WRITE)) {
    // Write bytes if there are any
    while (buffer_.Length() != 0) {
      talk_base::StreamResult result;
      size_t written;
      int error;
      result = stream_->Write(buffer_.Data(), buffer_.Length(),
                              &written, &error);
      if (result == talk_base::SR_ERROR) {
        LOG(LS_ERROR) << "Send error: " << error;
        return;
      }
      if (result == talk_base::SR_BLOCK)
        return;
      ASSERT(result == talk_base::SR_SUCCESS);
      ASSERT(written > 0);
      buffer_.Shift(written);
    }
  }
  if ((events & talk_base::SE_CLOSE))
    SignalCloseEvent(err);
}
#endif  // USE_SSLSTREAM

buzz::AsyncSocket::State XmppSocket::state() {
  return state_;
}

buzz::AsyncSocket::Error XmppSocket::error() {
  return buzz::AsyncSocket::ERROR_NONE;
}

int XmppSocket::GetError() {
  return 0;
}

bool XmppSocket::Connect(const talk_base::SocketAddress& addr) {
  if (cricket_socket_ == NULL) {
    CreateCricketSocket(addr.family());
  }
  if (cricket_socket_->Connect(addr) < 0) {
    return cricket_socket_->IsBlocking();
  }
  return true;
}

bool XmppSocket::Read(char * data, size_t len, size_t* len_read) {
#ifndef USE_SSLSTREAM
  int read = cricket_socket_->Recv(data, len);
  if (read > 0) {
    *len_read = (size_t)read;
    return true;
  }
#else  // USE_SSLSTREAM
  talk_base::StreamResult result = stream_->Read(data, len, len_read, NULL);
  if (result == talk_base::SR_SUCCESS)
    return true;
#endif  // USE_SSLSTREAM
  return false;
}

bool XmppSocket::Write(const char * data, size_t len) {
  buffer_.WriteBytes(data, len);
#ifndef USE_SSLSTREAM
  OnWriteEvent(cricket_socket_);
#else  // USE_SSLSTREAM
  OnEvent(stream_, talk_base::SE_WRITE, 0);
#endif  // USE_SSLSTREAM
  return true;
}

bool XmppSocket::Close() {
  if (state_ != buzz::AsyncSocket::STATE_OPEN)
    return false;
#ifndef USE_SSLSTREAM
  if (cricket_socket_->Close() == 0) {
    state_ = buzz::AsyncSocket::STATE_CLOSED;
    SignalClosed();
    return true;
  }
  return false;
#else  // USE_SSLSTREAM
  state_ = buzz::AsyncSocket::STATE_CLOSED;
  stream_->Close();
  SignalClosed();
  return true;
#endif  // USE_SSLSTREAM
}

bool XmppSocket::StartTls(const std::string & domainname) {
#if defined(FEATURE_ENABLE_SSL)
  if (tls_ == buzz::TLS_DISABLED)
    return false;
#ifndef USE_SSLSTREAM
  talk_base::SSLAdapter* ssl_adapter =
    static_cast<talk_base::SSLAdapter *>(cricket_socket_);
  if (ssl_adapter->StartSSL(domainname.c_str(), false) != 0)
    return false;
#else  // USE_SSLSTREAM
  talk_base::SSLStreamAdapter* ssl_stream =
    static_cast<talk_base::SSLStreamAdapter *>(stream_);
  if (ssl_stream->StartSSLWithServer(domainname.c_str()) != 0)
    return false;
#endif  // USE_SSLSTREAM
  state_ = buzz::AsyncSocket::STATE_TLS_CONNECTING;
  return true;
#else  // !defined(FEATURE_ENABLE_SSL)
  return false;
#endif  // !defined(FEATURE_ENABLE_SSL)
}

}  // namespace buzz

