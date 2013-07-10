// Copyright 2011 Google Inc. All Rights Reserved.


#include <string>

#include "talk/p2p/client/connectivitychecker.h"

#include "talk/base/asynchttprequest.h"
#include "talk/base/autodetectproxy.h"
#include "talk/base/helpers.h"
#include "talk/base/httpcommon.h"
#include "talk/base/httpcommon-inl.h"
#include "talk/base/logging.h"
#include "talk/base/proxydetect.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/candidate.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/common.h"
#include "talk/p2p/base/port.h"
#include "talk/p2p/base/relayport.h"
#include "talk/p2p/base/stunport.h"

namespace cricket {

static const char kSessionTypeVideo[] =
    "http://www.google.com/session/video";
static const char kSessionNameRtp[] = "rtp";

static const char kDefaultStunHostname[] = "stun.l.google.com";
static const int kDefaultStunPort = 19302;

// Default maximum time in milliseconds we will wait for connections.
static const uint32 kDefaultTimeoutMs = 3000;

enum {
  MSG_START = 1,
  MSG_STOP = 2,
  MSG_TIMEOUT = 3,
  MSG_SIGNAL_RESULTS = 4
};

class TestHttpPortAllocator : public HttpPortAllocator {
 public:
  TestHttpPortAllocator(talk_base::NetworkManager* network_manager,
                        const std::string& user_agent,
                        const std::string& relay_token) :
      HttpPortAllocator(network_manager, user_agent) {
    SetRelayToken(relay_token);
  }
  PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) {
    return new TestHttpPortAllocatorSession(this, content_name, component,
                                            ice_ufrag, ice_pwd,
                                            stun_hosts(), relay_hosts(),
                                            relay_token(), user_agent());
  }
};

void TestHttpPortAllocatorSession::ConfigReady(PortConfiguration* config) {
  SignalConfigReady(username(), password(), config, proxy_);
}

void TestHttpPortAllocatorSession::OnRequestDone(
    talk_base::SignalThread* data) {
  talk_base::AsyncHttpRequest* request =
      static_cast<talk_base::AsyncHttpRequest*>(data);

  // Tell the checker that the request is complete.
  SignalRequestDone(request);

  // Pass on the response to super class.
  HttpPortAllocatorSession::OnRequestDone(data);
}

ConnectivityChecker::ConnectivityChecker(
    talk_base::Thread* worker,
    const std::string& jid,
    const std::string& session_id,
    const std::string& user_agent,
    const std::string& relay_token,
    const std::string& connection)
    : worker_(worker),
      jid_(jid),
      session_id_(session_id),
      user_agent_(user_agent),
      relay_token_(relay_token),
      connection_(connection),
      proxy_detect_(NULL),
      timeout_ms_(kDefaultTimeoutMs),
      stun_address_(kDefaultStunHostname, kDefaultStunPort),
      started_(false) {
}

ConnectivityChecker::~ConnectivityChecker() {
  if (started_) {
    // We try to clear the TIMEOUT below. But worker may still handle it and
    // cause SignalCheckDone to happen on main-thread. So we finally clear any
    // pending SIGNAL_RESULTS.
    worker_->Clear(this, MSG_TIMEOUT);
    worker_->Send(this, MSG_STOP);
    nics_.clear();
    main_->Clear(this, MSG_SIGNAL_RESULTS);
  }
}

bool ConnectivityChecker::Initialize() {
  network_manager_.reset(CreateNetworkManager());
  socket_factory_.reset(CreateSocketFactory(worker_));
  port_allocator_.reset(CreatePortAllocator(network_manager_.get(),
                                            user_agent_, relay_token_));
  uint32 new_allocator_flags = port_allocator_->flags();
  new_allocator_flags |= cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG;
  port_allocator_->set_flags(new_allocator_flags);
  return true;
}

void ConnectivityChecker::Start() {
  main_ = talk_base::Thread::Current();
  worker_->Post(this, MSG_START);
  started_ = true;
}

void ConnectivityChecker::CleanUp() {
  ASSERT(worker_ == talk_base::Thread::Current());
  if (proxy_detect_) {
    proxy_detect_->Release();
    proxy_detect_ = NULL;
  }

  for (uint32 i = 0; i < sessions_.size(); ++i) {
    delete sessions_[i];
  }
  sessions_.clear();
  for (uint32 i = 0; i < ports_.size(); ++i) {
    delete ports_[i];
  }
  ports_.clear();
}

bool ConnectivityChecker::AddNic(const talk_base::IPAddress& ip,
                                 const talk_base::SocketAddress& proxy_addr) {
  NicMap::iterator i = nics_.find(NicId(ip, proxy_addr));
  if (i != nics_.end()) {
    // Already have it.
    return false;
  }
  uint32 now = talk_base::Time();
  NicInfo info;
  info.ip = ip;
  info.proxy_info = GetProxyInfo();
  info.stun.start_time_ms = now;
  nics_.insert(std::pair<NicId, NicInfo>(NicId(ip, proxy_addr), info));
  return true;
}

