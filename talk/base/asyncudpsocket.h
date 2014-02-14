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

#ifndef TALK_BASE_ASYNCUDPSOCKET_H_
#define TALK_BASE_ASYNCUDPSOCKET_H_

#include "talk/base/asyncpacketsocket.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/socketfactory.h"

namespace talk_base {

// Provides the ability to receive packets asynchronously.  Sends are not
// buffered since it is acceptable to drop packets under high load.
class AsyncUDPSocket : public AsyncPacketSocket {
 public:
  // Binds |socket| and creates AsyncUDPSocket for it. Takes ownership
  // of |socket|. Returns NULL if bind() fails (|socket| is destroyed
  // in that case).
  static AsyncUDPSocket* Create(AsyncSocket* socket,
                                const SocketAddress& bind_address);
  // Creates a new socket for sending asynchronous UDP packets using an
  // asynchronous socket from the given factory.
  static AsyncUDPSocket* Create(SocketFactory* factory,
                                const SocketAddress& bind_address);
  explicit AsyncUDPSocket(AsyncSocket* socket);
  virtual ~AsyncUDPSocket();

  virtual SocketAddress GetLocalAddress() const;
  virtual SocketAddress GetRemoteAddress() const;
  virtual int Send(const void *pv, size_t cb,
                   const talk_base::PacketOptions& options);
  virtual int SendTo(const void *pv, size_t cb, const SocketAddress& addr,
                     const talk_base::PacketOptions& options);
  virtual int Close();

  virtual State GetState() const;
  virtual int GetOption(Socket::Option opt, int* value);
  virtual int SetOption(Socket::Option opt, int value);
  virtual int GetError() const;
  virtual void SetError(int error);

 private:
  // Called when the underlying socket is ready to be read from.
  void OnReadEvent(AsyncSocket* socket);
  // Called when the underlying socket is ready to send.
  void OnWriteEvent(AsyncSocket* socket);

  scoped_ptr<AsyncSocket> socket_;
  char* buf_;
  size_t size_;
};

}  // namespace talk_base

#endif  // TALK_BASE_ASYNCUDPSOCKET_H_
