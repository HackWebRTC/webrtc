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
#ifndef TALK_APP_WEBRTC_PEERCONNECTIONFACTORY_H_
#define TALK_APP_WEBRTC_PEERCONNECTIONFACTORY_H_

#include <string>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/session/media/channelmanager.h"

namespace webrtc {

class PeerConnectionFactory : public PeerConnectionFactoryInterface,
                              public talk_base::MessageHandler {
 public:
  virtual void SetOptions(const Options& options) {
    options_ = options;
  }

  virtual talk_base::scoped_refptr<PeerConnectionInterface>
      CreatePeerConnection(
          const PeerConnectionInterface::IceServers& configuration,
          const MediaConstraintsInterface* constraints,
          DTLSIdentityServiceInterface* dtls_identity_service,
          PeerConnectionObserver* observer);

  virtual talk_base::scoped_refptr<PeerConnectionInterface>
      CreatePeerConnection(
          const PeerConnectionInterface::IceServers& configuration,
          const MediaConstraintsInterface* constraints,
          PortAllocatorFactoryInterface* allocator_factory,
          DTLSIdentityServiceInterface* dtls_identity_service,
          PeerConnectionObserver* observer);
  bool Initialize();

  virtual talk_base::scoped_refptr<MediaStreamInterface>
      CreateLocalMediaStream(const std::string& label);

  virtual talk_base::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const MediaConstraintsInterface* constraints);

  virtual talk_base::scoped_refptr<VideoSourceInterface> CreateVideoSource(
      cricket::VideoCapturer* capturer,
      const MediaConstraintsInterface* constraints);

  virtual talk_base::scoped_refptr<VideoTrackInterface>
      CreateVideoTrack(const std::string& id,
                       VideoSourceInterface* video_source);

  virtual talk_base::scoped_refptr<AudioTrackInterface>
      CreateAudioTrack(const std::string& id,
                       AudioSourceInterface* audio_source);

  virtual bool StartAecDump(talk_base::PlatformFile file);

  virtual cricket::ChannelManager* channel_manager();
  virtual talk_base::Thread* signaling_thread();
  virtual talk_base::Thread* worker_thread();
  const Options& options() const { return options_; }

 protected:
  PeerConnectionFactory();
  PeerConnectionFactory(
      talk_base::Thread* worker_thread,
      talk_base::Thread* signaling_thread,
      AudioDeviceModule* default_adm,
      cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
      cricket::WebRtcVideoDecoderFactory* video_decoder_factory);
  virtual ~PeerConnectionFactory();

 private:
  bool Initialize_s();
  void Terminate_s();
  talk_base::scoped_refptr<AudioSourceInterface> CreateAudioSource_s(
      const MediaConstraintsInterface* constraints);
  talk_base::scoped_refptr<VideoSourceInterface> CreateVideoSource_s(
      cricket::VideoCapturer* capturer,
      const MediaConstraintsInterface* constraints);
  talk_base::scoped_refptr<PeerConnectionInterface> CreatePeerConnection_s(
      const PeerConnectionInterface::IceServers& configuration,
      const MediaConstraintsInterface* constraints,
      PortAllocatorFactoryInterface* allocator_factory,
      DTLSIdentityServiceInterface* dtls_identity_service,
      PeerConnectionObserver* observer);
  bool StartAecDump_s(talk_base::PlatformFile file);

  // Implements talk_base::MessageHandler.
  void OnMessage(talk_base::Message* msg);

  bool owns_ptrs_;
  talk_base::Thread* signaling_thread_;
  talk_base::Thread* worker_thread_;
  Options options_;
  talk_base::scoped_refptr<PortAllocatorFactoryInterface> allocator_factory_;
  // External Audio device used for audio playback.
  talk_base::scoped_refptr<AudioDeviceModule> default_adm_;
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
  // External Video encoder factory. This can be NULL if the client has not
  // injected any. In that case, video engine will use the internal SW encoder.
  talk_base::scoped_ptr<cricket::WebRtcVideoEncoderFactory>
      video_encoder_factory_;
  // External Video decoder factory. This can be NULL if the client has not
  // injected any. In that case, video engine will use the internal SW decoder.
  talk_base::scoped_ptr<cricket::WebRtcVideoDecoderFactory>
      video_decoder_factory_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTIONFACTORY_H_