void ConnectivityChecker::SetProxyInfo(const talk_base::ProxyInfo& proxy_info) {
  port_allocator_->set_proxy(user_agent_, proxy_info);
  AllocatePorts();
}

talk_base::ProxyInfo ConnectivityChecker::GetProxyInfo() const {
  talk_base::ProxyInfo proxy_info;
  if (proxy_detect_) {
    proxy_info = proxy_detect_->proxy();
  }
  return proxy_info;
}

void ConnectivityChecker::CheckNetworks() {
  network_manager_->SignalNetworksChanged.connect(
      this, &ConnectivityChecker::OnNetworksChanged);
  network_manager_->StartUpdating();
}

void ConnectivityChecker::OnMessage(talk_base::Message *msg) {
  switch (msg->message_id) {
    case MSG_START:
      ASSERT(worker_ == talk_base::Thread::Current());
      worker_->PostDelayed(timeout_ms_, this, MSG_TIMEOUT);
      CheckNetworks();
      break;
    case MSG_STOP:
      // We're being stopped, free resources.
      CleanUp();
      break;
    case MSG_TIMEOUT:
      // We need to signal results on the main thread.
      main_->Post(this, MSG_SIGNAL_RESULTS);
      break;
    case MSG_SIGNAL_RESULTS:
      ASSERT(main_ == talk_base::Thread::Current());
      SignalCheckDone(this);
      break;
    default:
      LOG(LS_ERROR) << "Unknown message: " << msg->message_id;
  }
}

void ConnectivityChecker::OnProxyDetect(talk_base::SignalThread* thread) {
  ASSERT(worker_ == talk_base::Thread::Current());
  if (proxy_detect_->proxy().type != talk_base::PROXY_NONE) {
    SetProxyInfo(proxy_detect_->proxy());
  }
}

void ConnectivityChecker::OnRequestDone(talk_base::AsyncHttpRequest* request) {
  ASSERT(worker_ == talk_base::Thread::Current());
  // Since we don't know what nic were actually used for the http request,
  // for now, just use the first one.
  std::vector<talk_base::Network*> networks;
  network_manager_->GetNetworks(&networks);
  if (networks.empty()) {
    LOG(LS_ERROR) << "No networks while registering http start.";
    return;
  }
  talk_base::ProxyInfo proxy_info = request->proxy();
  NicMap::iterator i = nics_.find(NicId(networks[0]->ip(), proxy_info.address));
  if (i != nics_.end()) {
    int port = request->port();
    uint32 now = talk_base::Time();
    NicInfo* nic_info = &i->second;
    if (port == talk_base::HTTP_DEFAULT_PORT) {
      nic_info->http.rtt = now - nic_info->http.start_time_ms;
    } else if (port == talk_base::HTTP_SECURE_PORT) {
      nic_info->https.rtt = now - nic_info->https.start_time_ms;
    } else {
      LOG(LS_ERROR) << "Got response with unknown port: " << port;
    }
  } else {
    LOG(LS_ERROR) << "No nic info found while receiving response.";
  }
}

void ConnectivityChecker::OnConfigReady(
    const std::string& username, const std::string& password,
    const PortConfiguration* config, const talk_base::ProxyInfo& proxy_info) {
  ASSERT(worker_ == talk_base::Thread::Current());

  // Since we send requests on both HTTP and HTTPS we will get two
  // configs per nic. Results from the second will overwrite the
  // result from the first.
  // TODO: Handle multiple pings on one nic.
  CreateRelayPorts(username, password, config, proxy_info);
}

void ConnectivityChecker::OnRelayPortComplete(Port* port) {
  ASSERT(worker_ == talk_base::Thread::Current());
  RelayPort* relay_port = reinterpret_cast<RelayPort*>(port);
  const ProtocolAddress* address = relay_port->ServerAddress(0);
  talk_base::IPAddress ip = port->Network()->ip();
  NicMap::iterator i = nics_.find(NicId(ip, port->proxy().address));
  if (i != nics_.end()) {
    // We have it already, add the new information.
    NicInfo* nic_info = &i->second;
    ConnectInfo* connect_info = NULL;
    if (address) {
      switch (address->proto) {
        case PROTO_UDP:
          connect_info = &nic_info->udp;
          break;
        case PROTO_TCP:
          connect_info = &nic_info->tcp;
          break;
        case PROTO_SSLTCP:
          connect_info = &nic_info->ssltcp;
          break;
        default:
          LOG(LS_ERROR) << " relay address with bad protocol added";
      }
      if (connect_info) {
        connect_info->rtt =
            talk_base::TimeSince(connect_info->start_time_ms);
      }
    }
  } else {
    LOG(LS_ERROR) << " got relay address for non-existing nic";
  }
}

