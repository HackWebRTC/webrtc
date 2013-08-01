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

#include "talk/p2p/client/basicportallocator.h"

#include <string>
#include <vector>

#include "talk/base/common.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/p2p/base/basicpacketsocketfactory.h"
#include "talk/p2p/base/common.h"
#include "talk/p2p/base/port.h"
#include "talk/p2p/base/relayport.h"
#include "talk/p2p/base/stunport.h"
#include "talk/p2p/base/tcpport.h"
#include "talk/p2p/base/turnport.h"
#include "talk/p2p/base/udpport.h"

using talk_base::CreateRandomId;
using talk_base::CreateRandomString;

namespace {

const uint32 MSG_CONFIG_START = 1;
const uint32 MSG_CONFIG_READY = 2;
const uint32 MSG_ALLOCATE = 3;
const uint32 MSG_ALLOCATION_PHASE = 4;
const uint32 MSG_SHAKE = 5;
const uint32 MSG_SEQUENCEOBJECTS_CREATED = 6;
const uint32 MSG_CONFIG_STOP = 7;

const uint32 ALLOCATE_DELAY = 250;
const uint32 ALLOCATION_STEP_DELAY = 1 * 1000;

const int PHASE_UDP = 0;
const int PHASE_RELAY = 1;
const int PHASE_TCP = 2;
const int PHASE_SSLTCP = 3;

const int kNumPhases = 4;

// Both these values are in bytes.
const int kLargeSocketSendBufferSize = 128 * 1024;
const int kNormalSocketSendBufferSize = 64 * 1024;

const int SHAKE_MIN_DELAY = 45 * 1000;  // 45 seconds
const int SHAKE_MAX_DELAY = 90 * 1000;  // 90 seconds

int ShakeDelay() {
  int range = SHAKE_MAX_DELAY - SHAKE_MIN_DELAY + 1;
  return SHAKE_MIN_DELAY + CreateRandomId() % range;
}

}  // namespace

