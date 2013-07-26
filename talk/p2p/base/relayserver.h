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

#ifndef TALK_P2P_BASE_RELAYSERVER_H_
#define TALK_P2P_BASE_RELAYSERVER_H_

#include <string>
#include <vector>
#include <map>

#include "talk/base/asyncudpsocket.h"
#include "talk/base/socketaddresspair.h"
#include "talk/base/thread.h"
#include "talk/base/timeutils.h"
#include "talk/p2p/base/port.h"
#include "talk/p2p/base/stun.h"

namespace cricket {

class RelayServerBinding;
class RelayServerConnection;

// Relays traffic between connections to the server that are "bound" together.
// All connections created with the same username/password are bound together.
class RelayServer : public talk_base::MessageHandler,
                    public sigslot::has_slots<> {
 public:
  // Creates a server, which will use this thread to post messages to itself.
  explicit RelayServer(talk_base::Thread* thread);
  ~RelayServer();

  talk_base::Thread* thread() { return thread_; }

  // Indicates whether we will print updates of the number of bindings.
  bool log_bindings() const { return log_bindings_; }
  void set_log_bindings(bool log_bindings) { log_bindings_ = log_bindings; }

  // Updates the set of sockets that the server uses to talk to "internal"
  // clients.  These are clients that do the "port allocations".
  void AddInternalSocket(talk_base::AsyncPacketSocket* socket);
  void RemoveInternalSocket(talk_base::AsyncPacketSocket* socket);

  // Updates the set of sockets that the server uses to talk to "external"
  // clients.  These are the clients that do not do allocations.  They do not
  // know that these addresses represent a relay server.
  void AddExternalSocket(talk_base::AsyncPacketSocket* socket);
  void RemoveExternalSocket(talk_base::AsyncPacketSocket* socket);

  // Starts listening for connections on this sockets. When someone
  // tries to connect, the connection will be accepted and a new
  // internal socket will be added.
  void AddInternalServerSocket(talk_base::AsyncSocket* socket,
                               cricket::ProtocolType proto);

  // Removes this server socket from the list.
  void RemoveInternalServerSocket(talk_base::AsyncSocket* socket);

  // Methods for testing and debuging.
  int GetConnectionCount() const;
  talk_base::SocketAddressPair GetConnection(int connection) const;
  bool HasConnection(const talk_base::SocketAddress& address) const;

 private:
  typedef std::vector<talk_base::AsyncPacketSocket*> SocketList;
  typedef std::map<talk_base::AsyncSocket*,
                   cricket::ProtocolType> ServerSocketMap;
  typedef std::map<std::string, RelayServerBinding*> BindingMap;
  typedef std::map<talk_base::SocketAddressPair,
                   RelayServerConnection*> ConnectionMap;

  talk_base::Thread* thread_;
  bool log_bindings_;
  SocketList internal_sockets_;
  SocketList external_sockets_;
  SocketList removed_sockets_;
  ServerSocketMap server_sockets_;
  BindingMap bindings_;
  ConnectionMap connections_;

  // Called when a packet is received by the server on one of its sockets.
  void OnInternalPacket(talk_base::AsyncPacketSocket* socket,
                        const char* bytes, size_t size,
                        const talk_base::SocketAddress& remote_addr);
  void OnExternalPacket(talk_base::AsyncPacketSocket* socket,
                        const char* bytes, size_t size,
                        const talk_base::SocketAddress& remote_addr);

  void OnReadEvent(talk_base::AsyncSocket* socket);

  // Processes the relevant STUN request types from the client.
  bool HandleStun(const char* bytes, size_t size,
                  const talk_base::SocketAddress& remote_addr,
                  talk_base::AsyncPacketSocket* socket,
                  std::string* username, StunMessage* msg);
  void HandleStunAllocate(const char* bytes, size_t size,
                          const talk_base::SocketAddressPair& ap,
                          talk_base::AsyncPacketSocket* socket);
  void HandleStun(RelayServerConnection* int_conn, const char* bytes,
                  size_t size);
  void HandleStunAllocate(RelayServerConnection* int_conn,
                          const StunMessage& msg);
  void HandleStunSend(RelayServerConnection* int_conn, const StunMessage& msg);

  // Adds/Removes the a connection or binding.
  void AddConnection(RelayServerConnection* conn);
  void RemoveConnection(RelayServerConnection* conn);
  void RemoveBinding(RelayServerBinding* binding);

  // Handle messages in our worker thread.
  void OnMessage(talk_base::Message *pmsg);

  // Called when the timer for checking lifetime times out.
  void OnTimeout(RelayServerBinding* binding);

  // Accept connections on this server socket.
  void AcceptConnection(talk_base::AsyncSocket* server_socket);

  friend class RelayServerConnection;
  friend class RelayServerBinding;
};

// Maintains information about a connection to the server.  Each connection is
// part of one and only one binding.
class RelayServerConnection {
 public:
  RelayServerConnection(RelayServerBinding* binding,
                        const talk_base::SocketAddressPair& addrs,
                        talk_base::AsyncPacketSocket* socket);
  ~RelayServerConnection();

