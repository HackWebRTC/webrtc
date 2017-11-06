/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_PEERCONNECTION_H_
#define PC_PEERCONNECTION_H_

#include <string>
#include <map>
#include <memory>
#include <vector>

#include "api/peerconnectioninterface.h"
#include "api/turncustomizer.h"
#include "pc/iceserverparsing.h"
#include "pc/peerconnectionfactory.h"
#include "pc/rtcstatscollector.h"
#include "pc/rtpreceiver.h"
#include "pc/rtpsender.h"
#include "pc/statscollector.h"
#include "pc/streamcollection.h"
#include "pc/webrtcsession.h"

namespace webrtc {

class MediaStreamObserver;
class VideoRtpReceiver;
class RtcEventLog;

// TODO(steveanton): Remove once WebRtcSession is merged into PeerConnection.
std::string GetSignalingStateString(
    PeerConnectionInterface::SignalingState state);

// PeerConnection implements the PeerConnectionInterface interface.
// It uses WebRtcSession to implement the PeerConnection functionality.
class PeerConnection : public PeerConnectionInterface,
                       public rtc::MessageHandler,
                       public sigslot::has_slots<> {
 public:
  // TODO(steveanton): Remove once WebRtcSession is merged into PeerConnection.
  friend class WebRtcSession;

  explicit PeerConnection(PeerConnectionFactory* factory,
                          std::unique_ptr<RtcEventLog> event_log,
                          std::unique_ptr<Call> call);

  bool Initialize(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      std::unique_ptr<cricket::PortAllocator> allocator,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      PeerConnectionObserver* observer);

  rtc::scoped_refptr<StreamCollectionInterface> local_streams() override;
  rtc::scoped_refptr<StreamCollectionInterface> remote_streams() override;
  bool AddStream(MediaStreamInterface* local_stream) override;
  void RemoveStream(MediaStreamInterface* local_stream) override;

  rtc::scoped_refptr<RtpSenderInterface> AddTrack(
      MediaStreamTrackInterface* track,
      std::vector<MediaStreamInterface*> streams) override;
  bool RemoveTrack(RtpSenderInterface* sender) override;

  // TODO(steveanton): Remove this once all clients have switched to using the
  // PeerConnection shims for WebRtcSession methods instead of the methods
  // directly via this getter.
  virtual WebRtcSession* session() { return session_; }

  // Gets the DTLS SSL certificate associated with the audio transport on the
  // remote side. This will become populated once the DTLS connection with the
  // peer has been completed, as indicated by the ICE connection state
  // transitioning to kIceConnectionCompleted.
  // Note that this will be removed once we implement RTCDtlsTransport which
  // has standardized method for getting this information.
  // See https://www.w3.org/TR/webrtc/#rtcdtlstransport-interface
  std::unique_ptr<rtc::SSLCertificate> GetRemoteAudioSSLCertificate();

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
  void GetStats(RTCStatsCollectorCallback* callback) override;

  SignalingState signaling_state() override;

  IceConnectionState ice_connection_state() override;
  IceGatheringState ice_gathering_state() override;

  const SessionDescriptionInterface* local_description() const override;
  const SessionDescriptionInterface* remote_description() const override;
  const SessionDescriptionInterface* current_local_description() const override;
  const SessionDescriptionInterface* current_remote_description()
      const override;
  const SessionDescriptionInterface* pending_local_description() const override;
  const SessionDescriptionInterface* pending_remote_description()
      const override;

  // JSEP01
  // Deprecated, use version without constraints.
  void CreateOffer(CreateSessionDescriptionObserver* observer,
                   const MediaConstraintsInterface* constraints) override;
  void CreateOffer(CreateSessionDescriptionObserver* observer,
                   const RTCOfferAnswerOptions& options) override;
  // Deprecated, use version without constraints.
  void CreateAnswer(CreateSessionDescriptionObserver* observer,
                    const MediaConstraintsInterface* constraints) override;
  void CreateAnswer(CreateSessionDescriptionObserver* observer,
                    const RTCOfferAnswerOptions& options) override;
  void SetLocalDescription(SetSessionDescriptionObserver* observer,
                           SessionDescriptionInterface* desc) override;
  void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                            SessionDescriptionInterface* desc) override;
  PeerConnectionInterface::RTCConfiguration GetConfiguration() override;
  bool SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      RTCError* error) override;
  bool SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration& configuration) override {
    return SetConfiguration(configuration, nullptr);
  }
  bool AddIceCandidate(const IceCandidateInterface* candidate) override;
  bool RemoveIceCandidates(
      const std::vector<cricket::Candidate>& candidates) override;

  void RegisterUMAObserver(UMAObserver* observer) override;

  RTCError SetBitrate(const BitrateParameters& bitrate) override;

  void SetBitrateAllocationStrategy(
      std::unique_ptr<rtc::BitrateAllocationStrategy>
          bitrate_allocation_strategy) override;

  void SetAudioPlayout(bool playout) override;
  void SetAudioRecording(bool recording) override;

  RTC_DEPRECATED bool StartRtcEventLog(rtc::PlatformFile file,
                                       int64_t max_size_bytes) override;
  bool StartRtcEventLog(std::unique_ptr<RtcEventLogOutput> output) override;
  void StopRtcEventLog() override;

  void Close() override;

  sigslot::signal1<DataChannel*> SignalDataChannelCreated;

  // Virtual for unit tests.
  virtual const std::vector<rtc::scoped_refptr<DataChannel>>&
  sctp_data_channels() const {
    return sctp_data_channels_;
  }

  // TODO(steveanton): These methods are temporarily added to facilitate work
  // towards merging WebRtcSession into PeerConnection. To make this easier, we
  // want only PeerConnection to interact with WebRtcSession so they can be
  // merged easily. A few outside classes still access WebRtcSession methods
  // directly, so these have been added to PeerConnection to remove the
  // dependency from WebRtcSession.

  rtc::Thread* network_thread() const { return factory_->network_thread(); }
  rtc::Thread* worker_thread() const { return factory_->worker_thread(); }
  rtc::Thread* signaling_thread() const { return factory_->signaling_thread(); }
  virtual const std::string& session_id() const {
    return session_->session_id();
  }
  virtual bool session_created() const { return session_ != nullptr; }
  virtual bool initial_offerer() const { return session_->initial_offerer(); }
  virtual std::unique_ptr<SessionStats> GetSessionStats_s() {
    return session_->GetSessionStats_s();
  }
  virtual std::unique_ptr<SessionStats> GetSessionStats(
      const ChannelNamePairs& channel_name_pairs) {
    return session_->GetSessionStats(channel_name_pairs);
  }
  virtual bool GetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
    return session_->GetLocalCertificate(transport_name, certificate);
  }
  virtual std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate(
      const std::string& transport_name) {
    return session_->GetRemoteSSLCertificate(transport_name);
  }
  virtual Call::Stats GetCallStats() { return session_->GetCallStats(); }
  virtual cricket::VoiceChannel* voice_channel() {
    return session_->voice_channel();
  }
  virtual cricket::VideoChannel* video_channel() {
    return session_->video_channel();
  }
  virtual cricket::RtpDataChannel* rtp_data_channel() {
    return session_->rtp_data_channel();
  }
  virtual rtc::Optional<std::string> sctp_content_name() const {
    return session_->sctp_content_name();
  }
  virtual rtc::Optional<std::string> sctp_transport_name() const {
    return session_->sctp_transport_name();
  }
  virtual bool GetLocalTrackIdBySsrc(uint32_t ssrc, std::string* track_id) {
    return session_->GetLocalTrackIdBySsrc(ssrc, track_id);
  }
  virtual bool GetRemoteTrackIdBySsrc(uint32_t ssrc, std::string* track_id) {
    return session_->GetRemoteTrackIdBySsrc(ssrc, track_id);
  }
  bool IceRestartPending(const std::string& content_name) const {
    return session_->IceRestartPending(content_name);
  }
  bool NeedsIceRestart(const std::string& content_name) const {
    return session_->NeedsIceRestart(content_name);
  }
  bool GetSslRole(const std::string& content_name, rtc::SSLRole* role) {
    return session_->GetSslRole(content_name, role);
  }

  // This is needed for stats tests to inject a MockWebRtcSession. Once
  // WebRtcSession has been merged in, this will no longer be needed.
  void set_session_for_testing(WebRtcSession* session) {
    session_ = session;
  }

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
                           const std::string& track_id,
                           uint32_t ssrc);

  void CreateVideoReceiver(MediaStreamInterface* stream,
                           const std::string& track_id,
                           uint32_t ssrc);
  rtc::scoped_refptr<RtpReceiverInterface> RemoveAndStopReceiver(
      const std::string& track_id);

  // May be called either by AddStream/RemoveStream, or when a track is
  // added/removed from a stream previously added via AddStream.
  void AddAudioTrack(AudioTrackInterface* track, MediaStreamInterface* stream);
  void RemoveAudioTrack(AudioTrackInterface* track,
                        MediaStreamInterface* stream);
  void AddVideoTrack(VideoTrackInterface* track, MediaStreamInterface* stream);
  void RemoveVideoTrack(VideoTrackInterface* track,
                        MediaStreamInterface* stream);

  void SetIceConnectionState(IceConnectionState new_state);
  // Called any time the IceGatheringState changes
  void OnIceGatheringChange(IceGatheringState new_state);
  // New ICE candidate has been gathered.
  void OnIceCandidate(std::unique_ptr<IceCandidateInterface> candidate);
  // Some local ICE candidates have been removed.
  void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates);

  // Update the state, signaling if necessary.
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
  void GetOptionsForOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
      cricket::MediaSessionOptions* session_options);

  // Returns a MediaSessionOptions struct with options decided by
  // |constraints|, the local MediaStreams and DataChannels.
  void GetOptionsForAnswer(const RTCOfferAnswerOptions& options,
                           cricket::MediaSessionOptions* session_options);

  // Generates MediaDescriptionOptions for the |session_opts| based on existing
  // local description or remote description.
  void GenerateMediaDescriptionOptions(
      const SessionDescriptionInterface* session_desc,
      cricket::RtpTransceiverDirection audio_direction,
      cricket::RtpTransceiverDirection video_direction,
      rtc::Optional<size_t>* audio_index,
      rtc::Optional<size_t>* video_index,
      rtc::Optional<size_t>* data_index,
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

  // Called when voice_channel_, video_channel_ and
  // rtp_data_channel_/sctp_transport_ are created and destroyed. As a result
  // of, for example, setting a new description.
  void OnVoiceChannelCreated();
  void OnVoiceChannelDestroyed();
  void OnVideoChannelCreated();
  void OnVideoChannelDestroyed();
  void OnDataChannelCreated();
  void OnDataChannelDestroyed();
  // Called when a valid data channel OPEN message is received.
  void OnDataChannelOpenMessage(const std::string& label,
                                const InternalDataChannelInit& config);

  bool HasRtpSender(cricket::MediaType type) const;
  RtpSenderInternal* FindSenderById(const std::string& id);

  std::vector<rtc::scoped_refptr<
      RtpSenderProxyWithInternal<RtpSenderInternal>>>::iterator
  FindSenderForTrack(MediaStreamTrackInterface* track);
  std::vector<rtc::scoped_refptr<
      RtpReceiverProxyWithInternal<RtpReceiverInternal>>>::iterator
  FindReceiverForTrack(const std::string& track_id);

  TrackInfos* GetRemoteTracks(cricket::MediaType media_type);
  TrackInfos* GetLocalTracks(cricket::MediaType media_type);
  const TrackInfo* FindTrackInfo(const TrackInfos& infos,
                                 const std::string& stream_label,
                                 const std::string track_id) const;

  // Returns the specified SCTP DataChannel in sctp_data_channels_,
  // or nullptr if not found.
  DataChannel* FindDataChannelBySid(int sid) const;

  // Called when first configuring the port allocator.
  bool InitializePortAllocator_n(const RTCConfiguration& configuration);
  // Called when SetConfiguration is called to apply the supported subset
  // of the configuration on the network thread.
  bool ReconfigurePortAllocator_n(
      const cricket::ServerAddresses& stun_servers,
      const std::vector<cricket::RelayServerConfig>& turn_servers,
      IceTransportsType type,
      int candidate_pool_size,
      bool prune_turn_ports,
      webrtc::TurnCustomizer* turn_customizer);

  // Starts output of an RTC event log to the given output object.
  // This function should only be called from the worker thread.
  bool StartRtcEventLog_w(std::unique_ptr<RtcEventLogOutput> output);

  // Stops recording an RTC event log.
  // This function should only be called from the worker thread.
  void StopRtcEventLog_w();

  // Ensures the configuration doesn't have any parameters with invalid values,
  // or values that conflict with other parameters.
  //
  // Returns RTCError::OK() if there are no issues.
  RTCError ValidateConfiguration(const RTCConfiguration& config) const;

  cricket::ChannelManager* channel_manager() const;
  MetricsObserverInterface* metrics_observer() const;

  // Storing the factory as a scoped reference pointer ensures that the memory
  // in the PeerConnectionFactoryImpl remains available as long as the
  // PeerConnection is running. It is passed to PeerConnection as a raw pointer.
  // However, since the reference counting is done in the
  // PeerConnectionFactoryInterface all instances created using the raw pointer
  // will refer to the same reference count.
  rtc::scoped_refptr<PeerConnectionFactory> factory_;
  PeerConnectionObserver* observer_ = nullptr;
  UMAObserver* uma_observer_ = nullptr;

  // The EventLog needs to outlive |call_| (and any other object that uses it).
  std::unique_ptr<RtcEventLog> event_log_;

  SignalingState signaling_state_ = kStable;
  IceConnectionState ice_connection_state_ = kIceConnectionNew;
  IceGatheringState ice_gathering_state_ = kIceGatheringNew;
  PeerConnectionInterface::RTCConfiguration configuration_;

  std::unique_ptr<cricket::PortAllocator> port_allocator_;

  // One PeerConnection has only one RTCP CNAME.
  // https://tools.ietf.org/html/draft-ietf-rtcweb-rtp-usage-26#section-4.9
  std::string rtcp_cname_;

  // Streams added via AddStream.
  rtc::scoped_refptr<StreamCollection> local_streams_;
  // Streams created as a result of SetRemoteDescription.
  rtc::scoped_refptr<StreamCollection> remote_streams_;

  std::vector<std::unique_ptr<MediaStreamObserver>> stream_observers_;

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

  std::unique_ptr<Call> call_;
  WebRtcSession* session_ = nullptr;
  std::unique_ptr<WebRtcSession> owned_session_;
  std::unique_ptr<StatsCollector> stats_;  // A pointer is passed to senders_
  rtc::scoped_refptr<RTCStatsCollector> stats_collector_;

  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
      senders_;
  std::vector<
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
      receivers_;
};

}  // namespace webrtc

#endif  // PC_PEERCONNECTION_H_