namespace cricket {

const uint32 DISABLE_ALL_PHASES =
  PORTALLOCATOR_DISABLE_UDP
  | PORTALLOCATOR_DISABLE_TCP
  | PORTALLOCATOR_DISABLE_STUN
  | PORTALLOCATOR_DISABLE_RELAY;

// Performs the allocation of ports, in a sequenced (timed) manner, for a given
// network and IP address.
class AllocationSequence : public talk_base::MessageHandler,
                           public sigslot::has_slots<> {
 public:
  enum State {
    kInit,       // Initial state.
    kRunning,    // Started allocating ports.
    kStopped,    // Stopped from running.
    kCompleted,  // All ports are allocated.

    // kInit --> kRunning --> {kCompleted|kStopped}
  };

  AllocationSequence(BasicPortAllocatorSession* session,
                     talk_base::Network* network,
                     PortConfiguration* config,
                     uint32 flags);
  ~AllocationSequence();
  bool Init();

  State state() const { return state_; }

  // Disables the phases for a new sequence that this one already covers for an
  // equivalent network setup.
  void DisableEquivalentPhases(talk_base::Network* network,
      PortConfiguration* config, uint32* flags);

  // Starts and stops the sequence.  When started, it will continue allocating
  // new ports on its own timed schedule.
  void Start();
  void Stop();

  // MessageHandler
  void OnMessage(talk_base::Message* msg);

  void EnableProtocol(ProtocolType proto);
  bool ProtocolEnabled(ProtocolType proto) const;

  // Signal from AllocationSequence, when it's done with allocating ports.
  // This signal is useful, when port allocation fails which doesn't result
  // in any candidates. Using this signal BasicPortAllocatorSession can send
  // its candidate discovery conclusion signal. Without this signal,
  // BasicPortAllocatorSession doesn't have any event to trigger signal. This
  // can also be achieved by starting timer in BPAS.
  sigslot::signal1<AllocationSequence*> SignalPortAllocationComplete;

 private:
  typedef std::vector<ProtocolType> ProtocolList;

  bool IsFlagSet(uint32 flag) {
    return ((flags_ & flag) != 0);
  }
  void CreateUDPPorts();
  void CreateTCPPorts();
  void CreateStunPorts();
  void CreateRelayPorts();
  void CreateGturnPort(const RelayServerConfig& config);
  void CreateTurnPort(const RelayServerConfig& config);

  void OnReadPacket(talk_base::AsyncPacketSocket* socket,
                    const char* data, size_t size,
                    const talk_base::SocketAddress& remote_addr);
  void OnPortDestroyed(PortInterface* port);

  BasicPortAllocatorSession* session_;
  talk_base::Network* network_;
  talk_base::IPAddress ip_;
  PortConfiguration* config_;
  State state_;
  uint32 flags_;
  ProtocolList protocols_;
  talk_base::scoped_ptr<talk_base::AsyncPacketSocket> udp_socket_;
  // Keeping a list of all UDP based ports.
  std::deque<Port*> ports;
  int phase_;
};

// BasicPortAllocator
BasicPortAllocator::BasicPortAllocator(
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory)
    : network_manager_(network_manager),
      socket_factory_(socket_factory) {
  ASSERT(socket_factory_ != NULL);
  Construct();
}

BasicPortAllocator::BasicPortAllocator(
    talk_base::NetworkManager* network_manager)
    : network_manager_(network_manager),
      socket_factory_(NULL) {
  Construct();
}

BasicPortAllocator::BasicPortAllocator(
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory,
    const talk_base::SocketAddress& stun_address)
    : network_manager_(network_manager),
      socket_factory_(socket_factory),
      stun_address_(stun_address) {
  ASSERT(socket_factory_ != NULL);
  Construct();
}

BasicPortAllocator::BasicPortAllocator(
    talk_base::NetworkManager* network_manager,
    const talk_base::SocketAddress& stun_address,
    const talk_base::SocketAddress& relay_address_udp,
    const talk_base::SocketAddress& relay_address_tcp,
    const talk_base::SocketAddress& relay_address_ssl)
    : network_manager_(network_manager),
      socket_factory_(NULL),
      stun_address_(stun_address) {

  RelayServerConfig config(RELAY_GTURN);
  if (!relay_address_udp.IsAny())
    config.ports.push_back(ProtocolAddress(relay_address_udp, PROTO_UDP));
  if (!relay_address_tcp.IsAny())
    config.ports.push_back(ProtocolAddress(relay_address_tcp, PROTO_TCP));
  if (!relay_address_ssl.IsAny())
    config.ports.push_back(ProtocolAddress(relay_address_ssl, PROTO_SSLTCP));
  AddRelay(config);

  Construct();
}

void BasicPortAllocator::Construct() {
  allow_tcp_listen_ = true;
}

BasicPortAllocator::~BasicPortAllocator() {
}

PortAllocatorSession *BasicPortAllocator::CreateSessionInternal(
    const std::string& content_name, int component,
    const std::string& ice_ufrag, const std::string& ice_pwd) {
  return new BasicPortAllocatorSession(this, content_name, component,
                                       ice_ufrag, ice_pwd);
}

// BasicPortAllocatorSession
BasicPortAllocatorSession::BasicPortAllocatorSession(
    BasicPortAllocator *allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_ufrag,
    const std::string& ice_pwd)
    : PortAllocatorSession(content_name, component,
                           ice_ufrag, ice_pwd, allocator->flags()),
      allocator_(allocator), network_thread_(NULL),
      socket_factory_(allocator->socket_factory()),
      configuration_done_(false),
      allocation_started_(false),
      network_manager_started_(false),
      running_(false),
      allocation_sequences_created_(false) {
  allocator_->network_manager()->SignalNetworksChanged.connect(
      this, &BasicPortAllocatorSession::OnNetworksChanged);
  allocator_->network_manager()->StartUpdating();
}

BasicPortAllocatorSession::~BasicPortAllocatorSession() {
  allocator_->network_manager()->StopUpdating();
  if (network_thread_ != NULL)
    network_thread_->Clear(this);

  std::vector<PortData>::iterator it;
  for (it = ports_.begin(); it != ports_.end(); it++)
    delete it->port();

  for (uint32 i = 0; i < configs_.size(); ++i)
    delete configs_[i];

  for (uint32 i = 0; i < sequences_.size(); ++i)
    delete sequences_[i];
}

void BasicPortAllocatorSession::StartGettingPorts() {
  network_thread_ = talk_base::Thread::Current();
  if (!socket_factory_) {
    owned_socket_factory_.reset(
        new talk_base::BasicPacketSocketFactory(network_thread_));
    socket_factory_ = owned_socket_factory_.get();
  }

  running_ = true;
  network_thread_->Post(this, MSG_CONFIG_START);

  if (flags() & PORTALLOCATOR_ENABLE_SHAKER)
    network_thread_->PostDelayed(ShakeDelay(), this, MSG_SHAKE);
}

void BasicPortAllocatorSession::StopGettingPorts() {
  ASSERT(talk_base::Thread::Current() == network_thread_);
  running_ = false;
  network_thread_->Clear(this, MSG_ALLOCATE);
  for (uint32 i = 0; i < sequences_.size(); ++i)
    sequences_[i]->Stop();
  network_thread_->Post(this, MSG_CONFIG_STOP);
}

void BasicPortAllocatorSession::OnMessage(talk_base::Message *message) {
  switch (message->message_id) {
  case MSG_CONFIG_START:
    ASSERT(talk_base::Thread::Current() == network_thread_);
    GetPortConfigurations();
    break;

  case MSG_CONFIG_READY:
    ASSERT(talk_base::Thread::Current() == network_thread_);
    OnConfigReady(static_cast<PortConfiguration*>(message->pdata));
    break;

  case MSG_ALLOCATE:
    ASSERT(talk_base::Thread::Current() == network_thread_);
    OnAllocate();
    break;

  case MSG_SHAKE:
    ASSERT(talk_base::Thread::Current() == network_thread_);
    OnShake();
    break;
  case MSG_SEQUENCEOBJECTS_CREATED:
    ASSERT(talk_base::Thread::Current() == network_thread_);
    OnAllocationSequenceObjectsCreated();
    break;
  case MSG_CONFIG_STOP:
    ASSERT(talk_base::Thread::Current() == network_thread_);
    OnConfigStop();
    break;
  default:
    ASSERT(false);
  }
}

void BasicPortAllocatorSession::GetPortConfigurations() {
  PortConfiguration* config = new PortConfiguration(allocator_->stun_address(),
                                                    username(),
                                                    password());

  for (size_t i = 0; i < allocator_->relays().size(); ++i) {
    config->AddRelay(allocator_->relays()[i]);
  }
  ConfigReady(config);
}

void BasicPortAllocatorSession::ConfigReady(PortConfiguration* config) {
  network_thread_->Post(this, MSG_CONFIG_READY, config);
}

// Adds a configuration to the list.
void BasicPortAllocatorSession::OnConfigReady(PortConfiguration* config) {
  if (config)
    configs_.push_back(config);

  AllocatePorts();
}

void BasicPortAllocatorSession::OnConfigStop() {
  ASSERT(talk_base::Thread::Current() == network_thread_);

  // If any of the allocated ports have not completed the candidates allocation,
  // mark those as error. Since session doesn't need any new candidates
  // at this stage of the allocation, it's safe to discard any new candidates.
  bool send_signal = false;
  for (std::vector<PortData>::iterator it = ports_.begin();
       it != ports_.end(); ++it) {
    if (!it->complete()) {
      // Updating port state to error, which didn't finish allocating candidates
      // yet.
      it->set_error();
      send_signal = true;
    }
  }

  // Did we stop any running sequences?
  for (std::vector<AllocationSequence*>::iterator it = sequences_.begin();
       it != sequences_.end() && !send_signal; ++it) {
    if ((*it)->state() == AllocationSequence::kStopped) {
      send_signal = true;
    }
  }

  // If we stopped anything that was running, send a done signal now.
  if (send_signal) {
    MaybeSignalCandidatesAllocationDone();
  }
}

void BasicPortAllocatorSession::AllocatePorts() {
  ASSERT(talk_base::Thread::Current() == network_thread_);
  network_thread_->Post(this, MSG_ALLOCATE);
}

void BasicPortAllocatorSession::OnAllocate() {
  if (network_manager_started_)
    DoAllocate();

  allocation_started_ = true;
  if (running_)
    network_thread_->PostDelayed(ALLOCATE_DELAY, this, MSG_ALLOCATE);
}

// For each network, see if we have a sequence that covers it already.  If not,
// create a new sequence to create the appropriate ports.
void BasicPortAllocatorSession::DoAllocate() {
  bool done_signal_needed = false;
  std::vector<talk_base::Network*> networks;
  allocator_->network_manager()->GetNetworks(&networks);
  if (networks.empty()) {
    LOG(LS_WARNING) << "Machine has no networks; no ports will be allocated";
    done_signal_needed = true;
  } else {
    for (uint32 i = 0; i < networks.size(); ++i) {
      PortConfiguration* config = NULL;
      if (configs_.size() > 0)
        config = configs_.back();

      uint32 sequence_flags = flags();
      if ((sequence_flags & DISABLE_ALL_PHASES) == DISABLE_ALL_PHASES) {
        // If all the ports are disabled we should just fire the allocation
        // done event and return.
        done_signal_needed = true;
        break;
      }

      // Disables phases that are not specified in this config.
      if (!config || config->stun_address.IsNil()) {
        // No STUN ports specified in this config.
        sequence_flags |= PORTALLOCATOR_DISABLE_STUN;
      }
      if (!config || config->relays.empty()) {
        // No relay ports specified in this config.
        sequence_flags |= PORTALLOCATOR_DISABLE_RELAY;
      }

      if (!(sequence_flags & PORTALLOCATOR_ENABLE_IPV6) &&
          networks[i]->ip().family() == AF_INET6) {
        // Skip IPv6 networks unless the flag's been set.
        continue;
      }

      // Disable phases that would only create ports equivalent to
      // ones that we have already made.
      DisableEquivalentPhases(networks[i], config, &sequence_flags);

      if ((sequence_flags & DISABLE_ALL_PHASES) == DISABLE_ALL_PHASES) {
        // New AllocationSequence would have nothing to do, so don't make it.
        continue;
      }

      AllocationSequence* sequence =
          new AllocationSequence(this, networks[i], config, sequence_flags);
      if (!sequence->Init()) {
        delete sequence;
        continue;
      }
      done_signal_needed = true;
      sequence->SignalPortAllocationComplete.connect(
          this, &BasicPortAllocatorSession::OnPortAllocationComplete);
      if (running_)
        sequence->Start();
      sequences_.push_back(sequence);
    }
  }
  if (done_signal_needed) {
    network_thread_->Post(this, MSG_SEQUENCEOBJECTS_CREATED);
  }
}

void BasicPortAllocatorSession::OnNetworksChanged() {
  network_manager_started_ = true;
  if (allocation_started_)
    DoAllocate();
}

void BasicPortAllocatorSession::DisableEquivalentPhases(
    talk_base::Network* network, PortConfiguration* config, uint32* flags) {
  for (uint32 i = 0; i < sequences_.size() &&
      (*flags & DISABLE_ALL_PHASES) != DISABLE_ALL_PHASES; ++i) {
    sequences_[i]->DisableEquivalentPhases(network, config, flags);
  }
}

void BasicPortAllocatorSession::AddAllocatedPort(Port* port,
                                                 AllocationSequence * seq,
                                                 bool prepare_address) {
  if (!port)
    return;

  LOG(LS_INFO) << "Adding allocated port for " << content_name();
  port->set_content_name(content_name());
  port->set_component(component_);
  port->set_generation(generation());
  if (allocator_->proxy().type != talk_base::PROXY_NONE)
    port->set_proxy(allocator_->user_agent(), allocator_->proxy());
  port->set_send_retransmit_count_attribute((allocator_->flags() &
      PORTALLOCATOR_ENABLE_STUN_RETRANSMIT_ATTRIBUTE) != 0);

  if (content_name().compare(CN_VIDEO) == 0 &&
      component_ == cricket::ICE_CANDIDATE_COMPONENT_RTP) {
    // For video RTP alone, we set send-buffer sizes. This used to be set in the
    // engines/channels.
    int sendBufSize = (flags() & PORTALLOCATOR_USE_LARGE_SOCKET_SEND_BUFFERS)
                      ? kLargeSocketSendBufferSize
                      : kNormalSocketSendBufferSize;
    port->SetOption(talk_base::Socket::OPT_SNDBUF, sendBufSize);
  }

  PortData data(port, seq);
  ports_.push_back(data);

  port->SignalCandidateReady.connect(
      this, &BasicPortAllocatorSession::OnCandidateReady);
  port->SignalPortComplete.connect(this,
      &BasicPortAllocatorSession::OnPortComplete);
  port->SignalDestroyed.connect(this,
      &BasicPortAllocatorSession::OnPortDestroyed);
  port->SignalPortError.connect(
      this, &BasicPortAllocatorSession::OnPortError);
  LOG_J(LS_INFO, port) << "Added port to allocator";

  if (prepare_address)
    port->PrepareAddress();
  if (running_)
    port->Start();
}

void BasicPortAllocatorSession::OnAllocationSequenceObjectsCreated() {
  allocation_sequences_created_ = true;
  // Send candidate allocation complete signal if we have no sequences.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::OnCandidateReady(
    Port* port, const Candidate& c) {
  ASSERT(talk_base::Thread::Current() == network_thread_);
  PortData* data = FindPort(port);
  ASSERT(data != NULL);
  // Discarding any candidate signal if port allocation status is
  // already in completed state.
  if (data->complete())
    return;

  // Send candidates whose protocol is enabled.
  std::vector<Candidate> candidates;
  ProtocolType pvalue;
  if (StringToProto(c.protocol().c_str(), &pvalue) &&
      data->sequence()->ProtocolEnabled(pvalue)) {
    candidates.push_back(c);
  }

  if (!candidates.empty()) {
    SignalCandidatesReady(this, candidates);
  }

  // Moving to READY state as we have atleast one candidate from the port.
  // Since this port has atleast one candidate we should forward this port
  // to listners, to allow connections from this port.
  if (!data->ready()) {
    data->set_ready();
    SignalPortReady(this, port);
  }
}

void BasicPortAllocatorSession::OnPortComplete(Port* port) {
  ASSERT(talk_base::Thread::Current() == network_thread_);
  PortData* data = FindPort(port);
  ASSERT(data != NULL);

  // Ignore any late signals.
  if (data->complete())
    return;

  // Moving to COMPLETE state.
  data->set_complete();
  // Send candidate allocation complete signal if this was the last port.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::OnPortError(Port* port) {
  ASSERT(talk_base::Thread::Current() == network_thread_);
  PortData* data = FindPort(port);
  ASSERT(data != NULL);
  // We might have already given up on this port and stopped it.
  if (data->complete())
    return;

  // SignalAddressError is currently sent from StunPort/TurnPort.
  // But this signal itself is generic.
  data->set_error();
  // Send candidate allocation complete signal if this was the last port.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::OnProtocolEnabled(AllocationSequence* seq,
                                                  ProtocolType proto) {
  std::vector<Candidate> candidates;
  for (std::vector<PortData>::iterator it = ports_.begin();
       it != ports_.end(); ++it) {
    if (it->sequence() != seq)
      continue;

    const std::vector<Candidate>& potentials = it->port()->Candidates();
    for (size_t i = 0; i < potentials.size(); ++i) {
      ProtocolType pvalue;
      if (!StringToProto(potentials[i].protocol().c_str(), &pvalue))
        continue;
      if (pvalue == proto) {
        candidates.push_back(potentials[i]);
      }
    }
  }

  if (!candidates.empty()) {
    SignalCandidatesReady(this, candidates);
  }
}

void BasicPortAllocatorSession::OnPortAllocationComplete(
    AllocationSequence* seq) {
  // Send candidate allocation complete signal if all ports are done.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::MaybeSignalCandidatesAllocationDone() {
  // Send signal only if all required AllocationSequence objects
  // are created.
  if (!allocation_sequences_created_)
    return;

  // Check that all port allocation sequences are complete.
  for (std::vector<AllocationSequence*>::iterator it = sequences_.begin();
       it != sequences_.end(); ++it) {
    if ((*it)->state() == AllocationSequence::kRunning)
      return;
  }

  // If all allocated ports are in complete state, session must have got all
  // expected candidates. Session will trigger candidates allocation complete
  // signal.
  for (std::vector<PortData>::iterator it = ports_.begin();
       it != ports_.end(); ++it) {
    if (!it->complete())
      return;
  }
  LOG(LS_INFO) << "All candidates gathered for " << content_name_ << ":"
               << component_ << ":" << generation();
  SignalCandidatesAllocationDone(this);
}

void BasicPortAllocatorSession::OnPortDestroyed(
    PortInterface* port) {
  ASSERT(talk_base::Thread::Current() == network_thread_);
  for (std::vector<PortData>::iterator iter = ports_.begin();
       iter != ports_.end(); ++iter) {
    if (port == iter->port()) {
      ports_.erase(iter);
      LOG_J(LS_INFO, port) << "Removed port from allocator ("
                           << static_cast<int>(ports_.size()) << " remaining)";
      return;
    }
  }
  ASSERT(false);
}

void BasicPortAllocatorSession::OnShake() {
  LOG(INFO) << ">>>>> SHAKE <<<<< >>>>> SHAKE <<<<< >>>>> SHAKE <<<<<";

  std::vector<Port*> ports;
  std::vector<Connection*> connections;

  for (size_t i = 0; i < ports_.size(); ++i) {
    if (ports_[i].ready())
      ports.push_back(ports_[i].port());
  }

  for (size_t i = 0; i < ports.size(); ++i) {
    Port::AddressMap::const_iterator iter;
    for (iter = ports[i]->connections().begin();
         iter != ports[i]->connections().end();
         ++iter) {
      connections.push_back(iter->second);
    }
  }

  LOG(INFO) << ">>>>> Destroying " << ports.size() << " ports and "
            << connections.size() << " connections";

  for (size_t i = 0; i < connections.size(); ++i)
    connections[i]->Destroy();

  if (running_ || (ports.size() > 0) || (connections.size() > 0))
    network_thread_->PostDelayed(ShakeDelay(), this, MSG_SHAKE);
}

BasicPortAllocatorSession::PortData* BasicPortAllocatorSession::FindPort(
    Port* port) {
  for (std::vector<PortData>::iterator it = ports_.begin();
       it != ports_.end(); ++it) {
    if (it->port() == port) {
      return &*it;
    }
  }
  return NULL;
}

// AllocationSequence

AllocationSequence::AllocationSequence(BasicPortAllocatorSession* session,
                                       talk_base::Network* network,
                                       PortConfiguration* config,
                                       uint32 flags)
    : session_(session),
      network_(network),
      ip_(network->ip()),
      config_(config),
      state_(kInit),
      flags_(flags),
      udp_socket_(NULL),
      phase_(0) {
}

bool AllocationSequence::Init() {
  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET) &&
      !IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_UFRAG)) {
    LOG(LS_ERROR) << "Shared socket option can't be set without "
                  << "shared ufrag.";
    ASSERT(false);
    return false;
  }

  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
    udp_socket_.reset(session_->socket_factory()->CreateUdpSocket(
        talk_base::SocketAddress(ip_, 0), session_->allocator()->min_port(),
        session_->allocator()->max_port()));
    if (udp_socket_) {
      udp_socket_->SignalReadPacket.connect(
          this, &AllocationSequence::OnReadPacket);
    }
    // Continuing if |udp_socket_| is NULL, as local TCP and RelayPort using TCP
    // are next available options to setup a communication channel.
  }
  return true;
}

