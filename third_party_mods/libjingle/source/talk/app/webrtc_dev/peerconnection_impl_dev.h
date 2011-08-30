/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#include <list>
#include <map>
#include <string>

#include "talk/app/webrtc_dev/peerconnection_dev.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/messagequeue.h"

namespace cricket {
class ChannelManager;
class PortAllocator;
}

namespace webrtc {

class PeerConnectionImpl : public PeerConnection,
                           public talk_base::MessageHandler {
 public:
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
                     cricket::PortAllocator* port_allocator,
                     talk_base::Thread* signal_thread);
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
  virtual void AddStream(LocalMediaStream* stream);
  virtual void RemoveStream(LocalMediaStream* stream);
  virtual void CommitStreamChanges();

  void RegisterObserver(PeerConnectionObserver* observer);

  // Implement talk_base::MessageHandler.
  void OnMessage(talk_base::Message* msg);

 private:
  enum {
      MSG_ADDMEDIASTREAM = 1,
      MSG_REMOVEMEDIASTREAM = 2,
      MSG_COMMITSTREAMCHANGES = 3
  };

  PeerConnectionObserver* observer_;

  // Map of local media streams.
  typedef std::map<std::string, scoped_refptr<LocalMediaStream> > LocalStreamMap;
  LocalStreamMap local_media_streams_;

  talk_base::Thread* signal_thread_;
  cricket::ChannelManager* channel_manager_;
  cricket::PortAllocator* port_allocator_;
};

} // namespace webrtc

#endif // TALK_APP_WEBRTC_PEERCONNECTION_IMPL_H_
