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

#ifndef TALK_XMPP_XMPPSOCKET_H_
#define TALK_XMPP_XMPPSOCKET_H_

#include "webrtc/base/asyncsocket.h"
#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/sigslot.h"
#include "talk/xmpp/asyncsocket.h"
#include "talk/xmpp/xmppengine.h"

// The below define selects the SSLStreamAdapter implementation for
// SSL, as opposed to the SSLAdapter socket adapter.
// #define USE_SSLSTREAM 

namespace rtc {
  class StreamInterface;
  class SocketAddress;
};
extern rtc::AsyncSocket* cricket_socket_;

namespace buzz {

class XmppSocket : public buzz::AsyncSocket, public sigslot::has_slots<> {
public:
  XmppSocket(buzz::TlsOptions tls);
  ~XmppSocket();

  virtual buzz::AsyncSocket::State state();
  virtual buzz::AsyncSocket::Error error();
  virtual int GetError();

  virtual bool Connect(const rtc::SocketAddress& addr);
  virtual bool Read(char * data, size_t len, size_t* len_read);
  virtual bool Write(const char * data, size_t len);
  virtual bool Close();
  virtual bool StartTls(const std::string & domainname);

  sigslot::signal1<int> SignalCloseEvent;

private:
  void CreateCricketSocket(int family);
#ifndef USE_SSLSTREAM
  void OnReadEvent(rtc::AsyncSocket * socket);
  void OnWriteEvent(rtc::AsyncSocket * socket);
  void OnConnectEvent(rtc::AsyncSocket * socket);
  void OnCloseEvent(rtc::AsyncSocket * socket, int error);
#else  // USE_SSLSTREAM
  void OnEvent(rtc::StreamInterface* stream, int events, int err);
#endif  // USE_SSLSTREAM

  rtc::AsyncSocket * cricket_socket_;
#ifdef USE_SSLSTREAM
  rtc::StreamInterface *stream_;
#endif  // USE_SSLSTREAM
  buzz::AsyncSocket::State state_;
  rtc::ByteBuffer buffer_;
  buzz::TlsOptions tls_;
};

}  // namespace buzz

#endif // TALK_XMPP_XMPPSOCKET_H_