AllocationSequence::~AllocationSequence() {
  session_->network_thread()->Clear(this);
}

void AllocationSequence::DisableEquivalentPhases(talk_base::Network* network,
    PortConfiguration* config, uint32* flags) {
  if (!((network == network_) && (ip_ == network->ip()))) {
    // Different network setup; nothing is equivalent.
    return;
  }

  // Else turn off the stuff that we've already got covered.

  // Every config implicitly specifies local, so turn that off right away.
  *flags |= PORTALLOCATOR_DISABLE_UDP;
  *flags |= PORTALLOCATOR_DISABLE_TCP;

  if (config_ && config) {
    if (config_->stun_address == config->stun_address) {
      // Already got this STUN server covered.
      *flags |= PORTALLOCATOR_DISABLE_STUN;
    }
    if (!config_->relays.empty()) {
      // Already got relays covered.
      // NOTE: This will even skip a _different_ set of relay servers if we
      // were to be given one, but that never happens in our codebase. Should
      // probably get rid of the list in PortConfiguration and just keep a
      // single relay server in each one.
      *flags |= PORTALLOCATOR_DISABLE_RELAY;
    }
  }
}

void AllocationSequence::Start() {
  state_ = kRunning;
  session_->network_thread()->Post(this, MSG_ALLOCATION_PHASE);
}

