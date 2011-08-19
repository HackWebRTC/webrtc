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

#include "talk/app/webrtc/peerconnection_dev.h"
#include "talk/base/scoped_ptr.h"

namespace cricket {
class ChannelManager;
class PortAllocator;
}

namespace talk_base {
class Message;
}

namespace webrtc {

class WebRtcSession;

class PeerConnectionImpl : public PeerConnection {
 public:
  enum ReadyState {
    NEW = 0,
    NEGOTIATING,
    ACTIVE,
    CLOSED,
  };

  enum Error {
    ERROR_NONE = 0,             // Good
    ERROR_TIMEOUT = 1,          // No Candidates generated for X amount of time
    ERROR_AUDIO_DEVICE = 2,     // DeviceManager audio device error
    ERROR_VIDEO_DEVICE = 3,     // DeviceManager video device error
    ERROR_NETWORK = 4,          // Transport errors
    ERROR_MEDIADESCRIPTION = 5, // SignalingMessage error
    ERROR_MEDIA = 6,            // Related to Engines
    ERROR_UNKNOWN = 10,         // Everything else
  };

  PeerConnectionImpl(cricket::ChannelManager* channel_manager,
                     cricket::PortAllocator* port_allocator);
  virtual ~PeerConnectionImpl();

  // Interfaces from PeerConnection
  virtual bool StartNegotiation() {
    //TODO: implement
  }
  virtual bool SignalingMessage(const std::string& msg) {
    //TODO: implement
  }
  virtual bool Send(const std::string& msg) {
    //TODO: implement
  }
  virtual scoped_refptr<StreamCollection> local_streams() {
    //TODO: implement
  }
  virtual scoped_refptr<StreamCollection> remote_streams() {
    //TODO: implement
  }
  virtual void AddStream(LocalStream* stream);
  virtual void RemoveStream(LocalStream* stream);

  bool Init();
  void RegisterObserver(PeerConnectionObserver* observer);

  bool ProcessSignalingMessage(const std::string& msg);

  bool initialized() {
    return initialized_;
  }
  ReadyState ready_state() {
    return ready_state_;
  }

private:
  WebRtcSession* CreateSession();
  virtual void OnMessage(talk_base::Message* msg);
  void AddStream_s(LocalStream* stream);
  void RemoveStream_s(LocalStream* stream);
  void ProcessSignalingMessage_s(const std::string& msg);
  void StartNegotiation_s();

  bool initialized_;
  ReadyState ready_state_;
  PeerConnectionObserver* observer_;
  talk_base::scoped_ptr<WebRtcSession> session_;
  talk_base::scoped_ptr<talk_base::Thread> signaling_thread_;
  cricket::ChannelManager* channel_manager_;
  cricket::PortAllocator* port_allocator_;
};

} // namespace webrtc

#endif // TALK_APP_WEBRTC_PEERCONNECTION_IMPL_H_
