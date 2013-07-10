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

#ifndef TALK_P2P_BASE_TURNPORT_H_
#define TALK_P2P_BASE_TURNPORT_H_

#include <stdio.h>
#include <string>
#include <list>

#include "talk/p2p/base/port.h"
#include "talk/p2p/client/basicportallocator.h"

namespace talk_base {
class AsyncPacketSocket;
class AsyncResolver;
class SignalThread;
}

namespace cricket {

extern const char TURN_PORT_TYPE[];
class TurnAllocateRequest;
class TurnEntry;

class TurnPort : public Port {
 public:
  static TurnPort* Create(talk_base::Thread* thread,
                          talk_base::PacketSocketFactory* factory,
                          talk_base::Network* network,
                          const talk_base::IPAddress& ip,
                          int min_port, int max_port,
                          const std::string& username,  // ice username.
                          const std::string& password,  // ice password.
                          const ProtocolAddress& server_address,
                          const RelayCredentials& credentials) {
    return new TurnPort(thread, factory, network, ip, min_port, max_port,
                        username, password, server_address, credentials);
  }

  virtual ~TurnPort();

  const ProtocolAddress& server_address() const { return server_address_; }

  bool connected() const { return connected_; }
  const RelayCredentials& credentials() const { return credentials_; }

  virtual void PrepareAddress();
  virtual Connection* CreateConnection(
      const Candidate& c, PortInterface::CandidateOrigin origin);
  virtual int SendTo(const void* data, size_t size,
                     const talk_base::SocketAddress& addr,
                     bool payload);
  virtual int SetOption(talk_base::Socket::Option opt, int value);
  virtual int GetOption(talk_base::Socket::Option opt, int* value);
  virtual int GetError();
  virtual void OnReadPacket(talk_base::AsyncPacketSocket* socket,
                            const char* data, size_t size,
                            const talk_base::SocketAddress& remote_addr);
  virtual void OnReadyToSend(talk_base::AsyncPacketSocket* socket);

  void OnSocketConnect(talk_base::AsyncPacketSocket* socket);
  void OnSocketClose(talk_base::AsyncPacketSocket* socket, int error);


  const std::string& hash() const { return hash_; }
  const std::string& nonce() const { return nonce_; }

  // This signal is only for testing purpose.
  sigslot::signal3<TurnPort*, const talk_base::SocketAddress&, int>
      SignalCreatePermissionResult;

 protected:
  TurnPort(talk_base::Thread* thread,
           talk_base::PacketSocketFactory* factory,
           talk_base::Network* network,
           const talk_base::IPAddress& ip,
           int min_port, int max_port,
           const std::string& username,
           const std::string& password,
           const ProtocolAddress& server_address,
           const RelayCredentials& credentials);

 private:
  typedef std::list<TurnEntry*> EntryList;
  typedef std::map<talk_base::Socket::Option, int> SocketOptionsMap;

  virtual void OnMessage(talk_base::Message* pmsg);

  void set_nonce(const std::string& nonce) { nonce_ = nonce; }
  void set_realm(const std::string& realm) {
    if (realm != realm_) {
      realm_ = realm;
      UpdateHash();
    }
  }

  void ResolveTurnAddress(const talk_base::SocketAddress& address);
  void OnResolveResult(talk_base::SignalThread* signal_thread);

  void AddRequestAuthInfo(StunMessage* msg);
  void OnSendStunPacket(const void* data, size_t size, StunRequest* request);
  // Stun address from allocate success response.
  // Currently used only for testing.
  void OnStunAddress(const talk_base::SocketAddress& address);
  void OnAllocateSuccess(const talk_base::SocketAddress& address);
  void OnAllocateError();
  void OnAllocateRequestTimeout();

  void HandleDataIndication(const char* data, size_t size);
  void HandleChannelData(int channel_id, const char* data, size_t size);
  void DispatchPacket(const char* data, size_t size,
      const talk_base::SocketAddress& remote_addr, ProtocolType proto);

  bool ScheduleRefresh(int lifetime);
  void SendRequest(StunRequest* request, int delay);
  int Send(const void* data, size_t size);
  void UpdateHash();
  bool UpdateNonce(StunMessage* response);

  bool HasPermission(const talk_base::IPAddress& ipaddr) const;
  TurnEntry* FindEntry(const talk_base::SocketAddress& address) const;
  TurnEntry* FindEntry(int channel_id) const;
  TurnEntry* CreateEntry(const talk_base::SocketAddress& address);
  void DestroyEntry(const talk_base::SocketAddress& address);
  void OnConnectionDestroyed(Connection* conn);

  ProtocolAddress server_address_;
  RelayCredentials credentials_;

  talk_base::scoped_ptr<talk_base::AsyncPacketSocket> socket_;
  SocketOptionsMap socket_options_;
  talk_base::AsyncResolver* resolver_;
  int error_;

  StunRequestManager request_manager_;
  std::string realm_;       // From 401/438 response message.
  std::string nonce_;       // From 401/438 response message.
  std::string hash_;        // Digest of username:realm:password

  int next_channel_number_;
  EntryList entries_;

  bool connected_;

  friend class TurnEntry;
  friend class TurnAllocateRequest;
  friend class TurnRefreshRequest;
  friend class TurnCreatePermissionRequest;
  friend class TurnChannelBindRequest;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_TURNPORT_H_
