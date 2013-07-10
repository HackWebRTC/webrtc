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

#ifndef TALK_BASE_TESTCLIENT_H_
#define TALK_BASE_TESTCLIENT_H_

#include <vector>
#include "talk/base/asyncudpsocket.h"
#include "talk/base/criticalsection.h"

namespace talk_base {

// A simple client that can send TCP or UDP data and check that it receives
// what it expects to receive. Useful for testing server functionality.
class TestClient : public sigslot::has_slots<> {
 public:
  // Records the contents of a packet that was received.
  struct Packet {
    Packet(const SocketAddress& a, const char* b, size_t s);
    Packet(const Packet& p);
    virtual ~Packet();

    SocketAddress addr;
    char*  buf;
    size_t size;
  };

  // Creates a client that will send and receive with the given socket and
  // will post itself messages with the given thread.
  explicit TestClient(AsyncPacketSocket* socket);
  ~TestClient();

  SocketAddress address() const { return socket_->GetLocalAddress(); }
  SocketAddress remote_address() const { return socket_->GetRemoteAddress(); }

  // Checks that the socket moves to the specified connect state.
  bool CheckConnState(AsyncPacketSocket::State state);

  // Checks that the socket is connected to the remote side.
  bool CheckConnected() {
    return CheckConnState(AsyncPacketSocket::STATE_CONNECTED);
  }

  // Sends using the clients socket.
  int Send(const char* buf, size_t size);

  // Sends using the clients socket to the given destination.
  int SendTo(const char* buf, size_t size, const SocketAddress& dest);

  // Returns the next packet received by the client or 0 if none is received
  // within a reasonable amount of time.  The caller must delete the packet
  // when done with it.
  Packet* NextPacket();

  // Checks that the next packet has the given contents. Returns the remote
  // address that the packet was sent from.
  bool CheckNextPacket(const char* buf, size_t len, SocketAddress* addr);

  // Checks that no packets have arrived or will arrive in the next second.
  bool CheckNoPacket();

  int GetError();
  int SetOption(Socket::Option opt, int value);

  bool ready_to_send() const;

 private:
  static const int kTimeout = 1000;
  // Workaround for the fact that AsyncPacketSocket::GetConnState doesn't exist.
  Socket::ConnState GetState();
  // Slot for packets read on the socket.
  void OnPacket(AsyncPacketSocket* socket, const char* buf, size_t len,
                const SocketAddress& remote_addr);
  void OnReadyToSend(AsyncPacketSocket* socket);

  CriticalSection crit_;
  AsyncPacketSocket* socket_;
  std::vector<Packet*>* packets_;
  bool ready_to_send_;
  DISALLOW_EVIL_CONSTRUCTORS(TestClient);
};

}  // namespace talk_base

#endif  // TALK_BASE_TESTCLIENT_H_
