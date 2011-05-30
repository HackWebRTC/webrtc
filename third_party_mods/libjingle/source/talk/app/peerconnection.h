// Copyright 2011 Google Inc. All Rights Reserved.
// Author: mallinath@google.com (Mallinath Bareddy)


#ifndef TALK_APP_WEBRTC_PEERCONNECTION_H_
#define TALK_APP_WEBRTC_PEERCONNECTION_H_

#include <string>
#include "talk/base/sigslot.h"
#include "talk/base/thread.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/basicpacketsocketfactory.h"
#include "talk/app/webrtcchannelmanager.h"

namespace Json {
class Value;
}

namespace cricket {
class BasicPortAllocator;
}

#ifdef PLATFORM_CHROMIUM
class P2PSocketDispatcher;
#endif  // PLATFORM_CHROMIUM

namespace webrtc {

class AudioDeviceModule;
class ExternalRenderer;
class WebRTCSessionImpl;

class PeerConnectionObserver {
 public:
  virtual void OnError() = 0;
  // serialized signaling message
  virtual void OnSignalingMessage(const std::string& msg) = 0;

  // Triggered when a remote peer accepts a media connection.
  virtual void OnAddStream(const std::string& stream_id,
                           int channel_id,
                           bool video) = 0;

  // Triggered when a remote peer closes a media stream.
  virtual void OnRemoveStream(const std::string& stream_id,
                              int channel_id,
                              bool video) = 0;

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~PeerConnectionObserver() {}
};

class PeerConnection : public sigslot::has_slots<> {
 public:

#ifdef PLATFORM_CHROMIUM
  PeerConnection(const std::string& config,
                 P2PSocketDispatcher* p2p_socket_dispatcher);
#else
  explicit PeerConnection(const std::string& config);
#endif  // PLATFORM_CHROMIUM

  ~PeerConnection();

  bool Init();
  void RegisterObserver(PeerConnectionObserver* observer);
  bool SignalingMessage(const std::string& msg);
  bool AddStream(const std::string& stream_id, bool video);
  bool RemoveStream(const std::string& stream_id);
  bool Connect();
  void Close();

  // TODO(ronghuawu): This section will be modified to reuse the existing libjingle APIs.
  // Set Audio device
  bool SetAudioDevice(const std::string& wave_in_device,
                      const std::string& wave_out_device, int opts);
  // Set the video renderer
  bool SetVideoRenderer(const std::string& stream_id,
                        ExternalRenderer* external_renderer);
  // Set channel_id to -1 for the local preview
  bool SetVideoRenderer(int channel_id,
                        void* window,
                        unsigned int zOrder,
                        float left,
                        float top,
                        float right,
                        float bottom);
  // Set video capture device
  // For Chromium the cam_device should use the capture session id.
  // For standalone app, cam_device is the camera name. It will try to
  // set the default capture device when cam_device is "".
  bool SetVideoCapture(const std::string& cam_device);

  // Access to the members
  const std::string& config() const { return config_; }
  bool incoming() const { return incoming_; }
  talk_base::Thread* media_thread() {
    return media_thread_.get();
  }
#ifdef PLATFORM_CHROMIUM
  P2PSocketDispatcher* p2p_socket_dispatcher() {
    return p2p_socket_dispatcher_;
  }
#endif  // PLATFORM_CHROMIUM

  // Callbacks
  void OnAddStream(const std::string& stream_id, int channel_id, bool video);
  void OnRemoveStream(const std::string& stream_id, int channel_id,
                      bool video);
  void OnLocalDescription(cricket::SessionDescription* desc,
                          const std::vector<cricket::Candidate>& candidates);
  void OnRtcMediaChannelCreated(const std::string& stream_id,
                                int channel_id,
                                bool video);
 private:
  void SendRemoveSignal(WebRTCSessionImpl* session);
  WebRTCSessionImpl* CreateMediaSession(const std::string& id,
                                        const std::string& dir);

  std::string config_;
  talk_base::scoped_ptr<talk_base::Thread> media_thread_;
  talk_base::scoped_ptr<WebRtcChannelManager> channel_manager_;
  talk_base::scoped_ptr<talk_base::NetworkManager> network_manager_;
  talk_base::scoped_ptr<cricket::BasicPortAllocator> port_allocator_;
  talk_base::scoped_ptr<talk_base::BasicPacketSocketFactory> socket_factory_;
  talk_base::scoped_ptr<talk_base::Thread> signaling_thread_;
  bool initialized_;

  // NOTE: The order of the enum values must be in sync with the array
  // in Init().
  enum ServiceType {
    STUN,
    STUNS,
    TURN,
    TURNS,
    SERVICE_COUNT,  // Also means 'invalid'.
  };

  ServiceType service_type_;
  std::string service_address_;
  PeerConnectionObserver* event_callback_;
  WebRTCSessionImpl* session_;
  bool incoming_;

#ifdef PLATFORM_CHROMIUM
  P2PSocketDispatcher* p2p_socket_dispatcher_;
#endif  // PLATFORM_CHROMIUM
};

}

#endif /* TALK_APP_WEBRTC_PEERCONNECTION_H_ */