void AllocationSequence::Stop() {
  // If the port is completed, don't set it to stopped.
  if (state_ == kRunning) {
    state_ = kStopped;
    session_->network_thread()->Clear(this, MSG_ALLOCATION_PHASE);
  }
}

void AllocationSequence::OnMessage(talk_base::Message* msg) {
  ASSERT(talk_base::Thread::Current() == session_->network_thread());
  ASSERT(msg->message_id == MSG_ALLOCATION_PHASE);

  const char* const PHASE_NAMES[kNumPhases] = {
    "Udp", "Relay", "Tcp", "SslTcp"
  };

  // Perform all of the phases in the current step.
  LOG_J(LS_INFO, network_) << "Allocation Phase="
                           << PHASE_NAMES[phase_];

  switch (phase_) {
    case PHASE_UDP:
      CreateUDPPorts();
      CreateStunPorts();
      EnableProtocol(PROTO_UDP);
      break;

    case PHASE_RELAY:
      CreateRelayPorts();
      break;

    case PHASE_TCP:
      CreateTCPPorts();
      EnableProtocol(PROTO_TCP);
      break;

    case PHASE_SSLTCP:
      state_ = kCompleted;
      EnableProtocol(PROTO_SSLTCP);
      break;

    default:
      ASSERT(false);
  }

  if (state() == kRunning) {
    ++phase_;
    session_->network_thread()->PostDelayed(
        session_->allocator()->step_delay(),
        this, MSG_ALLOCATION_PHASE);
  } else {
    // If all phases in AllocationSequence are completed, no allocation
    // steps needed further. Canceling  pending signal.
    session_->network_thread()->Clear(this, MSG_ALLOCATION_PHASE);
    SignalPortAllocationComplete(this);
  }
}

