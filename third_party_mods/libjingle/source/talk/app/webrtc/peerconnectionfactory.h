#ifndef TALK_APP_WEBRTC_PEERCONNECTIONFACTORY_H_
#define TALK_APP_WEBRTC_PEERCONNECTIONFACTORY_H_

#include <string>
#include <vector>

#include "talk/base/scoped_ptr.h"

namespace cricket {
class ChannelManager;
class DeviceManager;
class MediaEngine;
class PortAllocator;
}  // namespace cricket

namespace talk_base {
class SocketAddress;
class Thread;
}  // namespace talk_base

namespace webrtc {

class PeerConnection;

class PeerConnectionFactory {
 public:
  // NOTE: The order of the enum values must be in sync with the array
  // in Initialize().
  enum ServiceType {
    STUN = 0,
    STUNS,
    TURN,
    TURNS,
    SERVICE_COUNT,
    INVALID
  };

  PeerConnectionFactory(const std::string& config,
                        cricket::PortAllocator* port_allocator,
                        cricket::MediaEngine* media_engine,
                        cricket::DeviceManager* device_manager,
                        talk_base::Thread* worker_thread);
  PeerConnectionFactory(const std::string& config,
                        cricket::PortAllocator* port_allocator,
                        talk_base::Thread* worker_thread);

  virtual ~PeerConnectionFactory();
  bool Initialize();

  PeerConnection* CreatePeerConnection(talk_base::Thread* signaling_thread);

 private:
  bool ParseConfigString(const std::string&, talk_base::SocketAddress*);
  ServiceType service_type_;
  std::string config_;
  bool initialized_;
  talk_base::scoped_ptr<cricket::PortAllocator> port_allocator_;
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTIONFACTORY_H_

