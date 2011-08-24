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

#ifndef TALK_APP_WEBRTC_PEERCONNECTION_PROXY_H_
#define TALK_APP_WEBRTC_PEERCONNECTION_PROXY_H_

#include <string>

#include "talk/app/webrtc/peerconnection.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"

namespace cricket {
class ChannelManager;
class PortAllocator;
}

namespace webrtc {

class PeerConnectionImpl;

class PeerConnectionProxy : public PeerConnection,
                            public talk_base::MessageHandler {
 public:
  PeerConnectionProxy(cricket::PortAllocator* port_allocator,
                      cricket::ChannelManager* channel_manager,
                      talk_base::Thread* signaling_thread);
  virtual ~PeerConnectionProxy();

  // PeerConnection interfaces
  void RegisterObserver(PeerConnectionObserver* observer);
  bool SignalingMessage(const std::string& msg);
  bool AddStream(const std::string& stream_id, bool video);
  bool RemoveStream(const std::string& stream_id);
  bool Connect();
  bool Close();
  bool SetAudioDevice(const std::string& wave_in_device,
                      const std::string& wave_out_device, int opts);
  bool SetLocalVideoRenderer(cricket::VideoRenderer* renderer);
  bool SetVideoRenderer(const std::string& stream_id,
                        cricket::VideoRenderer* renderer);
  bool SetVideoCapture(const std::string& cam_device);

 private:

  bool Init();
  bool Send(uint32 id, talk_base::MessageData* data);
  virtual void OnMessage(talk_base::Message* message);

  talk_base::scoped_ptr<PeerConnectionImpl> peerconnection_impl_;
  talk_base::Thread* signaling_thread_;

  friend class PeerConnectionFactory;
};
}

#endif  // TALK_APP_WEBRTC_PEERCONNECTION_PROXY_H_