void AllocationSequence::EnableProtocol(ProtocolType proto) {
  if (!ProtocolEnabled(proto)) {
    protocols_.push_back(proto);
    session_->OnProtocolEnabled(this, proto);
  }
}

bool AllocationSequence::ProtocolEnabled(ProtocolType proto) const {
  for (ProtocolList::const_iterator it = protocols_.begin();
       it != protocols_.end(); ++it) {
    if (*it == proto)
      return true;
  }
  return false;
}

void AllocationSequence::CreateUDPPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_UDP)) {
    LOG(LS_VERBOSE) << "AllocationSequence: UDP ports disabled, skipping.";
    return;
  }

  // TODO(mallinath) - Remove UDPPort creating socket after shared socket
  // is enabled completely.
  UDPPort* port = NULL;
  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET) && udp_socket_) {
    port = UDPPort::Create(session_->network_thread(), network_,
                           udp_socket_.get(),
                           session_->username(), session_->password());
  } else {
    port = UDPPort::Create(session_->network_thread(),
                           session_->socket_factory(),
                           network_, ip_,
                           session_->allocator()->min_port(),
                           session_->allocator()->max_port(),
                           session_->username(), session_->password());
  }

  if (port) {
    ports.push_back(port);
    // If shared socket is enabled, STUN candidate will be allocated by the
    // UDPPort.
    if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET) &&
        !IsFlagSet(PORTALLOCATOR_DISABLE_STUN)) {
      ASSERT(config_ && !config_->stun_address.IsNil());
      if (!(config_ && !config_->stun_address.IsNil())) {
        LOG(LS_WARNING)
            << "AllocationSequence: No STUN server configured, skipping.";
        return;
      }
      port->set_server_addr(config_->stun_address);
    }

    session_->AddAllocatedPort(port, this, true);
    port->SignalDestroyed.connect(this, &AllocationSequence::OnPortDestroyed);
  }
}

