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

#ifndef TALK_P2P_BASE_STUNSERVER_H_
#define TALK_P2P_BASE_STUNSERVER_H_

#include "talk/base/asyncudpsocket.h"
#include "talk/base/scoped_ptr.h"
#include "talk/p2p/base/stun.h"

namespace cricket {

const int STUN_SERVER_PORT = 3478;

class StunServer : public sigslot::has_slots<> {
 public:
  // Creates a STUN server, which will listen on the given socket.
  explicit StunServer(talk_base::AsyncUDPSocket* socket);
  // Removes the STUN server from the socket and deletes the socket.
  ~StunServer();

 protected:
  // Slot for AsyncSocket.PacketRead:
  void OnPacket(
      talk_base::AsyncPacketSocket* socket, const char* buf, size_t size,
      const talk_base::SocketAddress& remote_addr);

  // Handlers for the different types of STUN/TURN requests:
  void OnBindingRequest(StunMessage* msg,
      const talk_base::SocketAddress& addr);
  void OnAllocateRequest(StunMessage* msg,
      const talk_base::SocketAddress& addr);
  void OnSharedSecretRequest(StunMessage* msg,
      const talk_base::SocketAddress& addr);
  void OnSendRequest(StunMessage* msg,
      const talk_base::SocketAddress& addr);

  // Sends an error response to the given message back to the user.
  void SendErrorResponse(
      const StunMessage& msg, const talk_base::SocketAddress& addr,
      int error_code, const char* error_desc);

  // Sends the given message to the appropriate destination.
  void SendResponse(const StunMessage& msg,
       const talk_base::SocketAddress& addr);

 private:
  talk_base::scoped_ptr<talk_base::AsyncUDPSocket> socket_;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_STUNSERVER_H_
