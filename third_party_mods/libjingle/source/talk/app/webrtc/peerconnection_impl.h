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

#include <string>
#include <vector>

#include "talk/app/webrtc/peerconnection.h"
#include "talk/base/sigslot.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/session/phone/channelmanager.h"

namespace cricket {
class ChannelManager;
class PortAllocator;
class SessionDescription;
}

namespace webrtc {
class WebRtcSession;

class PeerConnectionImpl : public PeerConnection,
                           public sigslot::has_slots<> {
 public:
  PeerConnectionImpl(cricket::PortAllocator* port_allocator,
                     cricket::ChannelManager* channel_manager,
                     talk_base::Thread* signaling_thread);
  virtual ~PeerConnectionImpl();

  // PeerConnection interfaces
  virtual void RegisterObserver(PeerConnectionObserver* observer);
  virtual bool SignalingMessage(const std::string& msg);
  virtual bool AddStream(const std::string& stream_id, bool video);
  virtual bool RemoveStream(const std::string& stream_id);
  virtual bool Connect();
  virtual bool Close();
  virtual bool SetAudioDevice(const std::string& wave_in_device,
                              const std::string& wave_out_device, int opts);
  virtual bool SetLocalVideoRenderer(cricket::VideoRenderer* renderer);
  virtual bool SetVideoRenderer(const std::string& stream_id,
                                cricket::VideoRenderer* renderer);
  virtual bool SetVideoCapture(const std::string& cam_device);
  virtual ReadyState GetReadyState();

  cricket::ChannelManager* channel_manager() {
    return channel_manager_;
  }

  // Callbacks from PeerConnectionImplCallbacks
  void OnAddStream(const std::string& stream_id, bool video);
  void OnRemoveStream(const std::string& stream_id, bool video);
  void OnLocalDescription(
      const cricket::SessionDescription* desc,
      const std::vector<cricket::Candidate>& candidates);
  void OnFailedCall();
  void OnRtcMediaChannelCreated(const std::string& stream_id,
                                bool video);
  bool Init();

 private:
  bool ParseConfigString(const std::string& config,
                         talk_base::SocketAddress* stun_addr);
  void SendRemoveSignal(WebRtcSession* session);
  WebRtcSession* CreateMediaSession(const std::string& id, bool incoming);

  cricket::PortAllocator* port_allocator_;
  cricket::ChannelManager* channel_manager_;
  talk_base::Thread* signaling_thread_;
  PeerConnectionObserver* event_callback_;
  talk_base::scoped_ptr<WebRtcSession> session_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTION_IMPL_H_
