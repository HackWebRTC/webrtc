/*
 * libjingle
 * Copyright 2013, Google Inc.
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

#ifndef TALK_BASE_ASYNCSTUNTCPSOCKET_H_
#define TALK_BASE_ASYNCSTUNTCPSOCKET_H_

#include "webrtc/base/asynctcpsocket.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/socketfactory.h"

namespace cricket {

class AsyncStunTCPSocket : public rtc::AsyncTCPSocketBase {
 public:
  // Binds and connects |socket| and creates AsyncTCPSocket for
  // it. Takes ownership of |socket|. Returns NULL if bind() or
  // connect() fail (|socket| is destroyed in that case).
  static AsyncStunTCPSocket* Create(
      rtc::AsyncSocket* socket,
      const rtc::SocketAddress& bind_address,
      const rtc::SocketAddress& remote_address);

  AsyncStunTCPSocket(rtc::AsyncSocket* socket, bool listen);
  virtual ~AsyncStunTCPSocket() {}

  virtual int Send(const void* pv, size_t cb,
                   const rtc::PacketOptions& options);
  virtual void ProcessInput(char* data, size_t* len);
  virtual void HandleIncomingConnection(rtc::AsyncSocket* socket);

 private:
  // This method returns the message hdr + length written in the header.
  // This method also returns the number of padding bytes needed/added to the
  // turn message. |pad_bytes| should be used only when |is_turn| is true.
  size_t GetExpectedLength(const void* data, size_t len,
                           int* pad_bytes);

  DISALLOW_EVIL_CONSTRUCTORS(AsyncStunTCPSocket);
};

}  // namespace cricket

#endif  // TALK_BASE_ASYNCSTUNTCPSOCKET_H_
