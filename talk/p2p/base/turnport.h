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
#include <list>
#include <set>
#include <string>

#include "talk/p2p/base/port.h"
#include "talk/p2p/client/basicportallocator.h"
#include "webrtc/base/asyncpacketsocket.h"

namespace rtc {
class AsyncResolver;
class SignalThread;
}

namespace cricket {

extern const char TURN_PORT_TYPE[];
class TurnAllocateRequest;
class TurnEntry;

class TurnPort : public Port {
 public:
  static TurnPort* Create(rtc::Thread* thread,
                          rtc::PacketSocketFactory* factory,
                          rtc::Network* network,
                          rtc::AsyncPacketSocket* socket,
                          const std::string& username,  // ice username.
                          const std::string& password,  // ice password.
                          const ProtocolAddress& server_address,
                          const RelayCredentials& credentials,
                          int server_priority) {
    return new TurnPort(thread, factory, network, socket,
                        username, password, server_address,
                        credentials, server_priority);
  }

  static TurnPort* Create(rtc::Thread* thread,
                          rtc::PacketSocketFactory* factory,
                          rtc::Network* network,
                          const rtc::IPAddress& ip,
                          int min_port, int max_port,
                          const std::string& username,  // ice username.
                          const std::string& password,  // ice password.
                          const ProtocolAddress& server_address,
                          const RelayCredentials& credentials,
                          int server_priority) {
    return new TurnPort(thread, factory, network, ip, min_port, max_port,
                        username, password, server_address, credentials,
                        server_priority);
  }

  virtual ~TurnPort();

  const ProtocolAddress& server_address() const { return server_address_; }

  bool connected() const { return connected_; }
  const RelayCredentials& credentials() const { return credentials_; }

  virtual void PrepareAddress();
  virtual Connection* CreateConnection(
      const Candidate& c, PortInterface::CandidateOrigin origin);
  virtual int SendTo(const void* data, size_t size,
                     const rtc::SocketAddress& addr,
                     const rtc::PacketOptions& options,
                     bool payload);
  virtual int SetOption(rtc::Socket::Option opt, int value);
  virtual int GetOption(rtc::Socket::Option opt, int* value);
  virtual int GetError();

  virtual bool HandleIncomingPacket(
      rtc::AsyncPacketSocket* socket, const char* data, size_t size,
      const rtc::SocketAddress& remote_addr,
      const rtc::PacketTime& packet_time) {
    OnReadPacket(socket, data, size, remote_addr, packet_time);
    return true;
  }
  virtual void OnReadPacket(rtc::AsyncPacketSocket* socket,
                            const char* data, size_t size,
                            const rtc::SocketAddress& remote_addr,
                            const rtc::PacketTime& packet_time);

  virtual void OnReadyToSend(rtc::AsyncPacketSocket* socket);

  void OnSocketConnect(rtc::AsyncPacketSocket* socket);
  void OnSocketClose(rtc::AsyncPacketSocket* socket, int error);


  const std::string& hash() const { return hash_; }
  const std::string& nonce() const { return nonce_; }

  int error() const { return error_; }

  // Signal with resolved server address.
  // Parameters are port, server address and resolved server address.
  // This signal will be sent only if server address is resolved successfully.
  sigslot::signal3<TurnPort*,
                   const rtc::SocketAddress&,
                   const rtc::SocketAddress&> SignalResolvedServerAddress;

  // This signal is only for testing purpose.
  sigslot::signal3<TurnPort*, const rtc::SocketAddress&, int>
      SignalCreatePermissionResult;

 protected:
  TurnPort(rtc::Thread* thread,
           rtc::PacketSocketFactory* factory,
           rtc::Network* network,
           rtc::AsyncPacketSocket* socket,
           const std::string& username,
           const std::string& password,
           const ProtocolAddress& server_address,
           const RelayCredentials& credentials,
           int server_priority);

  TurnPort(rtc::Thread* thread,
           rtc::PacketSocketFactory* factory,
           rtc::Network* network,
           const rtc::IPAddress& ip,
           int min_port, int max_port,
           const std::string& username,
           const std::string& password,
           const ProtocolAddress& server_address,
           const RelayCredentials& credentials,
           int server_priority);

 private:
  enum { MSG_ERROR = MSG_FIRST_AVAILABLE };

  typedef std::list<TurnEntry*> EntryList;
  typedef std::map<rtc::Socket::Option, int> SocketOptionsMap;
  typedef std::set<rtc::SocketAddress> AttemptedServerSet;

  virtual void OnMessage(rtc::Message* pmsg);

  bool CreateTurnClientSocket();

  void set_nonce(const std::string& nonce) { nonce_ = nonce; }
  void set_realm(const std::string& realm) {
    if (realm != realm_) {
      realm_ = realm;
      UpdateHash();
    }
  }

  bool SetAlternateServer(const rtc::SocketAddress& address);
  void ResolveTurnAddress(const rtc::SocketAddress& address);
  void OnResolveResult(rtc::AsyncResolverInterface* resolver);

  void AddRequestAuthInfo(StunMessage* msg);
  void OnSendStunPacket(const void* data, size_t size, StunRequest* request);
  // Stun address from allocate success response.
  // Currently used only for testing.
  void OnStunAddress(const rtc::SocketAddress& address);
  void OnAllocateSuccess(const rtc::SocketAddress& address,
                         const rtc::SocketAddress& stun_address);
  void OnAllocateError();
  void OnAllocateRequestTimeout();

  void HandleDataIndication(const char* data, size_t size,
                            const rtc::PacketTime& packet_time);
  void HandleChannelData(int channel_id, const char* data, size_t size,
                         const rtc::PacketTime& packet_time);
  void DispatchPacket(const char* data, size_t size,
      const rtc::SocketAddress& remote_addr,
      ProtocolType proto, const rtc::PacketTime& packet_time);

  bool ScheduleRefresh(int lifetime);
  void SendRequest(StunRequest* request, int delay);
  int Send(const void* data, size_t size,
           const rtc::PacketOptions& options);
  void UpdateHash();
  bool UpdateNonce(StunMessage* response);

  bool HasPermission(const rtc::IPAddress& ipaddr) const;
  TurnEntry* FindEntry(const rtc::SocketAddress& address) const;
  TurnEntry* FindEntry(int channel_id) const;
  TurnEntry* CreateEntry(const rtc::SocketAddress& address);
  void DestroyEntry(const rtc::SocketAddress& address);
  void OnConnectionDestroyed(Connection* conn);

  ProtocolAddress server_address_;
  RelayCredentials credentials_;
  AttemptedServerSet attempted_server_addresses_;

  rtc::AsyncPacketSocket* socket_;
  SocketOptionsMap socket_options_;
  rtc::AsyncResolverInterface* resolver_;
  int error_;

  StunRequestManager request_manager_;
  std::string realm_;       // From 401/438 response message.
  std::string nonce_;       // From 401/438 response message.
  std::string hash_;        // Digest of username:realm:password

  int next_channel_number_;
  EntryList entries_;

  bool connected_;
  // By default the value will be set to 0. This value will be used in
  // calculating the candidate priority.
  int server_priority_;

  friend class TurnEntry;
  friend class TurnAllocateRequest;
  friend class TurnRefreshRequest;
  friend class TurnCreatePermissionRequest;
  friend class TurnChannelBindRequest;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_TURNPORT_H_
