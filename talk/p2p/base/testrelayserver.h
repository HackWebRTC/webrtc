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

#ifndef TALK_P2P_BASE_TESTRELAYSERVER_H_
#define TALK_P2P_BASE_TESTRELAYSERVER_H_

#include "talk/base/asynctcpsocket.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/socketadapters.h"
#include "talk/base/sigslot.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/relayserver.h"

namespace cricket {

// A test relay server. Useful for unit tests.
class TestRelayServer : public sigslot::has_slots<> {
 public:
  TestRelayServer(talk_base::Thread* thread,
                  const talk_base::SocketAddress& udp_int_addr,
                  const talk_base::SocketAddress& udp_ext_addr,
                  const talk_base::SocketAddress& tcp_int_addr,
                  const talk_base::SocketAddress& tcp_ext_addr,
                  const talk_base::SocketAddress& ssl_int_addr,
                  const talk_base::SocketAddress& ssl_ext_addr)
      : server_(thread) {
    server_.AddInternalSocket(talk_base::AsyncUDPSocket::Create(
        thread->socketserver(), udp_int_addr));
    server_.AddExternalSocket(talk_base::AsyncUDPSocket::Create(
        thread->socketserver(), udp_ext_addr));

    tcp_int_socket_.reset(CreateListenSocket(thread, tcp_int_addr));
    tcp_ext_socket_.reset(CreateListenSocket(thread, tcp_ext_addr));
    ssl_int_socket_.reset(CreateListenSocket(thread, ssl_int_addr));
    ssl_ext_socket_.reset(CreateListenSocket(thread, ssl_ext_addr));
  }
  int GetConnectionCount() const {
    return server_.GetConnectionCount();
  }
  talk_base::SocketAddressPair GetConnection(int connection) const {
    return server_.GetConnection(connection);
  }
  bool HasConnection(const talk_base::SocketAddress& address) const {
    return server_.HasConnection(address);
  }

 private:
  talk_base::AsyncSocket* CreateListenSocket(talk_base::Thread* thread,
      const talk_base::SocketAddress& addr) {
    talk_base::AsyncSocket* socket =
        thread->socketserver()->CreateAsyncSocket(addr.family(), SOCK_STREAM);
    socket->Bind(addr);
    socket->Listen(5);
    socket->SignalReadEvent.connect(this, &TestRelayServer::OnAccept);
    return socket;
  }
  void OnAccept(talk_base::AsyncSocket* socket) {
    bool external = (socket == tcp_ext_socket_.get() ||
                     socket == ssl_ext_socket_.get());
    bool ssl = (socket == ssl_int_socket_.get() ||
                socket == ssl_ext_socket_.get());
    talk_base::AsyncSocket* raw_socket = socket->Accept(NULL);
    if (raw_socket) {
      talk_base::AsyncTCPSocket* packet_socket = new talk_base::AsyncTCPSocket(
          (!ssl) ? raw_socket :
          new talk_base::AsyncSSLServerSocket(raw_socket), false);
      if (!external) {
        packet_socket->SignalClose.connect(this,
            &TestRelayServer::OnInternalClose);
        server_.AddInternalSocket(packet_socket);
      } else {
        packet_socket->SignalClose.connect(this,
            &TestRelayServer::OnExternalClose);
        server_.AddExternalSocket(packet_socket);
      }
    }
  }
  void OnInternalClose(talk_base::AsyncPacketSocket* socket, int error) {
    server_.RemoveInternalSocket(socket);
  }
  void OnExternalClose(talk_base::AsyncPacketSocket* socket, int error) {
    server_.RemoveExternalSocket(socket);
  }
 private:
  cricket::RelayServer server_;
  talk_base::scoped_ptr<talk_base::AsyncSocket> tcp_int_socket_;
  talk_base::scoped_ptr<talk_base::AsyncSocket> tcp_ext_socket_;
  talk_base::scoped_ptr<talk_base::AsyncSocket> ssl_int_socket_;
  talk_base::scoped_ptr<talk_base::AsyncSocket> ssl_ext_socket_;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_TESTRELAYSERVER_H_
