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

#include "talk/base/testclient.h"
#include "talk/base/thread.h"
#include "talk/base/timeutils.h"

namespace talk_base {

// DESIGN: Each packet received is put it into a list of packets.
//         Callers can retrieve received packets from any thread by calling
//         NextPacket.

TestClient::TestClient(AsyncPacketSocket* socket)
    : socket_(socket), ready_to_send_(false) {
  packets_ = new std::vector<Packet*>();
  socket_->SignalReadPacket.connect(this, &TestClient::OnPacket);
  socket_->SignalReadyToSend.connect(this, &TestClient::OnReadyToSend);
}

TestClient::~TestClient() {
  delete socket_;
  for (unsigned i = 0; i < packets_->size(); i++)
    delete (*packets_)[i];
  delete packets_;
}

bool TestClient::CheckConnState(AsyncPacketSocket::State state) {
  // Wait for our timeout value until the socket reaches the desired state.
  uint32 end = TimeAfter(kTimeout);
  while (socket_->GetState() != state && TimeUntil(end) > 0)
    Thread::Current()->ProcessMessages(1);
  return (socket_->GetState() == state);
}

int TestClient::Send(const char* buf, size_t size) {
  return socket_->Send(buf, size);
}

int TestClient::SendTo(const char* buf, size_t size,
                       const SocketAddress& dest) {
  return socket_->SendTo(buf, size, dest);
}

TestClient::Packet* TestClient::NextPacket() {
  // If no packets are currently available, we go into a get/dispatch loop for
  // at most 1 second.  If, during the loop, a packet arrives, then we can stop
  // early and return it.

  // Note that the case where no packet arrives is important.  We often want to
  // test that a packet does not arrive.

  // Note also that we only try to pump our current thread's message queue.
  // Pumping another thread's queue could lead to messages being dispatched from
  // the wrong thread to non-thread-safe objects.

  uint32 end = TimeAfter(kTimeout);
  while (packets_->size() == 0 && TimeUntil(end) > 0)
    Thread::Current()->ProcessMessages(1);

  // Return the first packet placed in the queue.
  Packet* packet = NULL;
  if (packets_->size() > 0) {
    CritScope cs(&crit_);
    packet = packets_->front();
    packets_->erase(packets_->begin());
  }

  return packet;
}

bool TestClient::CheckNextPacket(const char* buf, size_t size,
                                 SocketAddress* addr) {
  bool res = false;
  Packet* packet = NextPacket();
  if (packet) {
    res = (packet->size == size && std::memcmp(packet->buf, buf, size) == 0);
    if (addr)
      *addr = packet->addr;
    delete packet;
  }
  return res;
}

bool TestClient::CheckNoPacket() {
  bool res;
  Packet* packet = NextPacket();
  res = (packet == NULL);
  delete packet;
  return res;
}

int TestClient::GetError() {
  return socket_->GetError();
}

int TestClient::SetOption(Socket::Option opt, int value) {
  return socket_->SetOption(opt, value);
}

bool TestClient::ready_to_send() const {
  return ready_to_send_;
}

void TestClient::OnPacket(AsyncPacketSocket* socket, const char* buf,
                          size_t size, const SocketAddress& remote_addr) {
  CritScope cs(&crit_);
  packets_->push_back(new Packet(remote_addr, buf, size));
}

void TestClient::OnReadyToSend(AsyncPacketSocket* socket) {
  ready_to_send_ = true;
}

TestClient::Packet::Packet(const SocketAddress& a, const char* b, size_t s)
    : addr(a), buf(0), size(s) {
  buf = new char[size];
  memcpy(buf, b, size);
}

TestClient::Packet::Packet(const Packet& p)
    : addr(p.addr), buf(0), size(p.size) {
  buf = new char[size];
  memcpy(buf, p.buf, size);
}

TestClient::Packet::~Packet() {
  delete[] buf;
}

}  // namespace talk_base
