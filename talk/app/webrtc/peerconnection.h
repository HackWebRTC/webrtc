/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#include "talk/app/webrtc/dtlsidentitystore.h"
#include "talk/app/webrtc/peerconnectionfactory.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/rtpreceiverinterface.h"
#include "talk/app/webrtc/rtpsenderinterface.h"
#include "talk/app/webrtc/statscollector.h"
#include "talk/app/webrtc/streamcollection.h"
#include "talk/app/webrtc/webrtcsession.h"
#include "webrtc/base/scoped_ptr.h"

namespace webrtc {

class MediaStreamObserver;
class RemoteMediaStreamFactory;

// Populates |session_options| from |rtc_options|, and returns true if options
// are valid.
bool ConvertRtcOptionsForOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options);

// Populates |session_options| from |constraints|, and returns true if all
// mandatory constraints are satisfied.
bool ParseConstraintsForAnswer(const MediaConstraintsInterface* constraints,
                               cricket::MediaSessionOptions* session_options);

// Parses the URLs for each server in |servers| to build |stun_servers| and
// |turn_servers|.
bool ParseIceServers(const PeerConnectionInterface::IceServers& servers,
                     cricket::ServerAddresses* stun_servers,
                     std::vector<cricket::RelayServerConfig>* turn_servers);

