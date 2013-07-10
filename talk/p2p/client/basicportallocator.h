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

#ifndef TALK_P2P_CLIENT_BASICPORTALLOCATOR_H_
#define TALK_P2P_CLIENT_BASICPORTALLOCATOR_H_

#include <string>
#include <vector>

#include "talk/base/messagequeue.h"
#include "talk/base/network.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/port.h"
#include "talk/p2p/base/portallocator.h"

namespace cricket {

struct RelayCredentials {
  RelayCredentials() {}
  RelayCredentials(const std::string& username,
                   const std::string& password)
      : username(username),
        password(password) {
  }

  std::string username;
  std::string password;
};

typedef std::vector<ProtocolAddress> PortList;
struct RelayServerConfig {
  RelayServerConfig(RelayType type) : type(type) {}

  RelayType type;
  PortList ports;
  RelayCredentials credentials;
};

class BasicPortAllocator : public PortAllocator {
 public:
  BasicPortAllocator(talk_base::NetworkManager* network_manager,
                     talk_base::PacketSocketFactory* socket_factory);
  explicit BasicPortAllocator(talk_base::NetworkManager* network_manager);
  BasicPortAllocator(talk_base::NetworkManager* network_manager,
                     talk_base::PacketSocketFactory* socket_factory,
                     const talk_base::SocketAddress& stun_server);
  BasicPortAllocator(talk_base::NetworkManager* network_manager,
                     const talk_base::SocketAddress& stun_server,
                     const talk_base::SocketAddress& relay_server_udp,
                     const talk_base::SocketAddress& relay_server_tcp,
                     const talk_base::SocketAddress& relay_server_ssl);
  virtual ~BasicPortAllocator();

  talk_base::NetworkManager* network_manager() { return network_manager_; }

  // If socket_factory() is set to NULL each PortAllocatorSession
  // creates its own socket factory.
  talk_base::PacketSocketFactory* socket_factory() { return socket_factory_; }

  const talk_base::SocketAddress& stun_address() const {
    return stun_address_;
  }

  const std::vector<RelayServerConfig>& relays() const {
    return relays_;
  }
  virtual void AddRelay(const RelayServerConfig& relay) {
    relays_.push_back(relay);
  }

  virtual PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd);

  bool allow_tcp_listen() const {
    return allow_tcp_listen_;
  }
  void set_allow_tcp_listen(bool allow_tcp_listen) {
    allow_tcp_listen_ = allow_tcp_listen;
  }

 private:
  void Construct();

  talk_base::NetworkManager* network_manager_;
  talk_base::PacketSocketFactory* socket_factory_;
  const talk_base::SocketAddress stun_address_;
  std::vector<RelayServerConfig> relays_;
  bool allow_tcp_listen_;
};

struct PortConfiguration;
class AllocationSequence;

class BasicPortAllocatorSession : public PortAllocatorSession,
                                  public talk_base::MessageHandler {
 public:
  BasicPortAllocatorSession(BasicPortAllocator* allocator,
                            const std::string& content_name,
                            int component,
                            const std::string& ice_ufrag,
                            const std::string& ice_pwd);
  ~BasicPortAllocatorSession();

  virtual BasicPortAllocator* allocator() { return allocator_; }
  talk_base::Thread* network_thread() { return network_thread_; }
  talk_base::PacketSocketFactory* socket_factory() { return socket_factory_; }

  virtual void StartGettingPorts();
  virtual void StopGettingPorts();
  virtual bool IsGettingPorts() { return running_; }

 protected:
  // Starts the process of getting the port configurations.
  virtual void GetPortConfigurations();

  // Adds a port configuration that is now ready.  Once we have one for each
  // network (or a timeout occurs), we will start allocating ports.
  virtual void ConfigReady(PortConfiguration* config);

  // MessageHandler.  Can be overriden if message IDs do not conflict.
  virtual void OnMessage(talk_base::Message *message);

 private:
  class PortData {
   public:
    PortData() : port_(NULL), sequence_(NULL), state_(STATE_INIT) {}
    PortData(Port* port, AllocationSequence* seq)
    : port_(port), sequence_(seq), state_(STATE_INIT) {
    }

    Port* port() { return port_; }
    AllocationSequence* sequence() { return sequence_; }
    bool ready() const { return state_ == STATE_READY; }
    bool complete() const {
      // Returns true if candidate allocation has completed one way or another.
      return ((state_ == STATE_COMPLETE) || (state_ == STATE_ERROR));
    }

    void set_ready() { ASSERT(state_ == STATE_INIT); state_ = STATE_READY; }
    void set_complete() {
      ASSERT(state_ == STATE_READY);
      state_ = STATE_COMPLETE;
    }
    void set_error() {
      ASSERT(state_ == STATE_INIT || state_ == STATE_READY);
      state_ = STATE_ERROR;
    }

   private:
    enum State {
      STATE_INIT,      // No candidates allocated yet.
      STATE_READY,     // At least one candidate is ready for process.
      STATE_COMPLETE,  // All candidates allocated and ready for process.
      STATE_ERROR      // Error in gathering candidates.
    };
    Port* port_;
    AllocationSequence* sequence_;
    State state_;
  };

  void OnConfigReady(PortConfiguration* config);
  void OnConfigStop();
  void AllocatePorts();
  void OnAllocate();
  void DoAllocate();
  void OnNetworksChanged();
  void OnAllocationSequenceObjectsCreated();
  void DisableEquivalentPhases(talk_base::Network* network,
                               PortConfiguration* config, uint32* flags);
  void AddAllocatedPort(Port* port, AllocationSequence* seq,
                        bool prepare_address);
  void OnCandidateReady(Port* port, const Candidate& c);
  void OnPortComplete(Port* port);
  void OnPortError(Port* port);
  void OnProtocolEnabled(AllocationSequence* seq, ProtocolType proto);
  void OnPortDestroyed(PortInterface* port);
  void OnShake();
  void MaybeSignalCandidatesAllocationDone();
  void OnPortAllocationComplete(AllocationSequence* seq);
  PortData* FindPort(Port* port);

  BasicPortAllocator* allocator_;
  talk_base::Thread* network_thread_;
  talk_base::scoped_ptr<talk_base::PacketSocketFactory> owned_socket_factory_;
  talk_base::PacketSocketFactory* socket_factory_;
  bool configuration_done_;
  bool allocation_started_;
  bool network_manager_started_;
  bool running_;  // set when StartGetAllPorts is called
  bool allocation_sequences_created_;
  std::vector<PortConfiguration*> configs_;
  std::vector<AllocationSequence*> sequences_;
  std::vector<PortData> ports_;

  friend class AllocationSequence;
};

// Records configuration information useful in creating ports.
struct PortConfiguration : public talk_base::MessageData {
  talk_base::SocketAddress stun_address;
  std::string username;
  std::string password;

  typedef std::vector<RelayServerConfig> RelayList;
  RelayList relays;

  PortConfiguration(const talk_base::SocketAddress& stun_address,
                    const std::string& username,
                    const std::string& password);

  // Adds another relay server, with the given ports and modifier, to the list.
  void AddRelay(const RelayServerConfig& config);

  // Determines whether the given relay server supports the given protocol.
  static bool SupportsProtocol(const RelayServerConfig& relay,
                               ProtocolType type);
};

}  // namespace cricket

#endif  // TALK_P2P_CLIENT_BASICPORTALLOCATOR_H_