void AllocationSequence::CreateTCPPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_TCP)) {
    LOG(LS_VERBOSE) << "AllocationSequence: TCP ports disabled, skipping.";
    return;
  }

  Port* port = TCPPort::Create(session_->network_thread(),
                               session_->socket_factory(),
                               network_, ip_,
                               session_->allocator()->min_port(),
                               session_->allocator()->max_port(),
                               session_->username(), session_->password(),
                               session_->allocator()->allow_tcp_listen());
  if (port) {
    session_->AddAllocatedPort(port, this, true);
    // Since TCPPort is not created using shared socket, |port| will not be
    // added to the dequeue.
  }
}

void AllocationSequence::CreateStunPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_STUN)) {
    LOG(LS_VERBOSE) << "AllocationSequence: STUN ports disabled, skipping.";
    return;
  }

  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
    LOG(LS_INFO) << "AllocationSequence: "
                 << "UDPPort will be handling the STUN candidate generation.";
    return;
  }

  // If BasicPortAllocatorSession::OnAllocate left STUN ports enabled then we
  // ought to have an address for them here.
  ASSERT(config_ && !config_->stun_address.IsNil());
  if (!(config_ && !config_->stun_address.IsNil())) {
    LOG(LS_WARNING)
        << "AllocationSequence: No STUN server configured, skipping.";
    return;
  }

  StunPort* port = StunPort::Create(session_->network_thread(),
                                session_->socket_factory(),
                                network_, ip_,
                                session_->allocator()->min_port(),
                                session_->allocator()->max_port(),
                                session_->username(), session_->password(),
                                config_->stun_address);
  if (port) {
    session_->AddAllocatedPort(port, this, true);
    // Since StunPort is not created using shared socket, |port| will not be
    // added to the dequeue.
  }
}