  RelayServerBinding* binding() { return binding_; }
  talk_base::AsyncPacketSocket* socket() { return socket_; }

  // Returns a pair where the source is the remote address and the destination
  // is the local address.
  const talk_base::SocketAddressPair& addr_pair() { return addr_pair_; }

  // Sends a packet to the connected client.  If an address is provided, then
  // we make sure the internal client receives it, wrapping if necessary.
  void Send(const char* data, size_t size);
  void Send(const char* data, size_t size,
            const talk_base::SocketAddress& ext_addr);

  // Sends a STUN message to the connected client with no wrapping.
  void SendStun(const StunMessage& msg);
  void SendStunError(const StunMessage& request, int code, const char* desc);

  // A locked connection is one for which we know the intended destination of
  // any raw packet received.
  bool locked() const { return locked_; }
  void Lock();
  void Unlock();

  // Records the address that raw packets should be forwarded to (for internal
  // packets only; for external, we already know where they go).
  const talk_base::SocketAddress& default_destination() const {
    return default_dest_;
  }
  void set_default_destination(const talk_base::SocketAddress& addr) {
    default_dest_ = addr;
  }

 private:
  RelayServerBinding* binding_;
  talk_base::SocketAddressPair addr_pair_;
  talk_base::AsyncPacketSocket* socket_;
  bool locked_;
  talk_base::SocketAddress default_dest_;
};

// Records a set of internal and external connections that we relay between,
// or in other words, that are "bound" together.
class RelayServerBinding : public talk_base::MessageHandler {
 public:
  RelayServerBinding(
      RelayServer* server, const std::string& username,
      const std::string& password, uint32 lifetime);
  virtual ~RelayServerBinding();

  RelayServer* server() { return server_; }
  uint32 lifetime() { return lifetime_; }
  const std::string& username() { return username_; }
  const std::string& password() { return password_; }
  const std::string& magic_cookie() { return magic_cookie_; }

  // Adds/Removes a connection into the binding.
  void AddInternalConnection(RelayServerConnection* conn);
  void AddExternalConnection(RelayServerConnection* conn);

  // We keep track of the use of each binding.  If we detect that it was not
  // used for longer than the lifetime, then we send a signal.
  void NoteUsed();
  sigslot::signal1<RelayServerBinding*> SignalTimeout;

  // Determines whether the given packet has the magic cookie present (in the
  // right place).
  bool HasMagicCookie(const char* bytes, size_t size) const;

  // Determines the connection to use to send packets to or from the given
  // external address.
  RelayServerConnection* GetInternalConnection(
      const talk_base::SocketAddress& ext_addr);
  RelayServerConnection* GetExternalConnection(
      const talk_base::SocketAddress& ext_addr);

  // MessageHandler:
  void OnMessage(talk_base::Message *pmsg);

 private:
  RelayServer* server_;

  std::string username_;
  std::string password_;
  std::string magic_cookie_;

  std::vector<RelayServerConnection*> internal_connections_;
  std::vector<RelayServerConnection*> external_connections_;

  uint32 lifetime_;
  uint32 last_used_;
  // TODO: bandwidth
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_RELAYSERVER_H_
