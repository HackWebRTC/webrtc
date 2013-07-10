/*
 * libjingle
 * Copyright 2004--2010, Google Inc.
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

#ifndef TALK_BASE_ASYNCTCPSOCKET_H_
#define TALK_BASE_ASYNCTCPSOCKET_H_

#include "talk/base/asyncpacketsocket.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/socketfactory.h"

namespace talk_base {

// Simulates UDP semantics over TCP.  Send and Recv packet sizes
// are preserved, and drops packets silently on Send, rather than
// buffer them in user space.
class AsyncTCPSocketBase : public AsyncPacketSocket {
 public:
  AsyncTCPSocketBase(AsyncSocket* socket, bool listen, size_t max_packet_size);
  virtual ~AsyncTCPSocketBase();

  // Pure virtual methods to send and recv data.
  virtual int Send(const void *pv, size_t cb) = 0;
  virtual void ProcessInput(char* data, size_t* len) = 0;
  // Signals incoming connection.
  virtual void HandleIncomingConnection(AsyncSocket* socket) = 0;

  virtual SocketAddress GetLocalAddress() const;
  virtual SocketAddress GetRemoteAddress() const;
  virtual int SendTo(const void *pv, size_t cb, const SocketAddress& addr);
  virtual int Close();

  virtual State GetState() const;
  virtual int GetOption(Socket::Option opt, int* value);
  virtual int SetOption(Socket::Option opt, int value);
  virtual int GetError() const;
  virtual void SetError(int error);

 protected:
  // Binds and connects |socket| and creates AsyncTCPSocket for
  // it. Takes ownership of |socket|. Returns NULL if bind() or
  // connect() fail (|socket| is destroyed in that case).
  static AsyncSocket* ConnectSocket(AsyncSocket* socket,
                                    const SocketAddress& bind_address,
                                    const SocketAddress& remote_address);
  virtual int SendRaw(const void* pv, size_t cb);
  int FlushOutBuffer();
  // Add data to |outbuf_|.
  void AppendToOutBuffer(const void* pv, size_t cb);

  // Helper methods for |outpos_|.
  bool IsOutBufferEmpty() const { return outpos_ == 0; }
  void ClearOutBuffer() { outpos_ = 0; }

 private:
  // Called by the underlying socket
  void OnConnectEvent(AsyncSocket* socket);
  void OnReadEvent(AsyncSocket* socket);
  void OnWriteEvent(AsyncSocket* socket);
  void OnCloseEvent(AsyncSocket* socket, int error);

  scoped_ptr<AsyncSocket> socket_;
  bool listen_;
  char* inbuf_, * outbuf_;
  size_t insize_, inpos_, outsize_, outpos_;

  DISALLOW_EVIL_CONSTRUCTORS(AsyncTCPSocketBase);
};

class AsyncTCPSocket : public AsyncTCPSocketBase {
 public:
  // Binds and connects |socket| and creates AsyncTCPSocket for
  // it. Takes ownership of |socket|. Returns NULL if bind() or
  // connect() fail (|socket| is destroyed in that case).
  static AsyncTCPSocket* Create(AsyncSocket* socket,
                                const SocketAddress& bind_address,
                                const SocketAddress& remote_address);
  AsyncTCPSocket(AsyncSocket* socket, bool listen);
  virtual ~AsyncTCPSocket() {}

  virtual int Send(const void* pv, size_t cb);
  virtual void ProcessInput(char* data, size_t* len);
  virtual void HandleIncomingConnection(AsyncSocket* socket);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(AsyncTCPSocket);
};

}  // namespace talk_base

#endif  // TALK_BASE_ASYNCTCPSOCKET_H_