void AllocationSequence::CreateRelayPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_RELAY)) {
     LOG(LS_VERBOSE) << "AllocationSequence: Relay ports disabled, skipping.";
     return;
  }

  // If BasicPortAllocatorSession::OnAllocate left relay ports enabled then we
  // ought to have a relay list for them here.
  ASSERT(config_ && !config_->relays.empty());
  if (!(config_ && !config_->relays.empty())) {
    LOG(LS_WARNING)
        << "AllocationSequence: No relay server configured, skipping.";
    return;
  }

  PortConfiguration::RelayList::const_iterator relay;
  for (relay = config_->relays.begin();
       relay != config_->relays.end(); ++relay) {
    if (relay->type == RELAY_GTURN) {
      CreateGturnPort(*relay);
    } else if (relay->type == RELAY_TURN) {
      CreateTurnPort(*relay);
    } else {
      ASSERT(false);
    }
  }
}

void AllocationSequence::CreateGturnPort(const RelayServerConfig& config) {
  // TODO(mallinath) - Rename RelayPort to GTurnPort.
  RelayPort* port = RelayPort::Create(session_->network_thread(),
                                      session_->socket_factory(),
                                      network_, ip_,
                                      session_->allocator()->min_port(),
                                      session_->allocator()->max_port(),
                                      config_->username, config_->password);
  if (port) {
    // Since RelayPort is not created using shared socket, |port| will not be
    // added to the dequeue.
    // Note: We must add the allocated port before we add addresses because
    //       the latter will create candidates that need name and preference
    //       settings.  However, we also can't prepare the address (normally
    //       done by AddAllocatedPort) until we have these addresses.  So we
    //       wait to do that until below.
    session_->AddAllocatedPort(port, this, false);

    // Add the addresses of this protocol.
    PortList::const_iterator relay_port;
    for (relay_port = config.ports.begin();
         relay_port != config.ports.end();
         ++relay_port) {
      port->AddServerAddress(*relay_port);
      port->AddExternalAddress(*relay_port);
    }
    // Start fetching an address for this port.
    port->PrepareAddress();
  }
}