void ConnectivityChecker::OnStunPortComplete(Port* port) {
  ASSERT(worker_ == talk_base::Thread::Current());
  const std::vector<Candidate> candidates = port->Candidates();
  Candidate c = candidates[0];
  talk_base::IPAddress ip = port->Network()->ip();
  NicMap::iterator i = nics_.find(NicId(ip, port->proxy().address));
  if (i != nics_.end()) {
    // We have it already, add the new information.
    uint32 now = talk_base::Time();
    NicInfo* nic_info = &i->second;
    nic_info->external_address = c.address();
    nic_info->stun_server_address = static_cast<StunPort*>(port)->server_addr();
    nic_info->stun.rtt = now - nic_info->stun.start_time_ms;
  } else {
    LOG(LS_ERROR) << "Got stun address for non-existing nic";
  }
}

void ConnectivityChecker::OnStunPortError(Port* port) {
  ASSERT(worker_ == talk_base::Thread::Current());
  LOG(LS_ERROR) << "Stun address error.";
  talk_base::IPAddress ip = port->Network()->ip();
  NicMap::iterator i = nics_.find(NicId(ip, port->proxy().address));
  if (i != nics_.end()) {
    // We have it already, add the new information.
    NicInfo* nic_info = &i->second;
    nic_info->stun_server_address = static_cast<StunPort*>(port)->server_addr();
  }
}

void ConnectivityChecker::OnRelayPortError(Port* port) {
  ASSERT(worker_ == talk_base::Thread::Current());
  LOG(LS_ERROR) << "Relay address error.";
}

void ConnectivityChecker::OnNetworksChanged() {
  ASSERT(worker_ == talk_base::Thread::Current());
  std::vector<talk_base::Network*> networks;
  network_manager_->GetNetworks(&networks);
  if (networks.empty()) {
    LOG(LS_ERROR) << "Machine has no networks; nothing to do";
    return;
  }
  AllocatePorts();
}

HttpPortAllocator* ConnectivityChecker::CreatePortAllocator(
    talk_base::NetworkManager* network_manager,
    const std::string& user_agent,
    const std::string& relay_token) {
  return new TestHttpPortAllocator(network_manager, user_agent, relay_token);
}

StunPort* ConnectivityChecker::CreateStunPort(
    const std::string& username, const std::string& password,
    const PortConfiguration* config, talk_base::Network* network) {
  return StunPort::Create(worker_, socket_factory_.get(),
                          network, network->ip(), 0, 0,
                          username, password, config->stun_address);
}

RelayPort* ConnectivityChecker::CreateRelayPort(
    const std::string& username, const std::string& password,
    const PortConfiguration* config, talk_base::Network* network) {
  return RelayPort::Create(worker_, socket_factory_.get(),
                           network, network->ip(),
                           port_allocator_->min_port(),
                           port_allocator_->max_port(),
                           username, password);
}

void ConnectivityChecker::CreateRelayPorts(
    const std::string& username, const std::string& password,
    const PortConfiguration* config, const talk_base::ProxyInfo& proxy_info) {
  PortConfiguration::RelayList::const_iterator relay;
  std::vector<talk_base::Network*> networks;
  network_manager_->GetNetworks(&networks);
  if (networks.empty()) {
    LOG(LS_ERROR) << "Machine has no networks; no relay ports created.";
    return;
  }
  for (relay = config->relays.begin();
       relay != config->relays.end(); ++relay) {
    for (uint32 i = 0; i < networks.size(); ++i) {
      NicMap::iterator iter = nics_.find(NicId(networks[i]->ip(),
                                               proxy_info.address));
      if (iter != nics_.end()) {
        // TODO: Now setting the same start time for all protocols.
        // This might affect accuracy, but since we are mainly looking for
        // connect failures or number that stick out, this is good enough.
        uint32 now = talk_base::Time();
        NicInfo* nic_info = &iter->second;
        nic_info->udp.start_time_ms = now;
        nic_info->tcp.start_time_ms = now;
        nic_info->ssltcp.start_time_ms = now;

        // Add the addresses of this protocol.
        PortList::const_iterator relay_port;
        for (relay_port = relay->ports.begin();
             relay_port != relay->ports.end();
             ++relay_port) {
          RelayPort* port = CreateRelayPort(username, password,
                                            config, networks[i]);
          port->AddServerAddress(*relay_port);
          port->AddExternalAddress(*relay_port);

          nic_info->media_server_address = port->ServerAddress(0)->address;

          // Listen to network events.
          port->SignalPortComplete.connect(
              this, &ConnectivityChecker::OnRelayPortComplete);
          port->SignalPortError.connect(
              this, &ConnectivityChecker::OnRelayPortError);

          port->set_proxy(user_agent_, proxy_info);

          // Start fetching an address for this port.
          port->PrepareAddress();
          ports_.push_back(port);
        }
      } else {
        LOG(LS_ERROR) << "Failed to find nic info when creating relay ports.";
      }
    }
  }
}

