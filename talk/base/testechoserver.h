/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#ifndef TALK_BASE_TESTECHOSERVER_H_
#define TALK_BASE_TESTECHOSERVER_H_

#include <list>
#include "talk/base/asynctcpsocket.h"
#include "talk/base/socketaddress.h"
#include "talk/base/sigslot.h"
#include "talk/base/thread.h"

namespace talk_base {

// A test echo server, echoes back any packets sent to it.
// Useful for unit tests.
class TestEchoServer : public sigslot::has_slots<> {
 public:
  TestEchoServer(Thread* thread, const SocketAddress& addr)
      : server_socket_(thread->socketserver()->CreateAsyncSocket(addr.family(),
                                                                 SOCK_STREAM)) {
    server_socket_->Bind(addr);
    server_socket_->Listen(5);
    server_socket_->SignalReadEvent.connect(this, &TestEchoServer::OnAccept);
  }
  ~TestEchoServer() {
    for (ClientList::iterator it = client_sockets_.begin();
         it != client_sockets_.end(); ++it) {
      delete *it;
    }
  }

  SocketAddress address() const { return server_socket_->GetLocalAddress(); }

 private:
  void OnAccept(AsyncSocket* socket) {
    AsyncSocket* raw_socket = socket->Accept(NULL);
    if (raw_socket) {
      AsyncTCPSocket* packet_socket = new AsyncTCPSocket(raw_socket, false);
      packet_socket->SignalReadPacket.connect(this, &TestEchoServer::OnPacket);
      packet_socket->SignalClose.connect(this, &TestEchoServer::OnClose);
      client_sockets_.push_back(packet_socket);
    }
  }
  void OnPacket(AsyncPacketSocket* socket, const char* buf, size_t size,
                const SocketAddress& remote_addr,
                const PacketTime& packet_time) {
    socket->Send(buf, size, DSCP_NO_CHANGE);
  }
  void OnClose(AsyncPacketSocket* socket, int err) {
    ClientList::iterator it =
        std::find(client_sockets_.begin(), client_sockets_.end(), socket);
    client_sockets_.erase(it);
    Thread::Current()->Dispose(socket);
  }

  typedef std::list<AsyncTCPSocket*> ClientList;
  scoped_ptr<AsyncSocket> server_socket_;
  ClientList client_sockets_;
  DISALLOW_EVIL_CONSTRUCTORS(TestEchoServer);
};

}  // namespace talk_base

#endif  // TALK_BASE_TESTECHOSERVER_H_
