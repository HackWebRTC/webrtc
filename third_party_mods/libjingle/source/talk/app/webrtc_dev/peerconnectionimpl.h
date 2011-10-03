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

#ifndef TALK_APP_WEBRTC_PEERCONNECTIONIMPL_H_
#define TALK_APP_WEBRTC_PEERCONNECTIONIMPL_H_

#include <map>
#include <string>

#include "talk/app/webrtc_dev/peerconnection.h"
#include "talk/app/webrtc_dev/peerconnectionsignaling.h"
#include "talk/app/webrtc_dev/webrtcsession.h"
#include "talk/base/scoped_ptr.h"
#include "talk/p2p/client/httpportallocator.h"

namespace cricket {
class ChannelManager;
}

namespace webrtc {
class MediaStreamHandlers;
class StreamCollectionImpl;


// PeerConnectionImpl implements the PeerConnection interface.
// It uses PeerConnectionSignaling and WebRtcSession to implement
// the PeerConnection functionality.
class PeerConnectionImpl : public PeerConnection,
                           public talk_base::MessageHandler,
                           public sigslot::has_slots<> {
 public:
  PeerConnectionImpl(cricket::ChannelManager* channel_manager,
                     talk_base::Thread* signaling_thread,
                     talk_base::Thread* worker_thread,
                     PcNetworkManager* network_manager,
                     PcPacketSocketFactory* socket_factory);

  bool Initialize(const std::string& configuration,
                  PeerConnectionObserver* observer);

  virtual ~PeerConnectionImpl();

  virtual bool ProcessSignalingMessage(const std::string& msg);
  virtual bool Send(const std::string& msg) {
    // TODO(perkj): implement
    ASSERT(false);
  }
  virtual scoped_refptr<StreamCollection> local_streams();
  virtual scoped_refptr<StreamCollection> remote_streams();
  virtual void AddStream(LocalMediaStream* stream);
  virtual void RemoveStream(LocalMediaStream* stream);
  virtual void CommitStreamChanges();

 private:
  // Implement talk_base::MessageHandler.
  void OnMessage(talk_base::Message* msg);

  // Signals from PeerConnectionSignaling.
  void OnNewPeerConnectionMessage(const std::string& message);
  void OnRemoteStreamAdded(MediaStream* remote_stream);
  void OnRemoteStreamRemoved(MediaStream* remote_stream);

  PeerConnectionObserver* observer_;
  scoped_refptr<StreamCollectionImpl> local_media_streams_;
  scoped_refptr<StreamCollectionImpl> remote_media_streams_;

  talk_base::Thread* signaling_thread_;  // Weak ref from PeerConnectionManager.
  cricket::ChannelManager* channel_manager_;
  scoped_refptr<PcNetworkManager> network_manager_;
  scoped_refptr<PcPacketSocketFactory> socket_factory_;
  talk_base::scoped_ptr<cricket::HttpPortAllocator> port_allocator_;
  talk_base::scoped_ptr<PeerConnectionSignaling> signaling_;
  talk_base::scoped_ptr<WebRtcSession> session_;
  talk_base::scoped_ptr<MediaStreamHandlers> stream_handler_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTIONIMPL_H_
