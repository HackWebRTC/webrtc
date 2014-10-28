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

#ifndef WEBRTC_P2P_BASE_TESTSTUNSERVER_H_
#define WEBRTC_P2P_BASE_TESTSTUNSERVER_H_

#include "webrtc/p2p/base/stunserver.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/thread.h"

namespace cricket {

// A test STUN server. Useful for unit tests.
class TestStunServer : StunServer {
 public:
  static TestStunServer* Create(rtc::Thread* thread,
                                const rtc::SocketAddress& addr) {
    rtc::AsyncSocket* socket =
        thread->socketserver()->CreateAsyncSocket(addr.family(), SOCK_DGRAM);
    rtc::AsyncUDPSocket* udp_socket =
        rtc::AsyncUDPSocket::Create(socket, addr);

    return new TestStunServer(udp_socket);
  }

  // Set a fake STUN address to return to the client.
  void set_fake_stun_addr(const rtc::SocketAddress& addr) {
    fake_stun_addr_ = addr;
  }

 private:
  explicit TestStunServer(rtc::AsyncUDPSocket* socket) : StunServer(socket) {}

  void OnBindingRequest(StunMessage* msg,
                        const rtc::SocketAddress& remote_addr) OVERRIDE {
    if (fake_stun_addr_.IsNil()) {
      StunServer::OnBindingRequest(msg, remote_addr);
    } else {
      StunMessage response;
      GetStunBindReqponse(msg, fake_stun_addr_, &response);
      SendResponse(response, remote_addr);
    }
  }

 private:
  rtc::SocketAddress fake_stun_addr_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TESTSTUNSERVER_H_