void ConnectivityChecker::AllocatePorts() {
  const std::string username = talk_base::CreateRandomString(ICE_UFRAG_LENGTH);
  const std::string password = talk_base::CreateRandomString(ICE_PWD_LENGTH);
  PortConfiguration config(stun_address_, username, password);
  std::vector<talk_base::Network*> networks;
  network_manager_->GetNetworks(&networks);
  if (networks.empty()) {
    LOG(LS_ERROR) << "Machine has no networks; no ports will be allocated";
    return;
  }
  talk_base::ProxyInfo proxy_info = GetProxyInfo();
  bool allocate_relay_ports = false;
  for (uint32 i = 0; i < networks.size(); ++i) {
    if (AddNic(networks[i]->ip(), proxy_info.address)) {
      Port* port = CreateStunPort(username, password, &config, networks[i]);
      if (port) {

        // Listen to network events.
        port->SignalPortComplete.connect(
            this, &ConnectivityChecker::OnStunPortComplete);
        port->SignalPortError.connect(
            this, &ConnectivityChecker::OnStunPortError);

        port->set_proxy(user_agent_, proxy_info);
        port->PrepareAddress();
        ports_.push_back(port);
        allocate_relay_ports = true;
      }
    }
  }

  // If any new ip/proxy combinations were added, send a relay allocate.
  if (allocate_relay_ports) {
    AllocateRelayPorts();
  }

  // Initiate proxy detection.
  InitiateProxyDetection();
}

void ConnectivityChecker::InitiateProxyDetection() {
  // Only start if we haven't been started before.
  if (!proxy_detect_) {
    proxy_detect_ = new talk_base::AutoDetectProxy(user_agent_);
    talk_base::Url<char> host_url("/", "relay.google.com",
                                  talk_base::HTTP_DEFAULT_PORT);
    host_url.set_secure(true);
    proxy_detect_->set_server_url(host_url.url());
    proxy_detect_->SignalWorkDone.connect(
        this, &ConnectivityChecker::OnProxyDetect);
    proxy_detect_->Start();
  }
}

void ConnectivityChecker::AllocateRelayPorts() {
  // Currently we are using the 'default' nic for http(s) requests.
  TestHttpPortAllocatorSession* allocator_session =
      reinterpret_cast<TestHttpPortAllocatorSession*>(
          port_allocator_->CreateSessionInternal(
              "connectivity checker test content",
              ICE_CANDIDATE_COMPONENT_RTP,
              talk_base::CreateRandomString(ICE_UFRAG_LENGTH),
              talk_base::CreateRandomString(ICE_PWD_LENGTH)));
  allocator_session->set_proxy(port_allocator_->proxy());
  allocator_session->SignalConfigReady.connect(
      this, &ConnectivityChecker::OnConfigReady);
  allocator_session->SignalRequestDone.connect(
      this, &ConnectivityChecker::OnRequestDone);

  // Try both http and https.
  RegisterHttpStart(talk_base::HTTP_SECURE_PORT);
  allocator_session->SendSessionRequest("relay.l.google.com",
                                        talk_base::HTTP_SECURE_PORT);
  RegisterHttpStart(talk_base::HTTP_DEFAULT_PORT);
  allocator_session->SendSessionRequest("relay.l.google.com",
                                        talk_base::HTTP_DEFAULT_PORT);

  sessions_.push_back(allocator_session);
}

void ConnectivityChecker::RegisterHttpStart(int port) {
  // Since we don't know what nic were actually used for the http request,
  // for now, just use the first one.
  std::vector<talk_base::Network*> networks;
  network_manager_->GetNetworks(&networks);
  if (networks.empty()) {
    LOG(LS_ERROR) << "No networks while registering http start.";
    return;
  }
  talk_base::ProxyInfo proxy_info = GetProxyInfo();
  NicMap::iterator i = nics_.find(NicId(networks[0]->ip(), proxy_info.address));
  if (i != nics_.end()) {
    uint32 now = talk_base::Time();
    NicInfo* nic_info = &i->second;
    if (port == talk_base::HTTP_DEFAULT_PORT) {
      nic_info->http.start_time_ms = now;
    } else if (port == talk_base::HTTP_SECURE_PORT) {
      nic_info->https.start_time_ms = now;
    } else {
      LOG(LS_ERROR) << "Registering start time for unknown port: " << port;
    }
  } else {
    LOG(LS_ERROR) << "Error, no nic info found while registering http start.";
  }
}

}  // namespace talk_base
