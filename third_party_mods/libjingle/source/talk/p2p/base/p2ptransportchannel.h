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
#include "talk/p2p/base/port.h"
#include "talk/p2p/base/portallocator.h"
#include "talk/p2p/base/transport.h"
#include "talk/p2p/base/transportchannelimpl.h"
#include "talk/p2p/base/p2ptransport.h"

namespace cricket {

// Adds the port on which the candidate originated.
class RemoteCandidate : public Candidate {
 public:
  RemoteCandidate(const Candidate& c, Port* origin_port)
    : Candidate(c), origin_port_(origin_port) {}

  Port* origin_port() { return origin_port_; }

 private:
  Port* origin_port_;
};

// P2PTransportChannel manages the candidates and connection process to keep
// two P2P clients connected to each other.
class P2PTransportChannel : public TransportChannelImpl,
    public talk_base::MessageHandler {
 public:
  P2PTransportChannel(const std::string &name,
                      const std::string &content_type,
                      P2PTransport* transport,
                      PortAllocator *allocator);
  virtual ~P2PTransportChannel();

  // From TransportChannelImpl:
  virtual Transport* GetTransport() { return transport_; }
  virtual void Connect();
  virtual void Reset();
  virtual void OnSignalingReady();

  // From TransportChannel:
  virtual int SendPacket(talk_base::Buffer* packet);
  virtual int SendPacket(const char *data, size_t len);
  virtual int SetOption(talk_base::Socket::Option opt, int value);
  virtual int GetError() { return error_; }

  // This hack is here to allow the SocketMonitor to downcast to the
  // P2PTransportChannel safely.
  virtual P2PTransportChannel* GetP2PChannel() { return this; }

  // These are used by the connection monitor.
  sigslot::signal1<P2PTransportChannel*> SignalConnectionMonitor;
  const std::vector<Connection *>& connections() const { return connections_; }
  Connection* best_connection() const { return best_connection_; }

  void set_incoming_only(bool value) { incoming_only_ = value; }

  // Handler for internal messages.
  virtual void OnMessage(talk_base::Message *pmsg);

  virtual void OnCandidate(const Candidate& candidate);

 private:
  void Allocate();
  void CancelPendingAllocate();
  void UpdateConnectionStates();
  void RequestSort();
  void SortConnections();
  void SwitchBestConnectionTo(Connection* conn);
  void UpdateChannelState();
  void HandleWritable();
  void HandleNotWritable();
  void HandleAllTimedOut();
  Connection* GetBestConnectionOnNetwork(talk_base::Network* network);
  bool CreateConnections(const Candidate &remote_candidate, Port* origin_port,
                         bool readable);
  bool CreateConnection(Port* port, const Candidate& remote_candidate,
                        Port* origin_port, bool readable);
  void RememberRemoteCandidate(const Candidate& remote_candidate,
                               Port* origin_port);
  void OnUnknownAddress(Port *port, const talk_base::SocketAddress &addr,
                        StunMessage *stun_msg,
                        const std::string &remote_username);
  void OnPortReady(PortAllocatorSession *session, Port* port);
  void OnCandidatesReady(PortAllocatorSession *session,
                         const std::vector<Candidate>& candidates);
  void OnConnectionStateChange(Connection *connection);
  void OnConnectionDestroyed(Connection *connection);
  void OnPortDestroyed(Port* port);
  void OnReadPacket(Connection *connection, const char *data, size_t len);
  void OnSort();
  void OnPing();
  bool IsPingable(Connection* conn);
  Connection* FindNextPingableConnection();
  uint32 NumPingableConnections();
  PortAllocatorSession* allocator_session() {
    return allocator_sessions_.back();
  }
  void AddAllocatorSession(PortAllocatorSession* session);

  talk_base::Thread* thread() const { return worker_thread_; }

  P2PTransport* transport_;
  PortAllocator *allocator_;
  talk_base::Thread *worker_thread_;
  bool incoming_only_;
  bool waiting_for_signaling_;
  int error_;
  std::vector<PortAllocatorSession*> allocator_sessions_;
  std::vector<Port *> ports_;
  std::vector<Connection *> connections_;
  Connection *best_connection_;
  std::vector<RemoteCandidate> remote_candidates_;
  // indicates whether StartGetAllCandidates has been called
  bool pinging_started_;
  bool sort_dirty_;  // indicates whether another sort is needed right now
  bool was_writable_;
  bool was_timed_out_;
  typedef std::map<talk_base::Socket::Option, int> OptionMap;
  OptionMap options_;

  DISALLOW_EVIL_CONSTRUCTORS(P2PTransportChannel);
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_P2PTRANSPORTCHANNEL_H_
