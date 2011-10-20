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
#ifndef TALK_APP_WEBRTC_PEERCONNECTIONMANAGERIMPL_H_
#define TALK_APP_WEBRTC_PEERCONNECTIONMANAGERIMPL_H_

#include <string>

#include "talk/base/scoped_ptr.h"
#include "talk/app/webrtc_dev/peerconnection.h"
#include "talk/app/webrtc_dev/mediastream.h"
#include "talk/base/thread.h"
#include "talk/session/phone/channelmanager.h"

namespace webrtc {

class PeerConnectionManagerImpl : public PeerConnectionManager,
                                  public talk_base::MessageHandler {
 public:
  talk_base::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      const std::string& config,
      PeerConnectionObserver* observer);
  bool Initialize();

  virtual talk_base::scoped_refptr<LocalMediaStreamInterface>
      CreateLocalMediaStream(const std::string& label);

  virtual talk_base::scoped_refptr<LocalVideoTrackInterface>
      CreateLocalVideoTrack(const std::string& label,
                            VideoCaptureModule* video_device);

  virtual talk_base::scoped_refptr<LocalAudioTrackInterface>
      CreateLocalAudioTrack(const std::string& label,
                            AudioDeviceModule* audio_device);

 protected:
  PeerConnectionManagerImpl();
  PeerConnectionManagerImpl(talk_base::Thread* worker_thread,
                            talk_base::Thread* signaling_thread,
                            PcNetworkManager* network_manager,
                            PcPacketSocketFactory* socket_factory,
                            AudioDeviceModule* default_adm);
  virtual ~PeerConnectionManagerImpl();


 private:
  bool Initialize_s();
  talk_base::scoped_refptr<PeerConnectionInterface> CreatePeerConnection_s(
      const std::string& configuration,
      PeerConnectionObserver* observer);
  // Implements talk_base::MessageHandler.
  void OnMessage(talk_base::Message* msg);

  talk_base::scoped_ptr<talk_base::Thread> signaling_thread_;
  talk_base::Thread* signaling_thread_ptr_;
  talk_base::scoped_ptr<talk_base::Thread> worker_thread_;
  talk_base::Thread* worker_thread_ptr_;
  talk_base::scoped_refptr<PcNetworkManager> network_manager_;
  talk_base::scoped_refptr<PcPacketSocketFactory> socket_factory_;
  // External Audio device used for audio playback.
  talk_base::scoped_refptr<AudioDeviceModule> default_adm_;
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTIONMANAGER_IMPL_H_
