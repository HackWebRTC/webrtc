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

#ifndef TALK_BASE_SCHANNELADAPTER_H__
#define TALK_BASE_SCHANNELADAPTER_H__

#include <string>
#include "talk/base/ssladapter.h"
#include "talk/base/messagequeue.h"
struct _SecBufferDesc;

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////

class SChannelAdapter : public SSLAdapter, public MessageHandler {
public:
  SChannelAdapter(AsyncSocket* socket);
  virtual ~SChannelAdapter();

  virtual int StartSSL(const char* hostname, bool restartable);
  virtual int Send(const void* pv, size_t cb);
  virtual int Recv(void* pv, size_t cb);
  virtual int Close();

  // Note that the socket returns ST_CONNECTING while SSL is being negotiated.
  virtual ConnState GetState() const;

protected:
  enum SSLState {
    SSL_NONE, SSL_WAIT, SSL_CONNECTING, SSL_CONNECTED, SSL_ERROR
  };
  struct SSLImpl;

  virtual void OnConnectEvent(AsyncSocket* socket);
  virtual void OnReadEvent(AsyncSocket* socket);
  virtual void OnWriteEvent(AsyncSocket* socket);
  virtual void OnCloseEvent(AsyncSocket* socket, int err);
  virtual void OnMessage(Message* pmsg);

  int BeginSSL();
  int ContinueSSL();
  int ProcessContext(long int status, _SecBufferDesc* sbd_in,
                     _SecBufferDesc* sbd_out);
  int DecryptData();

  int Read();
  int Flush();
  void Error(const char* context, int err, bool signal = true);
  void Cleanup();

  void PostEvent();

private:
  SSLState state_;
  std::string ssl_host_name_;
  // If true, socket will retain SSL configuration after Close.
  bool restartable_; 
  // If true, we are delaying signalling close until all data is read.
  bool signal_close_;
  // If true, we are waiting to be woken up to signal readability or closure.
  bool message_pending_;
  SSLImpl* impl_;
};

/////////////////////////////////////////////////////////////////////////////

} // namespace talk_base

#endif // TALK_BASE_SCHANNELADAPTER_H__
