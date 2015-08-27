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

#ifndef TALK_APP_WEBRTC_WEBRTCSESSION_H_
#define TALK_APP_WEBRTC_WEBRTCSESSION_H_

#include <string>

#include "talk/app/webrtc/datachannel.h"
#include "talk/app/webrtc/dtmfsender.h"
#include "talk/app/webrtc/mediastreamprovider.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/statstypes.h"
#include "talk/media/base/mediachannel.h"
#include "webrtc/p2p/base/session.h"
#include "talk/session/media/mediasession.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sslidentity.h"
#include "webrtc/base/thread.h"

namespace cricket {

class BaseChannel;
class ChannelManager;
class DataChannel;
class StatsReport;
class Transport;
class VideoCapturer;
class VideoChannel;
class VoiceChannel;

}  // namespace cricket

namespace webrtc {

class IceRestartAnswerLatch;
class JsepIceCandidate;
class MediaStreamSignaling;
class WebRtcSessionDescriptionFactory;

extern const char kBundleWithoutRtcpMux[];
extern const char kCreateChannelFailed[];
extern const char kInvalidCandidates[];
extern const char kInvalidSdp[];
extern const char kMlineMismatch[];
extern const char kPushDownTDFailed[];
extern const char kSdpWithoutDtlsFingerprint[];
extern const char kSdpWithoutSdesCrypto[];
extern const char kSdpWithoutIceUfragPwd[];
extern const char kSdpWithoutSdesAndDtlsDisabled[];
extern const char kSessionError[];
extern const char kSessionErrorDesc[];
extern const char kDtlsSetupFailureRtp[];
extern const char kDtlsSetupFailureRtcp[];
// Maximum number of received video streams that will be processed by webrtc
// even if they are not signalled beforehand.
extern const int kMaxUnsignalledRecvStreams;

// ICE state callback interface.
class IceObserver {
 public:
  IceObserver() {}
  // Called any time the IceConnectionState changes
  // TODO(honghaiz): Change the name to OnIceConnectionStateChange so as to
  // conform to the w3c standard.
  virtual void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) {}
  // Called any time the IceGatheringState changes
  virtual void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) {}
  // New Ice candidate have been found.
  virtual void OnIceCandidate(const IceCandidateInterface* candidate) = 0;
  // All Ice candidates have been found.
  // TODO(bemasc): Remove this once callers transition to OnIceGatheringChange.
  // (via PeerConnectionObserver)
  virtual void OnIceComplete() {}

  // Called whenever the state changes between receiving and not receiving.
  virtual void OnIceConnectionReceivingChange(bool receiving) {}

 protected:
  ~IceObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IceObserver);
};

