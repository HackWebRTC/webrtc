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
#ifndef TALK_APP_WEBRTC_PEERCONNECTIONMANAGER_H_
#define TALK_APP_WEBRTC_PEERCONNECTIONMANAGER_H_

#include "talk/base/scoped_ptr.h"
#include "talk/session/phone/channelmanager.h"

namespace talk_base {
class Thread;
}

namespace cricket {
class ChannelManager;
class DeviceManager;
class MediaEngine;
class PeerConnection;
class PortAllocator;
}

namespace webrtc {

class PeerConnection;

class PeerConnectionManager {
 public:
  static PeerConnectionManager* Create(
      cricket::MediaEngine* media_engine,
      cricket::DeviceManager* device_manager,
      cricket::PortAllocator* port_allocator,
      talk_base::Thread* worker_thread);
  static PeerConnectionManager* Create(
      cricket::PortAllocator* port_allocator,
      talk_base::Thread* worker_thread);

  PeerConnection* CreatePeerConnection();
  void DestroyPeerConnection(PeerConnection* pc);

 protected:
  PeerConnectionManager();
  virtual ~PeerConnectionManager() {};

 private:
  bool Initialize(cricket::MediaEngine* media_engine,
                  cricket::DeviceManager* device_manager,
                  cricket::PortAllocator* port_allocator,
                  talk_base::Thread* worker_thread);

  bool Initialize(cricket::PortAllocator* port_allocator,
                  talk_base::Thread* worker_thread);

  bool initialized_;
  talk_base::scoped_ptr<talk_base::Thread> signal_thread_;
  talk_base::scoped_ptr<cricket::PortAllocator> port_allocator_;
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
};

} // namespace webrtc

#endif // TALK_APP_WEBRTC_PEERCONNECTIONMANAGER_H_
