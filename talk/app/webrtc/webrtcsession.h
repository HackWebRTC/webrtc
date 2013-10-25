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

#ifndef TALK_APP_WEBRTC_WEBRTCSESSION_H_
#define TALK_APP_WEBRTC_WEBRTCSESSION_H_

#include <string>

#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/dtmfsender.h"
#include "talk/app/webrtc/mediastreamprovider.h"
#include "talk/app/webrtc/datachannel.h"
#include "talk/app/webrtc/statstypes.h"
#include "talk/base/sigslot.h"
#include "talk/base/thread.h"
#include "talk/media/base/mediachannel.h"
#include "talk/p2p/base/session.h"
#include "talk/session/media/mediasession.h"

namespace cricket {

class ChannelManager;
class DataChannel;
class StatsReport;
class Transport;
class VideoCapturer;
class BaseChannel;
class VideoChannel;
class VoiceChannel;

}  // namespace cricket

namespace webrtc {

class IceRestartAnswerLatch;
class MediaStreamSignaling;
class WebRtcSessionDescriptionFactory;

extern const char kSetLocalSdpFailed[];
extern const char kSetRemoteSdpFailed[];
extern const char kCreateChannelFailed[];
extern const char kBundleWithoutRtcpMux[];
extern const char kInvalidCandidates[];
extern const char kInvalidSdp[];
extern const char kMlineMismatch[];
extern const char kSdpWithoutCrypto[];
extern const char kSdpWithoutSdesAndDtlsDisabled[];
extern const char kSdpWithoutIceUfragPwd[];
extern const char kSessionError[];
extern const char kUpdateStateFailed[];
extern const char kPushDownOfferTDFailed[];
extern const char kPushDownPranswerTDFailed[];
extern const char kPushDownAnswerTDFailed[];

// ICE state callback interface.
class IceObserver {
 public:
  // Called any time the IceConnectionState changes
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

 protected:
  ~IceObserver() {}
};

class WebRtcSession : public cricket::BaseSession,
                      public AudioProviderInterface,
                      public DataChannelFactory,
                      public VideoProviderInterface,
                      public DtmfProviderInterface,
                      public DataChannelProviderInterface {
 public:
  WebRtcSession(cricket::ChannelManager* channel_manager,
                talk_base::Thread* signaling_thread,
                talk_base::Thread* worker_thread,
                cricket::PortAllocator* port_allocator,
                MediaStreamSignaling* mediastream_signaling);
  virtual ~WebRtcSession();

  bool Initialize(const PeerConnectionFactoryInterface::Options& options,
                  const MediaConstraintsInterface* constraints,
                  DTLSIdentityServiceInterface* dtls_identity_service);
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

  void set_secure_policy(cricket::SecureMediaPolicy secure_policy);
  cricket::SecureMediaPolicy secure_policy() const;

  // Get current ssl role from transport.
  bool GetSslRole(talk_base::SSLRole* role);

  // Generic error message callback from WebRtcSession.
  // TODO - It may be necessary to supply error code as well.
  sigslot::signal0<> SignalError;

  void CreateOffer(CreateSessionDescriptionObserver* observer,
                   const MediaConstraintsInterface* constraints);
  void CreateAnswer(CreateSessionDescriptionObserver* observer,
                    const MediaConstraintsInterface* constraints);
  // The ownership of |desc| will be transferred after this call.
  bool SetLocalDescription(SessionDescriptionInterface* desc,
                           std::string* err_desc);
  // The ownership of |desc| will be transferred after this call.
  bool SetRemoteDescription(SessionDescriptionInterface* desc,
                            std::string* err_desc);
  bool ProcessIceMessage(const IceCandidateInterface* ice_candidate);
  const SessionDescriptionInterface* local_description() const {
    return local_desc_.get();
  }
  const SessionDescriptionInterface* remote_description() const {
    return remote_desc_.get();
  }

  // Get the id used as a media stream track's "id" field from ssrc.
  virtual bool GetTrackIdBySsrc(uint32 ssrc, std::string* id);

  // AudioMediaProviderInterface implementation.
  virtual void SetAudioPlayout(uint32 ssrc, bool enable,
                               cricket::AudioRenderer* renderer) OVERRIDE;
  virtual void SetAudioSend(uint32 ssrc, bool enable,
                            const cricket::AudioOptions& options,
                            cricket::AudioRenderer* renderer) OVERRIDE;

  // Implements VideoMediaProviderInterface.
  virtual bool SetCaptureDevice(uint32 ssrc,
                                cricket::VideoCapturer* camera) OVERRIDE;
  virtual void SetVideoPlayout(uint32 ssrc,
                               bool enable,
                               cricket::VideoRenderer* renderer) OVERRIDE;
  virtual void SetVideoSend(uint32 ssrc, bool enable,
                            const cricket::VideoOptions* options) OVERRIDE;

  // Implements DtmfProviderInterface.
  virtual bool CanInsertDtmf(const std::string& track_id);
  virtual bool InsertDtmf(const std::string& track_id,
                          int code, int duration);
  virtual sigslot::signal0<>* GetOnDestroyedSignal();

  // Implements DataChannelProviderInterface.
  virtual bool SendData(const cricket::SendDataParams& params,
                        const talk_base::Buffer& payload,
                        cricket::SendDataResult* result) OVERRIDE;
  virtual bool ConnectDataChannel(DataChannel* webrtc_data_channel) OVERRIDE;
  virtual void DisconnectDataChannel(DataChannel* webrtc_data_channel) OVERRIDE;


  talk_base::scoped_refptr<DataChannel> CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config);

