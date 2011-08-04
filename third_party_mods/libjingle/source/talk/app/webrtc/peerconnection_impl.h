/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#ifndef TALK_APP_WEBRTC_PEERCONNECTION_IMPL_H_
#define TALK_APP_WEBRTC_PEERCONNECTION_IMPL_H_

#include "talk/app/webrtc/peerconnection.h"

#include <string>
#include "talk/base/sigslot.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/packetsocketfactory.h"
#include "talk/base/thread.h"
#include "talk/session/phone/channelmanager.h"

namespace Json {
class Value;
}

namespace cricket {
class BasicPortAllocator;
class ChannelManager;
class DeviceManager;
class SessionDescription;
}

namespace webrtc {

class AudioDeviceModule;
class ExternalRenderer;
class WebRTCSession;

class PeerConnectionImpl : public PeerConnection,
                           public talk_base::MessageHandler,
                           public sigslot::has_slots<> {
 public:
  PeerConnectionImpl(const std::string& config,
      cricket::PortAllocator* port_allocator,
      cricket::MediaEngine* media_engine,
      talk_base::Thread* worker_thread,
      cricket::DeviceManager* device_manager);
  PeerConnectionImpl(const std::string& config,
                     cricket::PortAllocator* port_allocator,
                     talk_base::Thread* worker_thread);
  virtual ~PeerConnectionImpl();

  enum ReadyState {
    NEW = 0,
    NEGOTIATING,
    ACTIVE,
    CLOSED,
  };

  // PeerConnection interfaces
  bool Init();
  void RegisterObserver(PeerConnectionObserver* observer);
  bool SignalingMessage(const std::string& msg);
  bool AddStream(const std::string& stream_id, bool video);
  bool RemoveStream(const std::string& stream_id);
  bool Connect();
  void Close();
  bool SetAudioDevice(const std::string& wave_in_device,
                      const std::string& wave_out_device, int opts);
  bool SetLocalVideoRenderer(cricket::VideoRenderer* renderer);
  bool SetVideoRenderer(const std::string& stream_id,
                        cricket::VideoRenderer* renderer);
  bool SetVideoCapture(const std::string& cam_device);

  // Access to the members
  const std::string& config() const { return config_; }
  bool incoming() const { return incoming_; }
  cricket::ChannelManager* channel_manager() {
    return channel_manager_.get();
  }
  ReadyState ready_state() const { return ready_state_; }

  // Callbacks from PeerConnectionImplCallbacks
  void OnAddStream(const std::string& stream_id, bool video);
  void OnRemoveStream2(const std::string& stream_id, bool video);
  void OnLocalDescription(
      const cricket::SessionDescription* desc,
      const std::vector<cricket::Candidate>& candidates);
  void OnFailedCall();
  void OnRtcMediaChannelCreated(const std::string& stream_id,
                                bool video);

 private:
  bool ParseConfigString(const std::string& config,
                         talk_base::SocketAddress* stun_addr);
  void WrapChromiumThread();
  void SendRemoveSignal(WebRTCSession* session);
  WebRTCSession* CreateMediaSession(const std::string& id,
                                        const std::string& dir);

  virtual void OnMessage(talk_base::Message* message);

  // signaling thread methods
  bool AddStream_s(const std::string& stream_id, bool video);
  bool SignalingMessage_s(const std::string& signaling_message);
  bool RemoveStream_s(const std::string& stream_id);
  bool Connect_s();
  void Close_s();
  bool SetAudioDevice_s(const std::string& wave_in_device,
                        const std::string& wave_out_device, int opts);
  bool SetLocalVideoRenderer_s(cricket::VideoRenderer* renderer);
  bool SetVideoRenderer_s(const std::string& stream_id,
                          cricket::VideoRenderer* renderer);
  bool SetVideoCapture_s(const std::string& cam_device);
  void CreateChannelManager_s();
  void Release_s();

  std::string config_;
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
  cricket::PortAllocator* port_allocator_;
  cricket::MediaEngine* media_engine_;
  talk_base::Thread* worker_thread_;
  cricket::DeviceManager* device_manager_;
  talk_base::scoped_ptr<talk_base::Thread> signaling_thread_;

  bool initialized_;
  ReadyState ready_state_;
  // TODO(ronghuawu/tommi): Replace the initialized_ with ready_state_.
  // Fire notifications via the observer interface
  // when ready_state_ changes (i.e. onReadyStateChanged()).

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
  PeerConnectionObserver* event_callback_;
  talk_base::scoped_ptr<WebRTCSession> session_;
  // TODO(ronghua): There's no such concept as "incoming" and "outgoing"
  // according to the spec. This will be removed in the new PeerConnection.
  bool incoming_;
};
}

#endif  // TALK_APP_WEBRTC_PEERCONNECTION_IMPL_H_