void AllocationSequence::CreateTurnPort(const RelayServerConfig& config) {
  PortList::const_iterator relay_port;
  for (relay_port = config.ports.begin();
       relay_port != config.ports.end(); ++relay_port) {
    TurnPort* port = TurnPort::Create(session_->network_thread(),
                                      session_->socket_factory(),
                                      network_, ip_,
                                      session_->allocator()->min_port(),
                                      session_->allocator()->max_port(),
                                      session_->username(),
                                      session_->password(),
                                      *relay_port, config.credentials);
    if (port) {
      session_->AddAllocatedPort(port, this, true);
    }
  }
}

void AllocationSequence::OnReadPacket(
    talk_base::AsyncPacketSocket* socket, const char* data, size_t size,
    const talk_base::SocketAddress& remote_addr) {
  ASSERT(socket == udp_socket_.get());
  for (std::deque<Port*>::iterator iter = ports.begin();
       iter != ports.end(); ++iter) {
    // We have only one port in the queue.
    // TODO(mallinath) - Add shared socket support to Relay and Turn ports.
    if ((*iter)->HandleIncomingPacket(socket, data, size, remote_addr)) {
      break;
    }
  }
}

void AllocationSequence::OnPortDestroyed(PortInterface* port) {
  std::deque<Port*>::iterator iter =
      std::find(ports.begin(), ports.end(), port);
  ASSERT(iter != ports.end());
  ports.erase(iter);
}

// PortConfiguration
PortConfiguration::PortConfiguration(
    const talk_base::SocketAddress& stun_address,
    const std::string& username,
    const std::string& password)
    : stun_address(stun_address),
      username(username),
      password(password) {
}

void PortConfiguration::AddRelay(const RelayServerConfig& config) {
  relays.push_back(config);
}

bool PortConfiguration::SupportsProtocol(
    const RelayServerConfig& relay, ProtocolType type) {
  PortList::const_iterator relay_port;
  for (relay_port = relay.ports.begin();
        relay_port != relay.ports.end();
        ++relay_port) {
    if (relay_port->proto == type)
      return true;
  }
  return false;
}

}  // namespace cricket
