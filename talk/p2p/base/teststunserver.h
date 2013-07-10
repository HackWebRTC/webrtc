/*
 * libjingle
 * Copyright 2008 Google Inc.
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

#ifndef TALK_P2P_BASE_TESTSTUNSERVER_H_
#define TALK_P2P_BASE_TESTSTUNSERVER_H_

#include "talk/base/socketaddress.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/stunserver.h"

namespace cricket {

// A test STUN server. Useful for unit tests.
class TestStunServer {
 public:
  TestStunServer(talk_base::Thread* thread,
                 const talk_base::SocketAddress& addr)
      : socket_(thread->socketserver()->CreateAsyncSocket(addr.family(),
                                                          SOCK_DGRAM)),
        udp_socket_(talk_base::AsyncUDPSocket::Create(socket_, addr)),
        server_(udp_socket_) {
  }
 private:
  talk_base::AsyncSocket* socket_;
  talk_base::AsyncUDPSocket* udp_socket_;
  cricket::StunServer server_;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_TESTSTUNSERVER_H_
