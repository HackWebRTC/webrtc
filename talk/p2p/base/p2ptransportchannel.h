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

// P2PTransportChannel wraps up the state management of the connection between
// two P2P clients.  Clients have candidate ports for connecting, and
// connections which are combinations of candidates from each end (Alice and
// Bob each have candidates, one candidate from Alice and one candidate from
// Bob are used to make a connection, repeat to make many connections).
//
// When all of the available connections become invalid (non-writable), we
// kick off a process of determining more candidates and more connections.
//
#ifndef TALK_P2P_BASE_P2PTRANSPORTCHANNEL_H_
#define TALK_P2P_BASE_P2PTRANSPORTCHANNEL_H_

#include <map>
#include <vector>
#include <string>
#include "talk/base/sigslot.h"
#include "talk/p2p/base/candidate.h"
#include "talk/p2p/base/portinterface.h"
#include "talk/p2p/base/portallocator.h"
#include "talk/p2p/base/transport.h"
#include "talk/p2p/base/transportchannelimpl.h"
#include "talk/p2p/base/p2ptransport.h"

namespace cricket {

// Adds the port on which the candidate originated.
class RemoteCandidate : public Candidate {
 public:
  RemoteCandidate(const Candidate& c, PortInterface* origin_port)
      : Candidate(c), origin_port_(origin_port) {}

  PortInterface* origin_port() { return origin_port_; }

 private:
  PortInterface* origin_port_;
};

// P2PTransportChannel manages the candidates and connection process to keep
// two P2P clients connected to each other.
class P2PTransportChannel : public TransportChannelImpl,
                            public talk_base::MessageHandler {
 public:
  P2PTransportChannel(const std::string& content_name,
                      int component,
                      P2PTransport* transport,
                      PortAllocator *allocator);
  virtual ~P2PTransportChannel();

  // From TransportChannelImpl:
  virtual Transport* GetTransport() { return transport_; }
  virtual void SetIceRole(IceRole role);
  virtual IceRole GetIceRole() const { return ice_role_; }
  virtual void SetIceTiebreaker(uint64 tiebreaker);
  virtual void SetIceProtocolType(IceProtocolType type);
  virtual void SetIceCredentials(const std::string& ice_ufrag,
                                 const std::string& ice_pwd);
  virtual void SetRemoteIceCredentials(const std::string& ice_ufrag,
                                       const std::string& ice_pwd);
  virtual void SetRemoteIceMode(IceMode mode);
  virtual void Connect();
  virtual void Reset();
  virtual void OnSignalingReady();
  virtual void OnCandidate(const Candidate& candidate);

  // From TransportChannel:
  virtual int SendPacket(const char *data, size_t len, int flags);
  virtual int SetOption(talk_base::Socket::Option opt, int value);
  virtual int GetError() { return error_; }
  virtual bool GetStats(std::vector<ConnectionInfo>* stats);

  const Connection* best_connection() const { return best_connection_; }
  void set_incoming_only(bool value) { incoming_only_ = value; }

  // Note: This is only for testing purpose.
  // |ports_| should not be changed from outside.
  const std::vector<PortInterface *>& ports() { return ports_; }

  IceMode remote_ice_mode() const { return remote_ice_mode_; }

 private:
  talk_base::Thread* thread() { return worker_thread_; }
  PortAllocatorSession* allocator_session() {
    return allocator_sessions_.back();
  }

  void Allocate();
  void UpdateConnectionStates();
  void RequestSort();
  void SortConnections();
  void SwitchBestConnectionTo(Connection* conn);
  void UpdateChannelState();
  void HandleWritable();
  void HandleNotWritable();
  void HandleAllTimedOut();

  Connection* GetBestConnectionOnNetwork(talk_base::Network* network);
  bool CreateConnections(const Candidate &remote_candidate,
                         PortInterface* origin_port, bool readable);
  bool CreateConnection(PortInterface* port, const Candidate& remote_candidate,
                        PortInterface* origin_port, bool readable);
  bool FindConnection(cricket::Connection* connection) const;

  uint32 GetRemoteCandidateGeneration(const Candidate& candidate);
  void RememberRemoteCandidate(const Candidate& remote_candidate,
                               PortInterface* origin_port);
  bool IsPingable(Connection* conn);
  Connection* FindNextPingableConnection();
  void PingConnection(Connection* conn);
  void AddAllocatorSession(PortAllocatorSession* session);
  void AddConnection(Connection* connection);

  void OnPortReady(PortAllocatorSession *session, PortInterface* port);
  void OnCandidatesReady(PortAllocatorSession *session,
                         const std::vector<Candidate>& candidates);
  void OnCandidatesAllocationDone(PortAllocatorSession* session);
  void OnUnknownAddress(PortInterface* port,
                        const talk_base::SocketAddress& addr,
                        ProtocolType proto,
                        IceMessage* stun_msg,
                        const std::string& remote_username,
                        bool port_muxed);
  void OnPortDestroyed(PortInterface* port);
  void OnRoleConflict(PortInterface* port);

  void OnConnectionStateChange(Connection *connection);
  void OnReadPacket(Connection *connection, const char *data, size_t len);
  void OnReadyToSend(Connection* connection);
  void OnConnectionDestroyed(Connection *connection);

  void OnUseCandidate(Connection* conn);

  virtual void OnMessage(talk_base::Message *pmsg);
  void OnSort();
  void OnPing();

  P2PTransport* transport_;
  PortAllocator *allocator_;
  talk_base::Thread *worker_thread_;
  bool incoming_only_;
  bool waiting_for_signaling_;
  int error_;
  std::vector<PortAllocatorSession*> allocator_sessions_;
  std::vector<PortInterface *> ports_;
  std::vector<Connection *> connections_;
  Connection* best_connection_;
  // Connection selected by the controlling agent. This should be used only
  // at controlled side when protocol type is RFC5245.
  Connection* pending_best_connection_;
  std::vector<RemoteCandidate> remote_candidates_;
  bool sort_dirty_;  // indicates whether another sort is needed right now
  bool was_writable_;
  typedef std::map<talk_base::Socket::Option, int> OptionMap;
  OptionMap options_;
  std::string ice_ufrag_;
  std::string ice_pwd_;
  std::string remote_ice_ufrag_;
  std::string remote_ice_pwd_;
  IceProtocolType protocol_type_;
  IceMode remote_ice_mode_;
  IceRole ice_role_;
  uint64 tiebreaker_;
  uint32 remote_candidate_generation_;

  DISALLOW_EVIL_CONSTRUCTORS(P2PTransportChannel);
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_P2PTRANSPORTCHANNEL_H_