// PeerConnection implements the PeerConnectionInterface interface.
// It uses WebRtcSession to implement the PeerConnection functionality.
class PeerConnection : public PeerConnectionInterface,
                       public IceObserver,
                       public rtc::MessageHandler,
                       public sigslot::has_slots<> {
 public:
  explicit PeerConnection(PeerConnectionFactory* factory);

  bool Initialize(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      const MediaConstraintsInterface* constraints,
      rtc::scoped_ptr<cricket::PortAllocator> allocator,
      rtc::scoped_ptr<DtlsIdentityStoreInterface> dtls_identity_store,
      PeerConnectionObserver* observer);

  rtc::scoped_refptr<StreamCollectionInterface> local_streams() override;
  rtc::scoped_refptr<StreamCollectionInterface> remote_streams() override;
  bool AddStream(MediaStreamInterface* local_stream) override;
  void RemoveStream(MediaStreamInterface* local_stream) override;

  virtual WebRtcSession* session() { return session_.get(); }

  rtc::scoped_refptr<DtmfSenderInterface> CreateDtmfSender(
      AudioTrackInterface* track) override;

  rtc::scoped_refptr<RtpSenderInterface> CreateSender(
      const std::string& kind,
      const std::string& stream_id) override;

  std::vector<rtc::scoped_refptr<RtpSenderInterface>> GetSenders()
      const override;
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceivers()
      const override;

  rtc::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config) override;
  bool GetStats(StatsObserver* observer,
                webrtc::MediaStreamTrackInterface* track,
                StatsOutputLevel level) override;

  SignalingState signaling_state() override;

  // TODO(bemasc): Remove ice_state() when callers are removed.
  IceState ice_state() override;
  IceConnectionState ice_connection_state() override;
  IceGatheringState ice_gathering_state() override;

  const SessionDescriptionInterface* local_description() const override;
  const SessionDescriptionInterface* remote_description() const override;

  // JSEP01
  void CreateOffer(CreateSessionDescriptionObserver* observer,
                   const MediaConstraintsInterface* constraints) override;
  void CreateOffer(CreateSessionDescriptionObserver* observer,
                   const RTCOfferAnswerOptions& options) override;
  void CreateAnswer(CreateSessionDescriptionObserver* observer,
                    const MediaConstraintsInterface* constraints) override;
  void SetLocalDescription(SetSessionDescriptionObserver* observer,
                           SessionDescriptionInterface* desc) override;
  void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                            SessionDescriptionInterface* desc) override;
  bool SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration& config) override;
  bool AddIceCandidate(const IceCandidateInterface* candidate) override;

  void RegisterUMAObserver(UMAObserver* observer) override;

  void Close() override;

  // Virtual for unit tests.
  virtual const std::vector<rtc::scoped_refptr<DataChannel>>&
  sctp_data_channels() const {
    return sctp_data_channels_;
  };

 protected:
  ~PeerConnection() override;

 private:
  struct TrackInfo {
    TrackInfo() : ssrc(0) {}
    TrackInfo(const std::string& stream_label,
              const std::string track_id,
              uint32_t ssrc)
        : stream_label(stream_label), track_id(track_id), ssrc(ssrc) {}
    bool operator==(const TrackInfo& other) {
      return this->stream_label == other.stream_label &&
             this->track_id == other.track_id && this->ssrc == other.ssrc;
    }
    std::string stream_label;
    std::string track_id;
    uint32_t ssrc;
  };
  typedef std::vector<TrackInfo> TrackInfos;

  // Implements MessageHandler.
  void OnMessage(rtc::Message* msg) override;

  void CreateAudioReceiver(MediaStreamInterface* stream,
                           AudioTrackInterface* audio_track,
                           uint32_t ssrc);
  void CreateVideoReceiver(MediaStreamInterface* stream,
                           VideoTrackInterface* video_track,
                           uint32_t ssrc);
  void DestroyAudioReceiver(MediaStreamInterface* stream,
                            AudioTrackInterface* audio_track);
  void DestroyVideoReceiver(MediaStreamInterface* stream,
                            VideoTrackInterface* video_track);
  void DestroyAudioSender(MediaStreamInterface* stream,
                          AudioTrackInterface* audio_track,
                          uint32_t ssrc);
  void DestroyVideoSender(MediaStreamInterface* stream,
                          VideoTrackInterface* video_track);

  // Implements IceObserver
  void OnIceConnectionChange(IceConnectionState new_state) override;
  void OnIceGatheringChange(IceGatheringState new_state) override;
  void OnIceCandidate(const IceCandidateInterface* candidate) override;
  void OnIceComplete() override;
  void OnIceConnectionReceivingChange(bool receiving) override;

  // Signals from WebRtcSession.
  void OnSessionStateChange(WebRtcSession* session, WebRtcSession::State state);
  void ChangeSignalingState(SignalingState signaling_state);

  // Signals from MediaStreamObserver.
  void OnAudioTrackAdded(AudioTrackInterface* track,
                         MediaStreamInterface* stream);
  void OnAudioTrackRemoved(AudioTrackInterface* track,
                           MediaStreamInterface* stream);
  void OnVideoTrackAdded(VideoTrackInterface* track,
                         MediaStreamInterface* stream);
  void OnVideoTrackRemoved(VideoTrackInterface* track,
                           MediaStreamInterface* stream);

  rtc::Thread* signaling_thread() const {
    return factory_->signaling_thread();
  }

  void PostSetSessionDescriptionFailure(SetSessionDescriptionObserver* observer,
                                        const std::string& error);
  void PostCreateSessionDescriptionFailure(
      CreateSessionDescriptionObserver* observer,
      const std::string& error);

  bool IsClosed() const {
    return signaling_state_ == PeerConnectionInterface::kClosed;
  }

  // Returns a MediaSessionOptions struct with options decided by |options|,
  // the local MediaStreams and DataChannels.
  virtual bool GetOptionsForOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
      cricket::MediaSessionOptions* session_options);

  // Returns a MediaSessionOptions struct with options decided by
  // |constraints|, the local MediaStreams and DataChannels.
  virtual bool GetOptionsForAnswer(
      const MediaConstraintsInterface* constraints,
      cricket::MediaSessionOptions* session_options);

  // Remove all local and remote tracks of type |media_type|.
  // Called when a media type is rejected (m-line set to port 0).
  void RemoveTracks(cricket::MediaType media_type);

  // Makes sure a MediaStreamTrack is created for each StreamParam in |streams|,
  // and existing MediaStreamTracks are removed if there is no corresponding
  // StreamParam. If |default_track_needed| is true, a default MediaStreamTrack
  // is created if it doesn't exist; if false, it's removed if it exists.
  // |media_type| is the type of the |streams| and can be either audio or video.
  // If a new MediaStream is created it is added to |new_streams|.
  void UpdateRemoteStreamsList(
      const std::vector<cricket::StreamParams>& streams,
      bool default_track_needed,
      cricket::MediaType media_type,
      StreamCollection* new_streams);

  // Triggered when a remote track has been seen for the first time in a remote
  // session description. It creates a remote MediaStreamTrackInterface
  // implementation and triggers CreateAudioReceiver or CreateVideoReceiver.
  void OnRemoteTrackSeen(const std::string& stream_label,
                         const std::string& track_id,
                         uint32_t ssrc,
                         cricket::MediaType media_type);

  // Triggered when a remote track has been removed from a remote session
  // description. It removes the remote track with id |track_id| from a remote
  // MediaStream and triggers DestroyAudioReceiver or DestroyVideoReceiver.
  void OnRemoteTrackRemoved(const std::string& stream_label,
                            const std::string& track_id,
                            cricket::MediaType media_type);

  // Finds remote MediaStreams without any tracks and removes them from
  // |remote_streams_| and notifies the observer that the MediaStreams no longer
  // exist.
  void UpdateEndedRemoteMediaStreams();

  // Set the MediaStreamTrackInterface::TrackState to |kEnded| on all remote
  // tracks of type |media_type|.
  void EndRemoteTracks(cricket::MediaType media_type);

  // Loops through the vector of |streams| and finds added and removed
  // StreamParams since last time this method was called.
  // For each new or removed StreamParam, OnLocalTrackSeen or
  // OnLocalTrackRemoved is invoked.
  void UpdateLocalTracks(const std::vector<cricket::StreamParams>& streams,
                         cricket::MediaType media_type);

  // Triggered when a local track has been seen for the first time in a local
  // session description.
  // This method triggers CreateAudioSender or CreateVideoSender if the rtp
  // streams in the local SessionDescription can be mapped to a MediaStreamTrack
  // in a MediaStream in |local_streams_|
  void OnLocalTrackSeen(const std::string& stream_label,
                        const std::string& track_id,
                        uint32_t ssrc,
                        cricket::MediaType media_type);

  // Triggered when a local track has been removed from a local session
  // description.
  // This method triggers DestroyAudioSender or DestroyVideoSender if a stream
  // has been removed from the local SessionDescription and the stream can be
  // mapped to a MediaStreamTrack in a MediaStream in |local_streams_|.
  void OnLocalTrackRemoved(const std::string& stream_label,
                           const std::string& track_id,
                           uint32_t ssrc,
                           cricket::MediaType media_type);

  void UpdateLocalRtpDataChannels(const cricket::StreamParamsVec& streams);
  void UpdateRemoteRtpDataChannels(const cricket::StreamParamsVec& streams);
  void UpdateClosingRtpDataChannels(
      const std::vector<std::string>& active_channels,
      bool is_local_update);
  void CreateRemoteRtpDataChannel(const std::string& label,
                                  uint32_t remote_ssrc);

  // Creates channel and adds it to the collection of DataChannels that will
  // be offered in a SessionDescription.
  rtc::scoped_refptr<DataChannel> InternalCreateDataChannel(
      const std::string& label,
      const InternalDataChannelInit* config);

  // Checks if any data channel has been added.
  bool HasDataChannels() const;

  void AllocateSctpSids(rtc::SSLRole role);
  void OnSctpDataChannelClosed(DataChannel* channel);

  // Notifications from WebRtcSession relating to BaseChannels.
  void OnVoiceChannelDestroyed();
  void OnVideoChannelDestroyed();
  void OnDataChannelCreated();
  void OnDataChannelDestroyed();
  // Called when the cricket::DataChannel receives a message indicating that a
  // webrtc::DataChannel should be opened.
  void OnDataChannelOpenMessage(const std::string& label,
                                const InternalDataChannelInit& config);

  RtpSenderInterface* FindSenderById(const std::string& id);

  std::vector<rtc::scoped_refptr<RtpSenderInterface>>::iterator
  FindSenderForTrack(MediaStreamTrackInterface* track);
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>>::iterator
  FindReceiverForTrack(MediaStreamTrackInterface* track);

  TrackInfos* GetRemoteTracks(cricket::MediaType media_type);
  TrackInfos* GetLocalTracks(cricket::MediaType media_type);
  const TrackInfo* FindTrackInfo(const TrackInfos& infos,
                                 const std::string& stream_label,
                                 const std::string track_id) const;

  // Returns the specified SCTP DataChannel in sctp_data_channels_,
  // or nullptr if not found.
  DataChannel* FindDataChannelBySid(int sid) const;

  // Storing the factory as a scoped reference pointer ensures that the memory
  // in the PeerConnectionFactoryImpl remains available as long as the
  // PeerConnection is running. It is passed to PeerConnection as a raw pointer.
  // However, since the reference counting is done in the
  // PeerConnectionFactoryInterface all instances created using the raw pointer
  // will refer to the same reference count.
  rtc::scoped_refptr<PeerConnectionFactory> factory_;
  PeerConnectionObserver* observer_;
  UMAObserver* uma_observer_;
  SignalingState signaling_state_;
  // TODO(bemasc): Remove ice_state_.
  IceState ice_state_;
  IceConnectionState ice_connection_state_;
  IceGatheringState ice_gathering_state_;

  rtc::scoped_ptr<cricket::PortAllocator> port_allocator_;
  rtc::scoped_ptr<MediaControllerInterface> media_controller_;

  // Streams added via AddStream.
  rtc::scoped_refptr<StreamCollection> local_streams_;
  // Streams created as a result of SetRemoteDescription.
  rtc::scoped_refptr<StreamCollection> remote_streams_;

  std::vector<rtc::scoped_ptr<MediaStreamObserver>> stream_observers_;

  // These lists store track info seen in local/remote descriptions.
  TrackInfos remote_audio_tracks_;
  TrackInfos remote_video_tracks_;
  TrackInfos local_audio_tracks_;
  TrackInfos local_video_tracks_;

  SctpSidAllocator sid_allocator_;
  // label -> DataChannel
  std::map<std::string, rtc::scoped_refptr<DataChannel>> rtp_data_channels_;
  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels_;
  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels_to_free_;

  bool remote_peer_supports_msid_ = false;
  rtc::scoped_ptr<RemoteMediaStreamFactory> remote_stream_factory_;

  std::vector<rtc::scoped_refptr<RtpSenderInterface>> senders_;
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> receivers_;

  // The session_ scoped_ptr is declared at the bottom of PeerConnection
  // because its destruction fires signals (such as VoiceChannelDestroyed)
  // which will trigger some final actions in PeerConnection...
  rtc::scoped_ptr<WebRtcSession> session_;
  // ... But stats_ depends on session_ so it should be destroyed even earlier.
  rtc::scoped_ptr<StatsCollector> stats_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTION_H_
