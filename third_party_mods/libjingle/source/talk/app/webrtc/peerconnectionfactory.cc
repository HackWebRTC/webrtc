#include "talk/app/webrtc/peerconnectionfactory.h"

#include "talk/app/webrtc/peerconnection_proxy.h"
#include "talk/base/logging.h"
#include "talk/p2p/client/basicportallocator.h"
#include "talk/session/phone/channelmanager.h"

namespace {
// The number of the tokens in the config string.
static const size_t kConfigTokens = 2;
// The default stun port.
static const int kDefaultStunPort = 3478;
// NOTE: Must be in the same order as the enum.
static const char* kValidServiceTypes[
    webrtc::PeerConnectionFactory::SERVICE_COUNT] = {
        "STUN", "STUNS", "TURN", "TURNS" };
}

namespace webrtc {

PeerConnectionFactory::PeerConnectionFactory(
    const std::string& config,
    cricket::PortAllocator* port_allocator,
    cricket::MediaEngine* media_engine,
    cricket::DeviceManager* device_manager,
    talk_base::Thread* worker_thread)
  : config_(config),
    initialized_(false),
    port_allocator_(port_allocator),
    channel_manager_(new cricket::ChannelManager(
        media_engine, device_manager, worker_thread)) {
}

PeerConnectionFactory::PeerConnectionFactory(
    const std::string& config,
    cricket::PortAllocator* port_allocator,
    talk_base::Thread* worker_thread)
  : config_(config),
    initialized_(false),
    port_allocator_(port_allocator),
    channel_manager_(new cricket::ChannelManager(worker_thread)) {
}

PeerConnectionFactory::~PeerConnectionFactory() {
}

bool PeerConnectionFactory::Initialize() {
  ASSERT(channel_manager_.get());
  std::vector<talk_base::SocketAddress> stun_hosts;
  talk_base::SocketAddress stun_addr;
  if (!ParseConfigString(config_, &stun_addr))
    return false;
  stun_hosts.push_back(stun_addr);

  initialized_ = channel_manager_->Init();
  return initialized_;
}

PeerConnection* PeerConnectionFactory::CreatePeerConnection(
    talk_base::Thread* signaling_thread) {
  PeerConnectionProxy* pc = NULL;
  if (initialized_) {
    pc =  new PeerConnectionProxy(
        port_allocator_.get(), channel_manager_.get(), signaling_thread);
    if (!pc->Init()) {
      LOG(LERROR) << "Error in initializing PeerConnection";
      delete pc;
      pc = NULL;
    }
  } else {
    LOG(LERROR) << "PeerConnectionFactory is not initialize";
  }
  return pc;
}

bool PeerConnectionFactory::ParseConfigString(
    const std::string& config, talk_base::SocketAddress* stun_addr) {
  std::vector<std::string> tokens;
  talk_base::tokenize(config_, ' ', &tokens);

  if (tokens.size() != kConfigTokens) {
    LOG(WARNING) << "Invalid config string";
    return false;
  }

  service_type_ = INVALID;

  const std::string& type = tokens[0];
  for (size_t i = 0; i < SERVICE_COUNT; ++i) {
    if (type.compare(kValidServiceTypes[i]) == 0) {
      service_type_ = static_cast<ServiceType>(i);
      break;
    }
  }

  if (service_type_ == SERVICE_COUNT) {
    LOG(WARNING) << "Invalid service type: " << type;
    return false;
  }
  std::string service_address = tokens[1];

  int port;
  tokens.clear();
  talk_base::tokenize(service_address, ':', &tokens);
  if (tokens.size() != kConfigTokens) {
    port = kDefaultStunPort;
  } else {
    port = atoi(tokens[1].c_str());
    if (port <= 0 || port > 0xffff) {
      LOG(WARNING) << "Invalid port: " << tokens[1];
      return false;
    }
  }
  stun_addr->SetIP(service_address);
  stun_addr->SetPort(port);
  return true;
}

}  // namespace webrtc