  cricket::DataChannelType data_channel_type() const;

  bool IceRestartPending() const;

  void ResetIceRestartLatch();

  // Called when an SSLIdentity is generated or retrieved by
  // WebRTCSessionDescriptionFactory. Should happen before setLocalDescription.
  void OnIdentityReady(talk_base::SSLIdentity* identity);

  // For unit test.
  bool waiting_for_identity() const;

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
                          const cricket::SessionDescription* desc,
                          std::string* err_desc);
  static Action GetAction(const std::string& type);

  // Transport related callbacks, override from cricket::BaseSession.
  virtual void OnTransportRequestSignaling(cricket::Transport* transport);
  virtual void OnTransportConnecting(cricket::Transport* transport);
  virtual void OnTransportWritable(cricket::Transport* transport);
  virtual void OnTransportProxyCandidatesReady(
      cricket::TransportProxy* proxy,
      const cricket::Candidates& candidates);
  virtual void OnCandidatesAllocationDone();

  // Creates local session description with audio and video contents.
  bool CreateDefaultLocalDescription();
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

  void OnNewDataChannelReceived(const std::string& label,
                                const DataChannelInit& init);

  bool GetLocalTrackId(uint32 ssrc, std::string* track_id);
  bool GetRemoteTrackId(uint32 ssrc, std::string* track_id);

  std::string BadStateErrMsg(const std::string& type, State state);
  void SetIceConnectionState(PeerConnectionInterface::IceConnectionState state);

  bool ValidateBundleSettings(const cricket::SessionDescription* desc);
  bool HasRtcpMuxEnabled(const cricket::ContentInfo* content);
  // Below methods are helper methods which verifies SDP.
  bool ValidateSessionDescription(const SessionDescriptionInterface* sdesc,
                                  cricket::ContentSource source,
                                  std::string* error_desc);

  // Check if a call to SetLocalDescription is acceptable with |action|.
  bool ExpectSetLocalDescription(Action action);
  // Check if a call to SetRemoteDescription is acceptable with |action|.
  bool ExpectSetRemoteDescription(Action action);
  // Verifies a=setup attribute as per RFC 5763.
  bool ValidateDtlsSetupAttribute(const cricket::SessionDescription* desc,
                                  Action action);

  talk_base::scoped_ptr<cricket::VoiceChannel> voice_channel_;
  talk_base::scoped_ptr<cricket::VideoChannel> video_channel_;
  talk_base::scoped_ptr<cricket::DataChannel> data_channel_;
  cricket::ChannelManager* channel_manager_;
  MediaStreamSignaling* mediastream_signaling_;
  IceObserver* ice_observer_;
  PeerConnectionInterface::IceConnectionState ice_connection_state_;
  talk_base::scoped_ptr<SessionDescriptionInterface> local_desc_;
  talk_base::scoped_ptr<SessionDescriptionInterface> remote_desc_;
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
  talk_base::scoped_ptr<IceRestartAnswerLatch> ice_restart_latch_;

  talk_base::scoped_ptr<WebRtcSessionDescriptionFactory>
      webrtc_session_desc_factory_;

  sigslot::signal0<> SignalVoiceChannelDestroyed;
  sigslot::signal0<> SignalVideoChannelDestroyed;
  sigslot::signal0<> SignalDataChannelDestroyed;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_WEBRTCSESSION_H_
