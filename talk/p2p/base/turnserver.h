/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#ifndef TALK_P2P_BASE_TURNSERVER_H_
#define TALK_P2P_BASE_TURNSERVER_H_

#include <list>
#include <map>
#include <set>
#include <string>

#include "talk/base/messagequeue.h"
#include "talk/base/sigslot.h"
#include "talk/base/socketaddress.h"
#include "talk/p2p/base/portinterface.h"

namespace talk_base {
class AsyncPacketSocket;
class ByteBuffer;
class PacketSocketFactory;
class Thread;
}

namespace cricket {

class StunMessage;
class TurnMessage;

// The default server port for TURN, as specified in RFC5766.
const int TURN_SERVER_PORT = 3478;

// An interface through which the MD5 credential hash can be retrieved.
class TurnAuthInterface {
 public:
  // Gets HA1 for the specified user and realm.
  // HA1 = MD5(A1) = MD5(username:realm:password).
  // Return true if the given username and realm are valid, or false if not.
  virtual bool GetKey(const std::string& username, const std::string& realm,
                      std::string* key) = 0;
};

// The core TURN server class. Give it a socket to listen on via
// AddInternalServerSocket, and a factory to create external sockets via
// SetExternalSocketFactory, and it's ready to go.
// Not yet wired up: TCP support.
class TurnServer : public sigslot::has_slots<> {
 public:
  explicit TurnServer(talk_base::Thread* thread);
  ~TurnServer();

  // Gets/sets the realm value to use for the server.
  const std::string& realm() const { return realm_; }
  void set_realm(const std::string& realm) { realm_ = realm; }

  // Gets/sets the value for the SOFTWARE attribute for TURN messages.
  const std::string& software() const { return software_; }
  void set_software(const std::string& software) { software_ = software; }

  // Sets the authentication callback; does not take ownership.
  void set_auth_hook(TurnAuthInterface* auth_hook) { auth_hook_ = auth_hook; }

  void set_enable_otu_nonce(bool enable) { enable_otu_nonce_ = enable; }

  // Starts listening for packets from internal clients.
  void AddInternalSocket(talk_base::AsyncPacketSocket* socket,
                         ProtocolType proto);
  // Starts listening for the connections on this socket. When someone tries
  // to connect, the connection will be accepted and a new internal socket
  // will be added.
  void AddInternalServerSocket(talk_base::AsyncSocket* socket,
                               ProtocolType proto);
  // Specifies the factory to use for creating external sockets.
  void SetExternalSocketFactory(talk_base::PacketSocketFactory* factory,
                                const talk_base::SocketAddress& address);

 private:
  // Encapsulates the client's connection to the server.
  class Connection {
   public:
    Connection() : proto_(PROTO_UDP), socket_(NULL) {}
    Connection(const talk_base::SocketAddress& src,
               ProtocolType proto,
               talk_base::AsyncPacketSocket* socket);
    const talk_base::SocketAddress& src() const { return src_; }
    talk_base::AsyncPacketSocket* socket() { return socket_; }
    bool operator==(const Connection& t) const;
    bool operator<(const Connection& t) const;
    std::string ToString() const;

   private:
    talk_base::SocketAddress src_;
    talk_base::SocketAddress dst_;
    cricket::ProtocolType proto_;
    talk_base::AsyncPacketSocket* socket_;
  };
  class Allocation;
  class Permission;
  class Channel;
  typedef std::map<Connection, Allocation*> AllocationMap;

  void OnInternalPacket(talk_base::AsyncPacketSocket* socket, const char* data,
                        size_t size, const talk_base::SocketAddress& address);

  void OnNewInternalConnection(talk_base::AsyncSocket* socket);

  // Accept connections on this server socket.
  void AcceptConnection(talk_base::AsyncSocket* server_socket);
  void OnInternalSocketClose(talk_base::AsyncPacketSocket* socket, int err);

  void HandleStunMessage(Connection* conn, const char* data, size_t size);
  void HandleBindingRequest(Connection* conn, const StunMessage* msg);
  void HandleAllocateRequest(Connection* conn, const TurnMessage* msg,
                             const std::string& key);

  bool GetKey(const StunMessage* msg, std::string* key);
  bool CheckAuthorization(Connection* conn, const StunMessage* msg,
                          const char* data, size_t size,
                          const std::string& key);
  std::string GenerateNonce() const;
  bool ValidateNonce(const std::string& nonce) const;

  Allocation* FindAllocation(Connection* conn);
  Allocation* CreateAllocation(Connection* conn, int proto,
                               const std::string& key);

  void SendErrorResponse(Connection* conn, const StunMessage* req,
                         int code, const std::string& reason);

  void SendErrorResponseWithRealmAndNonce(Connection* conn,
                                          const StunMessage* req,
                                          int code,
                                          const std::string& reason);
  void SendStun(Connection* conn, StunMessage* msg);
  void Send(Connection* conn, const talk_base::ByteBuffer& buf);

  void OnAllocationDestroyed(Allocation* allocation);
  void DestroyInternalSocket(talk_base::AsyncPacketSocket* socket);

  typedef std::map<talk_base::AsyncPacketSocket*,
                   ProtocolType> InternalSocketMap;
  typedef std::map<talk_base::AsyncSocket*,
                   ProtocolType> ServerSocketMap;

  talk_base::Thread* thread_;
  std::string nonce_key_;
  std::string realm_;
  std::string software_;
  TurnAuthInterface* auth_hook_;
  // otu - one-time-use. Server will respond with 438 if it's
  // sees the same nonce in next transaction.
  bool enable_otu_nonce_;
  InternalSocketMap server_sockets_;
  ServerSocketMap server_listen_sockets_;
  talk_base::scoped_ptr<talk_base::PacketSocketFactory>
      external_socket_factory_;
  talk_base::SocketAddress external_addr_;
  AllocationMap allocations_;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_TURNSERVER_H_