class WebRtcSession : public cricket::BaseSession,
                      public AudioProviderInterface,
                      public DataChannelFactory,
                      public VideoProviderInterface,
                      public DtmfProviderInterface,
                      public DataChannelProviderInterface {
 public:
  WebRtcSession(cricket::ChannelManager* channel_manager,
                rtc::Thread* signaling_thread,
                rtc::Thread* worker_thread,
                cricket::PortAllocator* port_allocator,
                MediaStreamSignaling* mediastream_signaling);
  virtual ~WebRtcSession();

  bool Initialize(
      const PeerConnectionFactoryInterface::Options& options,
      const MediaConstraintsInterface* constraints,
      rtc::scoped_ptr<DtlsIdentityStoreInterface> dtls_identity_store,
      const PeerConnectionInterface::RTCConfiguration& rtc_configuration);
  // Deletes the voice, video and data channel and changes the session state
  // to STATE_RECEIVEDTERMINATE.
  void Terminate();

  void RegisterIceObserver(IceObserver* observer) {
    ice_observer_ = observer;
  }

  virtual cricket::VoiceChannel* voice_channel() {
    return voice_channel_.get();
  }
  virtual cricket::VideoChannel* video_channel() {
    return video_channel_.get();
  }
  virtual cricket::DataChannel* data_channel() {
    return data_channel_.get();
  }

  virtual const MediaStreamSignaling* mediastream_signaling() const {
    return mediastream_signaling_;
  }

  void SetSdesPolicy(cricket::SecurePolicy secure_policy);
  cricket::SecurePolicy SdesPolicy() const;

  // Get current ssl role from transport.
  bool GetSslRole(rtc::SSLRole* role);

  // Generic error message callback from WebRtcSession.
  // TODO - It may be necessary to supply error code as well.
  sigslot::signal0<> SignalError;

  void CreateOffer(
      CreateSessionDescriptionObserver* observer,
      const PeerConnectionInterface::RTCOfferAnswerOptions& options);
  void CreateAnswer(CreateSessionDescriptionObserver* observer,
                    const MediaConstraintsInterface* constraints);
  // The ownership of |desc| will be transferred after this call.
  bool SetLocalDescription(SessionDescriptionInterface* desc,
                           std::string* err_desc);
  // The ownership of |desc| will be transferred after this call.
  bool SetRemoteDescription(SessionDescriptionInterface* desc,
                            std::string* err_desc);
  bool ProcessIceMessage(const IceCandidateInterface* ice_candidate);

  bool SetIceTransports(PeerConnectionInterface::IceTransportsType type);

  const SessionDescriptionInterface* local_description() const {
    return local_desc_.get();
  }
  const SessionDescriptionInterface* remote_description() const {
    return remote_desc_.get();
  }
  // TODO(pthatcher): Cleanup the distinction between
  // SessionDescription and SessionDescriptionInterface and remove
  // these if possible.
  const cricket::SessionDescription* base_local_description() const {
    return BaseSession::local_description();
  }
  const cricket::SessionDescription* base_remote_description() const {
    return BaseSession::remote_description();
  }

  // Get the id used as a media stream track's "id" field from ssrc.
  virtual bool GetLocalTrackIdBySsrc(uint32 ssrc, std::string* track_id);
  virtual bool GetRemoteTrackIdBySsrc(uint32 ssrc, std::string* track_id);


  // AudioMediaProviderInterface implementation.
  void SetAudioPlayout(uint32 ssrc,
                       bool enable,
                       cricket::AudioRenderer* renderer) override;
  void SetAudioSend(uint32 ssrc,
                    bool enable,
                    const cricket::AudioOptions& options,
                    cricket::AudioRenderer* renderer) override;
  void SetAudioPlayoutVolume(uint32 ssrc, double volume) override;

  // Implements VideoMediaProviderInterface.
  bool SetCaptureDevice(uint32 ssrc, cricket::VideoCapturer* camera) override;
  void SetVideoPlayout(uint32 ssrc,
                       bool enable,
                       cricket::VideoRenderer* renderer) override;
  void SetVideoSend(uint32 ssrc,
                    bool enable,
                    const cricket::VideoOptions* options) override;

  // Implements DtmfProviderInterface.
  virtual bool CanInsertDtmf(const std::string& track_id);
  virtual bool InsertDtmf(const std::string& track_id,
                          int code, int duration);
  virtual sigslot::signal0<>* GetOnDestroyedSignal();

  // Implements DataChannelProviderInterface.
  bool SendData(const cricket::SendDataParams& params,
                const rtc::Buffer& payload,
                cricket::SendDataResult* result) override;
  bool ConnectDataChannel(DataChannel* webrtc_data_channel) override;
  void DisconnectDataChannel(DataChannel* webrtc_data_channel) override;
  void AddSctpDataStream(int sid) override;
  void RemoveSctpDataStream(int sid) override;
  bool ReadyToSendData() const override;

  // Returns stats for all channels of all transports.
  // This avoids exposing the internal structures used to track them.
  virtual bool GetTransportStats(cricket::SessionStats* stats);

  // Implements DataChannelFactory.
  rtc::scoped_refptr<DataChannel> CreateDataChannel(
      const std::string& label,
      const InternalDataChannelInit* config) override;

  cricket::DataChannelType data_channel_type() const;

  bool IceRestartPending() const;

  void ResetIceRestartLatch();

  // Called when an RTCCertificate is generated or retrieved by
  // WebRTCSessionDescriptionFactory. Should happen before setLocalDescription.
  void OnCertificateReady(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  void OnDtlsSetupFailure(cricket::BaseChannel*, bool rtcp);

  // For unit test.
  bool waiting_for_certificate_for_testing() const;

  void set_metrics_observer(
      webrtc::MetricsObserverInterface* metrics_observer) {
    metrics_observer_ = metrics_observer;
  }

 private:
  // Indicates the type of SessionDescription in a call to SetLocalDescription
  // and SetRemoteDescription.
  enum Action {
    kOffer,
    kPrAnswer,
    kAnswer,
  };

  // Invokes ConnectChannels() on transport proxies, which initiates ice
  // candidates allocation.
  bool StartCandidatesAllocation();
  bool UpdateSessionState(Action action, cricket::ContentSource source,
                          std::string* err_desc);
  static Action GetAction(const std::string& type);
  // Push the media parts of the local or remote session description
  // down to all of the channels.
  bool PushdownMediaDescription(cricket::ContentAction action,
                                cricket::ContentSource source,
                                std::string* error_desc);


  // Transport related callbacks, override from cricket::BaseSession.
  virtual void OnTransportRequestSignaling(cricket::Transport* transport);
  virtual void OnTransportConnecting(cricket::Transport* transport);
  virtual void OnTransportWritable(cricket::Transport* transport);
  virtual void OnTransportCompleted(cricket::Transport* transport);
  virtual void OnTransportFailed(cricket::Transport* transport);
  virtual void OnTransportProxyCandidatesReady(
      cricket::TransportProxy* proxy,
      const cricket::Candidates& candidates);
  virtual void OnCandidatesAllocationDone();
  void OnTransportReceiving(cricket::Transport* transport) override;

  // Enables media channels to allow sending of media.
  void EnableChannels();
  // Creates a JsepIceCandidate and adds it to the local session description
  // and notify observers. Called when a new local candidate have been found.
  void ProcessNewLocalCandidate(const std::string& content_name,
                                const cricket::Candidates& candidates);
  // Returns the media index for a local ice candidate given the content name.
  // Returns false if the local session description does not have a media
  // content called  |content_name|.
  bool GetLocalCandidateMediaIndex(const std::string& content_name,
                                   int* sdp_mline_index);
  // Uses all remote candidates in |remote_desc| in this session.
  bool UseCandidatesInSessionDescription(
      const SessionDescriptionInterface* remote_desc);
  // Uses |candidate| in this session.
  bool UseCandidate(const IceCandidateInterface* candidate);
  // Deletes the corresponding channel of contents that don't exist in |desc|.
  // |desc| can be null. This means that all channels are deleted.
  void RemoveUnusedChannelsAndTransports(
      const cricket::SessionDescription* desc);

  // Allocates media channels based on the |desc|. If |desc| doesn't have
  // the BUNDLE option, this method will disable BUNDLE in PortAllocator.
  // This method will also delete any existing media channels before creating.
  bool CreateChannels(const cricket::SessionDescription* desc);

  // Helper methods to create media channels.
  bool CreateVoiceChannel(const cricket::ContentInfo* content);
  bool CreateVideoChannel(const cricket::ContentInfo* content);
  bool CreateDataChannel(const cricket::ContentInfo* content);

  // Copy the candidates from |saved_candidates_| to |dest_desc|.
  // The |saved_candidates_| will be cleared after this function call.
  void CopySavedCandidates(SessionDescriptionInterface* dest_desc);

  // Listens to SCTP CONTROL messages on unused SIDs and process them as OPEN
  // messages.
  void OnDataChannelMessageReceived(cricket::DataChannel* channel,
                                    const cricket::ReceiveDataParams& params,
                                    const rtc::Buffer& payload);

  std::string BadStateErrMsg(State state);
  void SetIceConnectionState(PeerConnectionInterface::IceConnectionState state);
  void SetIceConnectionReceiving(bool receiving);

  bool ValidateBundleSettings(const cricket::SessionDescription* desc);
  bool HasRtcpMuxEnabled(const cricket::ContentInfo* content);
  // Below methods are helper methods which verifies SDP.
  bool ValidateSessionDescription(const SessionDescriptionInterface* sdesc,
                                  cricket::ContentSource source,
                                  std::string* err_desc);

  // Check if a call to SetLocalDescription is acceptable with |action|.
  bool ExpectSetLocalDescription(Action action);
  // Check if a call to SetRemoteDescription is acceptable with |action|.
  bool ExpectSetRemoteDescription(Action action);
  // Verifies a=setup attribute as per RFC 5763.
  bool ValidateDtlsSetupAttribute(const cricket::SessionDescription* desc,
                                  Action action);

  // Returns true if we are ready to push down the remote candidate.
  // |remote_desc| is the new remote description, or NULL if the current remote
  // description should be used. Output |valid| is true if the candidate media
  // index is valid.
  bool ReadyToUseRemoteCandidate(const IceCandidateInterface* candidate,
                                 const SessionDescriptionInterface* remote_desc,
                                 bool* valid);

  std::string GetSessionErrorMsg();

  // Invoked when OnTransportCompleted is signaled to gather the usage
  // of IPv4/IPv6 as best connection.
  void ReportBestConnectionState(const cricket::TransportStats& stats);

  void ReportNegotiatedCiphers(const cricket::TransportStats& stats);

  rtc::scoped_ptr<cricket::VoiceChannel> voice_channel_;
  rtc::scoped_ptr<cricket::VideoChannel> video_channel_;
  rtc::scoped_ptr<cricket::DataChannel> data_channel_;
  cricket::ChannelManager* channel_manager_;
  MediaStreamSignaling* mediastream_signaling_;
  IceObserver* ice_observer_;
  PeerConnectionInterface::IceConnectionState ice_connection_state_;
  bool ice_connection_receiving_;
  rtc::scoped_ptr<SessionDescriptionInterface> local_desc_;
  rtc::scoped_ptr<SessionDescriptionInterface> remote_desc_;
  // Candidates that arrived before the remote description was set.
  std::vector<IceCandidateInterface*> saved_candidates_;
  // If the remote peer is using a older version of implementation.
  bool older_version_remote_peer_;
  bool dtls_enabled_;
  // Specifies which kind of data channel is allowed. This is controlled
  // by the chrome command-line flag and constraints:
  // 1. If chrome command-line switch 'enable-sctp-data-channels' is enabled,
  // constraint kEnableDtlsSrtp is true, and constaint kEnableRtpDataChannels is
  // not set or false, SCTP is allowed (DCT_SCTP);
  // 2. If constraint kEnableRtpDataChannels is true, RTP is allowed (DCT_RTP);
  // 3. If both 1&2 are false, data channel is not allowed (DCT_NONE).
  cricket::DataChannelType data_channel_type_;
  rtc::scoped_ptr<IceRestartAnswerLatch> ice_restart_latch_;

  rtc::scoped_ptr<WebRtcSessionDescriptionFactory>
      webrtc_session_desc_factory_;

  sigslot::signal0<> SignalVoiceChannelDestroyed;
  sigslot::signal0<> SignalVideoChannelDestroyed;
  sigslot::signal0<> SignalDataChannelDestroyed;

  // Member variables for caching global options.
  cricket::AudioOptions audio_options_;
  cricket::VideoOptions video_options_;
  MetricsObserverInterface* metrics_observer_;

  // Declares the bundle policy for the WebRTCSession.
  PeerConnectionInterface::BundlePolicy bundle_policy_;

  // Declares the RTCP mux policy for the WebRTCSession.
  PeerConnectionInterface::RtcpMuxPolicy rtcp_mux_policy_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcSession);
};
}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_WEBRTCSESSION_H_
