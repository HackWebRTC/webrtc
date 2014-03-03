/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#ifndef TALK_APP_WEBRTC_PEERCONNECTION_H_
#define TALK_APP_WEBRTC_PEERCONNECTION_H_

#include <string>

#include "talk/app/webrtc/mediastreamsignaling.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/peerconnectionfactory.h"
#include "talk/app/webrtc/statscollector.h"
#include "talk/app/webrtc/streamcollection.h"
#include "talk/app/webrtc/webrtcsession.h"
#include "talk/base/scoped_ptr.h"

namespace webrtc {
class MediaStreamHandlerContainer;

typedef std::vector<PortAllocatorFactoryInterface::StunConfiguration>
    StunConfigurations;
typedef std::vector<PortAllocatorFactoryInterface::TurnConfiguration>
    TurnConfigurations;

// PeerConnectionImpl implements the PeerConnection interface.
// It uses MediaStreamSignaling and WebRtcSession to implement
// the PeerConnection functionality.
class PeerConnection : public PeerConnectionInterface,
                       public MediaStreamSignalingObserver,
                       public IceObserver,
                       public talk_base::MessageHandler,
                       public sigslot::has_slots<> {
 public:
  explicit PeerConnection(PeerConnectionFactory* factory);

  bool Initialize(const PeerConnectionInterface::IceServers& configuration,
                  const MediaConstraintsInterface* constraints,
                  PortAllocatorFactoryInterface* allocator_factory,
                  DTLSIdentityServiceInterface* dtls_identity_service,
                  PeerConnectionObserver* observer);
  virtual talk_base::scoped_refptr<StreamCollectionInterface> local_streams();
  virtual talk_base::scoped_refptr<StreamCollectionInterface> remote_streams();
  virtual bool AddStream(MediaStreamInterface* local_stream,
                         const MediaConstraintsInterface* constraints);
  virtual void RemoveStream(MediaStreamInterface* local_stream);

  virtual talk_base::scoped_refptr<DtmfSenderInterface> CreateDtmfSender(
      AudioTrackInterface* track);

  virtual talk_base::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config);
  virtual bool GetStats(StatsObserver* observer,
                        webrtc::MediaStreamTrackInterface* track);
  virtual bool GetStats(StatsObserver* observer,
                        webrtc::MediaStreamTrackInterface* track,
                        StatsOutputLevel level);

  virtual SignalingState signaling_state();

  // TODO(bemasc): Remove ice_state() when callers are removed.
  virtual IceState ice_state();
  virtual IceConnectionState ice_connection_state();
  virtual IceGatheringState ice_gathering_state();

  virtual const SessionDescriptionInterface* local_description() const;
  virtual const SessionDescriptionInterface* remote_description() const;

  // JSEP01
  virtual void CreateOffer(CreateSessionDescriptionObserver* observer,
                           const MediaConstraintsInterface* constraints);
  virtual void CreateAnswer(CreateSessionDescriptionObserver* observer,
                            const MediaConstraintsInterface* constraints);
  virtual void SetLocalDescription(SetSessionDescriptionObserver* observer,
                                   SessionDescriptionInterface* desc);
  virtual void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                                    SessionDescriptionInterface* desc);
  virtual bool UpdateIce(const IceServers& configuration,
                         const MediaConstraintsInterface* constraints);
  virtual bool AddIceCandidate(const IceCandidateInterface* candidate);

  virtual void Close();

 protected:
  virtual ~PeerConnection();

 private:
  // Implements MessageHandler.
  virtual void OnMessage(talk_base::Message* msg);

  // Implements MediaStreamSignalingObserver.
  virtual void OnAddRemoteStream(MediaStreamInterface* stream) OVERRIDE;
  virtual void OnRemoveRemoteStream(MediaStreamInterface* stream) OVERRIDE;
  virtual void OnAddDataChannel(DataChannelInterface* data_channel) OVERRIDE;
  virtual void OnAddRemoteAudioTrack(MediaStreamInterface* stream,
                                     AudioTrackInterface* audio_track,
                                     uint32 ssrc) OVERRIDE;
  virtual void OnAddRemoteVideoTrack(MediaStreamInterface* stream,
                                     VideoTrackInterface* video_track,
                                     uint32 ssrc) OVERRIDE;
  virtual void OnRemoveRemoteAudioTrack(
      MediaStreamInterface* stream,
      AudioTrackInterface* audio_track) OVERRIDE;
  virtual void OnRemoveRemoteVideoTrack(
      MediaStreamInterface* stream,
      VideoTrackInterface* video_track) OVERRIDE;
  virtual void OnAddLocalAudioTrack(MediaStreamInterface* stream,
                                    AudioTrackInterface* audio_track,
                                    uint32 ssrc) OVERRIDE;
  virtual void OnAddLocalVideoTrack(MediaStreamInterface* stream,
                                    VideoTrackInterface* video_track,
                                    uint32 ssrc) OVERRIDE;
  virtual void OnRemoveLocalAudioTrack(
      MediaStreamInterface* stream,
      AudioTrackInterface* audio_track,
      uint32 ssrc) OVERRIDE;
  virtual void OnRemoveLocalVideoTrack(
      MediaStreamInterface* stream,
      VideoTrackInterface* video_track) OVERRIDE;
  virtual void OnRemoveLocalStream(MediaStreamInterface* stream);

  // Implements IceObserver
  virtual void OnIceConnectionChange(IceConnectionState new_state);
  virtual void OnIceGatheringChange(IceGatheringState new_state);
  virtual void OnIceCandidate(const IceCandidateInterface* candidate);
  virtual void OnIceComplete();

  // Signals from WebRtcSession.
  void OnSessionStateChange(cricket::BaseSession* session,
                            cricket::BaseSession::State state);
  void ChangeSignalingState(SignalingState signaling_state);

  bool DoInitialize(const StunConfigurations& stun_config,
                    const TurnConfigurations& turn_config,
                    const MediaConstraintsInterface* constraints,
                    PortAllocatorFactoryInterface* allocator_factory,
                    DTLSIdentityServiceInterface* dtls_identity_service,
                    PeerConnectionObserver* observer);

  talk_base::Thread* signaling_thread() const {
    return factory_->signaling_thread();
  }

  void PostSetSessionDescriptionFailure(SetSessionDescriptionObserver* observer,
                                        const std::string& error);

  bool IsClosed() const {
    return signaling_state_ == PeerConnectionInterface::kClosed;
  }

  // Storing the factory as a scoped reference pointer ensures that the memory
  // in the PeerConnectionFactoryImpl remains available as long as the
  // PeerConnection is running. It is passed to PeerConnection as a raw pointer.
  // However, since the reference counting is done in the
  // PeerConnectionFactoryInteface all instances created using the raw pointer
  // will refer to the same reference count.
  talk_base::scoped_refptr<PeerConnectionFactory> factory_;
  PeerConnectionObserver* observer_;
  SignalingState signaling_state_;
  // TODO(bemasc): Remove ice_state_.
  IceState ice_state_;
  IceConnectionState ice_connection_state_;
  IceGatheringState ice_gathering_state_;

  talk_base::scoped_ptr<cricket::PortAllocator> port_allocator_;
  talk_base::scoped_ptr<WebRtcSession> session_;
  talk_base::scoped_ptr<MediaStreamSignaling> mediastream_signaling_;
  talk_base::scoped_ptr<MediaStreamHandlerContainer> stream_handler_container_;
  StatsCollector stats_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTION_H_
