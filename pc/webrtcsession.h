/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_WEBRTCSESSION_H_
#define PC_WEBRTCSESSION_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "api/candidate.h"
#include "api/optional.h"
#include "api/peerconnectioninterface.h"
#include "api/statstypes.h"
#include "call/call.h"
#include "pc/datachannel.h"
#include "pc/mediasession.h"
#include "pc/transportcontroller.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/sigslot.h"
#include "rtc_base/sslidentity.h"
#include "rtc_base/thread.h"

namespace cricket {

class ChannelManager;
class RtpDataChannel;
class SctpTransportInternal;
class SctpTransportInternalFactory;
class StatsReport;
class VideoChannel;
class VoiceChannel;

}  // namespace cricket

namespace webrtc {

class IceRestartAnswerLatch;
class JsepIceCandidate;
class MediaStreamSignaling;
class PeerConnection;
class RtcEventLog;
class WebRtcSessionDescriptionFactory;

// Statistics for all the transports of the session.
// TODO(pthatcher): Think of a better name for this.  We already have
// a TransportStats in transport.h.  Perhaps TransportsStats?
struct SessionStats {
  std::map<std::string, std::string> proxy_to_transport;
  std::map<std::string, cricket::TransportStats> transport_stats;
};

struct ChannelNamePair {
  ChannelNamePair(
      const std::string& content_name, const std::string& transport_name)
      : content_name(content_name), transport_name(transport_name) {}
  std::string content_name;
  std::string transport_name;
};

struct ChannelNamePairs {
  rtc::Optional<ChannelNamePair> voice;
  rtc::Optional<ChannelNamePair> video;
  rtc::Optional<ChannelNamePair> data;
};

// A WebRtcSession manages general session state. This includes negotiation
// of both the application-level and network-level protocols:  the former
// defines what will be sent and the latter defines how it will be sent.  Each
// network-level protocol is represented by a Transport object.  Each Transport
// participates in the network-level negotiation.  The individual streams of
// packets are represented by TransportChannels.  The application-level protocol
// is represented by SessionDecription objects.
class WebRtcSession :
    public DataChannelProviderInterface,
    public sigslot::has_slots<> {
 public:
  enum Error {
    ERROR_NONE = 0,       // no error
    ERROR_CONTENT = 1,    // channel errors in SetLocalContent/SetRemoteContent
    ERROR_TRANSPORT = 2,  // transport error of some kind
  };

  // |sctp_factory| may be null, in which case SCTP is treated as unsupported.
  WebRtcSession(
      PeerConnection* pc,  // TODO(steveanton): Temporary.
      std::unique_ptr<cricket::TransportController> transport_controller,
      std::unique_ptr<cricket::SctpTransportInternalFactory> sctp_factory);
  virtual ~WebRtcSession();

  // These are const to allow them to be called from const methods.
  rtc::Thread* network_thread() const;
  rtc::Thread* worker_thread() const;
  rtc::Thread* signaling_thread() const;

  // The ID of this session.
  const std::string& session_id() const { return session_id_; }

  void Initialize(
      const PeerConnectionFactoryInterface::Options& options,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      const PeerConnectionInterface::RTCConfiguration& rtc_configuration,
      PeerConnection* pc);
  // Deletes the voice, video and data channel and changes the session state
  // to STATE_CLOSED.
  void Close();

  // Returns true if we were the initial offerer.
  bool initial_offerer() const { return initial_offerer_ && *initial_offerer_; }

  // Returns the last error in the session. See the enum above for details.
  Error error() const { return error_; }
  const std::string& error_desc() const { return error_desc_; }

  // Exposed for stats collecting.
  // TODO(steveanton): Switch callers to use the plural form and remove these.
  virtual cricket::VoiceChannel* voice_channel() {
    if (voice_channels_.empty()) {
      return nullptr;
    } else {
      return voice_channels_[0];
    }
  }
  virtual cricket::VideoChannel* video_channel() {
    if (video_channels_.empty()) {
      return nullptr;
    } else {
      return video_channels_[0];
    }
  }

  virtual std::vector<cricket::VoiceChannel*> voice_channels() const {
    return voice_channels_;
  }
  virtual std::vector<cricket::VideoChannel*> video_channels() const {
    return video_channels_;
  }

  // Only valid when using deprecated RTP data channels.
  virtual cricket::RtpDataChannel* rtp_data_channel() {
    return rtp_data_channel_;
  }
  virtual rtc::Optional<std::string> sctp_content_name() const {
    return sctp_content_name_;
  }
  virtual rtc::Optional<std::string> sctp_transport_name() const {
    return sctp_transport_name_;
  }

  cricket::BaseChannel* GetChannel(const std::string& content_name);

  // Get current SSL role used by SCTP's underlying transport.
  bool GetSctpSslRole(rtc::SSLRole* role);
  // Get SSL role for an arbitrary m= section (handles bundling correctly).
  // TODO(deadbeef): This is only used internally by the session description
  // factory, it shouldn't really be public).
  bool GetSslRole(const std::string& content_name, rtc::SSLRole* role);

  void CreateOffer(
      CreateSessionDescriptionObserver* observer,
      const PeerConnectionInterface::RTCOfferAnswerOptions& options,
      const cricket::MediaSessionOptions& session_options);
  void CreateAnswer(CreateSessionDescriptionObserver* observer,
                    const cricket::MediaSessionOptions& session_options);
  bool SetLocalDescription(std::unique_ptr<SessionDescriptionInterface> desc,
                           std::string* err_desc);
  bool SetRemoteDescription(std::unique_ptr<SessionDescriptionInterface> desc,
                            std::string* err_desc);

  bool ProcessIceMessage(const IceCandidateInterface* ice_candidate);

  bool RemoveRemoteIceCandidates(
      const std::vector<cricket::Candidate>& candidates);

  cricket::IceConfig ParseIceConfig(
      const PeerConnectionInterface::RTCConfiguration& config) const;

  void SetIceConfig(const cricket::IceConfig& ice_config);

  // Start gathering candidates for any new transports, or transports doing an
  // ICE restart.
  void MaybeStartGathering();

  const SessionDescriptionInterface* local_description() const {
    return pending_local_description_ ? pending_local_description_.get()
                                      : current_local_description_.get();
  }
  const SessionDescriptionInterface* remote_description() const {
    return pending_remote_description_ ? pending_remote_description_.get()
                                       : current_remote_description_.get();
  }
  const SessionDescriptionInterface* current_local_description() const {
    return current_local_description_.get();
  }
  const SessionDescriptionInterface* current_remote_description() const {
    return current_remote_description_.get();
  }
  const SessionDescriptionInterface* pending_local_description() const {
    return pending_local_description_.get();
  }
  const SessionDescriptionInterface* pending_remote_description() const {
    return pending_remote_description_.get();
  }

  // Get the id used as a media stream track's "id" field from ssrc.
  virtual bool GetLocalTrackIdBySsrc(uint32_t ssrc, std::string* track_id);
  virtual bool GetRemoteTrackIdBySsrc(uint32_t ssrc, std::string* track_id);

  // Implements DataChannelProviderInterface.
  bool SendData(const cricket::SendDataParams& params,
                const rtc::CopyOnWriteBuffer& payload,
                cricket::SendDataResult* result) override;
  bool ConnectDataChannel(DataChannel* webrtc_data_channel) override;
  void DisconnectDataChannel(DataChannel* webrtc_data_channel) override;
  void AddSctpDataStream(int sid) override;
  void RemoveSctpDataStream(int sid) override;
  bool ReadyToSendData() const override;

  virtual Call::Stats GetCallStats();

  // Returns stats for all channels of all transports.
  // This avoids exposing the internal structures used to track them.
  // The parameterless version creates |ChannelNamePairs| from |voice_channel|,
  // |video_channel| and |voice_channel| if available - this requires it to be
  // called on the signaling thread - and invokes the other |GetStats|. The
  // other |GetStats| can be invoked on any thread; if not invoked on the
  // network thread a thread hop will happen.
  std::unique_ptr<SessionStats> GetSessionStats_s();
  virtual std::unique_ptr<SessionStats> GetSessionStats(
      const ChannelNamePairs& channel_name_pairs);

  // virtual so it can be mocked in unit tests
  virtual bool GetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate);

  // Caller owns returned certificate
  virtual std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate(
      const std::string& transport_name);

  cricket::DataChannelType data_channel_type() const;

  // Returns true if there was an ICE restart initiated by the remote offer.
  bool IceRestartPending(const std::string& content_name) const;

  // Set the "needs-ice-restart" flag as described in JSEP. After the flag is
  // set, offers should generate new ufrags/passwords until an ICE restart
  // occurs.
  void SetNeedsIceRestartFlag();
  // Returns true if the ICE restart flag above was set, and no ICE restart has
  // occurred yet for this transport (by applying a local description with
  // changed ufrag/password). If the transport has been deleted as a result of
  // bundling, returns false.
  bool NeedsIceRestart(const std::string& content_name) const;

  // Called when an RTCCertificate is generated or retrieved by
  // WebRTCSessionDescriptionFactory. Should happen before setLocalDescription.
  void OnCertificateReady(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  void OnDtlsSrtpSetupFailure(cricket::BaseChannel*, bool rtcp);

  cricket::TransportController* transport_controller() const {
    return transport_controller_.get();
  }

 private:
  // Indicates the type of SessionDescription in a call to SetLocalDescription
  // and SetRemoteDescription.
  enum Action {
    kOffer,
    kPrAnswer,
    kAnswer,
  };

  // TODO(steveanton): Remove this once WebRtcSession and PeerConnection are
  // merged. This is so that we can eliminate signals from this class and
  // directly call the method in PeerConnection they are connected to.
  PeerConnection* pc_;

  // Return all managed, non-null channels.
  std::vector<cricket::BaseChannel*> Channels() const;

  // Non-const versions of local_description()/remote_description(), for use
  // internally.
  SessionDescriptionInterface* mutable_local_description() {
    return pending_local_description_ ? pending_local_description_.get()
                                      : current_local_description_.get();
  }
  SessionDescriptionInterface* mutable_remote_description() {
    return pending_remote_description_ ? pending_remote_description_.get()
                                       : current_remote_description_.get();
  }

  // Updates the error state, signaling if necessary.
  void SetError(Error error, const std::string& error_desc);

  bool UpdateSessionState(Action action, cricket::ContentSource source,
                          std::string* err_desc);
  Action GetAction(const std::string& type);
  // Push the media parts of the local or remote session description
  // down to all of the channels.
  bool PushdownMediaDescription(cricket::ContentAction action,
                                cricket::ContentSource source,
                                std::string* error_desc);
  bool PushdownSctpParameters_n(cricket::ContentSource source);

  bool PushdownTransportDescription(cricket::ContentSource source,
                                    cricket::ContentAction action,
                                    std::string* error_desc);

  // Helper methods to push local and remote transport descriptions.
  bool PushdownLocalTransportDescription(
      const cricket::SessionDescription* sdesc,
      cricket::ContentAction action,
      std::string* error_desc);
  bool PushdownRemoteTransportDescription(
      const cricket::SessionDescription* sdesc,
      cricket::ContentAction action,
      std::string* error_desc);

  // Returns true and the TransportInfo of the given |content_name|
  // from |description|. Returns false if it's not available.
  static bool GetTransportDescription(
      const cricket::SessionDescription* description,
      const std::string& content_name,
      cricket::TransportDescription* info);

  // Returns the name of the transport channel when BUNDLE is enabled, or
  // nullptr if the channel is not part of any bundle.
  const std::string* GetBundleTransportName(
      const cricket::ContentInfo* content,
      const cricket::ContentGroup* bundle);

  // Cause all the BaseChannels in the bundle group to have the same
  // transport channel.
  bool EnableBundle(const cricket::ContentGroup& bundle);

  // Enables media channels to allow sending of media.
  void EnableChannels();
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
  void RemoveUnusedChannels(const cricket::SessionDescription* desc);

  // Allocates media channels based on the |desc|. If |desc| doesn't have
  // the BUNDLE option, this method will disable BUNDLE in PortAllocator.
  // This method will also delete any existing media channels before creating.
  bool CreateChannels(const cricket::SessionDescription* desc);

  // Helper methods to create media channels.
  bool CreateVoiceChannel(const cricket::ContentInfo* content,
                          const std::string* bundle_transport);
  bool CreateVideoChannel(const cricket::ContentInfo* content,
                          const std::string* bundle_transport);
  bool CreateDataChannel(const cricket::ContentInfo* content,
                         const std::string* bundle_transport);

  std::unique_ptr<SessionStats> GetSessionStats_n(
      const ChannelNamePairs& channel_name_pairs);

  bool CreateSctpTransport_n(const std::string& content_name,
                             const std::string& transport_name);
  // For bundling.
  void ChangeSctpTransport_n(const std::string& transport_name);
  void DestroySctpTransport_n();
  // SctpTransport signal handlers. Needed to marshal signals from the network
  // to signaling thread.
  void OnSctpTransportReadyToSendData_n();
  // This may be called with "false" if the direction of the m= section causes
  // us to tear down the SCTP connection.
  void OnSctpTransportReadyToSendData_s(bool ready);
  void OnSctpTransportDataReceived_n(const cricket::ReceiveDataParams& params,
                                     const rtc::CopyOnWriteBuffer& payload);
  // Beyond just firing the signal to the signaling thread, listens to SCTP
  // CONTROL messages on unused SIDs and processes them as OPEN messages.
  void OnSctpTransportDataReceived_s(const cricket::ReceiveDataParams& params,
                                     const rtc::CopyOnWriteBuffer& payload);
  void OnSctpStreamClosedRemotely_n(int sid);

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

  // Returns true if SRTP (either using DTLS-SRTP or SDES) is required by
  // this session.
  bool SrtpRequired() const;

  // TransportController signal handlers.
  void OnTransportControllerConnectionState(cricket::IceConnectionState state);
  void OnTransportControllerGatheringState(cricket::IceGatheringState state);
  void OnTransportControllerCandidatesGathered(
      const std::string& transport_name,
      const std::vector<cricket::Candidate>& candidates);
  void OnTransportControllerCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates);
  void OnTransportControllerDtlsHandshakeError(rtc::SSLHandshakeError error);

  std::string GetSessionErrorMsg();

  // Invoked when TransportController connection completion is signaled.
  // Reports stats for all transports in use.
  void ReportTransportStats();

  // Gather the usage of IPv4/IPv6 as best connection.
  void ReportBestConnectionState(const cricket::TransportStats& stats);

  void ReportNegotiatedCiphers(const cricket::TransportStats& stats);

  void OnSentPacket_w(const rtc::SentPacket& sent_packet);

  const std::string GetTransportName(const std::string& content_name);

  void DestroyRtcpTransport_n(const std::string& transport_name);
  void RemoveAndDestroyVideoChannel(cricket::VideoChannel* video_channel);
  void DestroyVideoChannel(cricket::VideoChannel* video_channel);
  void RemoveAndDestroyVoiceChannel(cricket::VoiceChannel* voice_channel);
  void DestroyVoiceChannel(cricket::VoiceChannel* voice_channel);
  void DestroyDataChannel();

  Error error_ = ERROR_NONE;
  std::string error_desc_;

  const std::string session_id_;
  rtc::Optional<bool> initial_offerer_;

  const std::unique_ptr<cricket::TransportController> transport_controller_;
  const std::unique_ptr<cricket::SctpTransportInternalFactory> sctp_factory_;
  // TODO(steveanton): voice_channels_ and video_channels_ used to be a single
  // VoiceChannel/VideoChannel respectively but are being changed to support
  // multiple m= lines in unified plan. But until more work is done, these can
  // only have 0 or 1 channel each.
  // These channels are owned by ChannelManager.
  std::vector<cricket::VoiceChannel*> voice_channels_;
  std::vector<cricket::VideoChannel*> video_channels_;
  // |rtp_data_channel_| is used if in RTP data channel mode, |sctp_transport_|
  // when using SCTP.
  cricket::RtpDataChannel* rtp_data_channel_ = nullptr;

  std::unique_ptr<cricket::SctpTransportInternal> sctp_transport_;
  // |sctp_transport_name_| keeps track of what DTLS transport the SCTP
  // transport is using (which can change due to bundling).
  rtc::Optional<std::string> sctp_transport_name_;
  // |sctp_content_name_| is the content name (MID) in SDP.
  rtc::Optional<std::string> sctp_content_name_;
  // Value cached on signaling thread. Only updated when SctpReadyToSendData
  // fires on the signaling thread.
  bool sctp_ready_to_send_data_ = false;
  // Same as signals provided by SctpTransport, but these are guaranteed to
  // fire on the signaling thread, whereas SctpTransport fires on the networking
  // thread.
  // |sctp_invoker_| is used so that any signals queued on the signaling thread
  // from the network thread are immediately discarded if the SctpTransport is
  // destroyed (due to m= section being rejected).
  // TODO(deadbeef): Use a proxy object to ensure that method calls/signals
  // are marshalled to the right thread. Could almost use proxy.h for this,
  // but it doesn't have a mechanism for marshalling sigslot::signals
  std::unique_ptr<rtc::AsyncInvoker> sctp_invoker_;
  sigslot::signal1<bool> SignalSctpReadyToSendData;
  sigslot::signal2<const cricket::ReceiveDataParams&,
                   const rtc::CopyOnWriteBuffer&>
      SignalSctpDataReceived;
  sigslot::signal1<int> SignalSctpStreamClosedRemotely;

  std::unique_ptr<SessionDescriptionInterface> current_local_description_;
  std::unique_ptr<SessionDescriptionInterface> pending_local_description_;
  std::unique_ptr<SessionDescriptionInterface> current_remote_description_;
  std::unique_ptr<SessionDescriptionInterface> pending_remote_description_;
  bool dtls_enabled_;
  // Specifies which kind of data channel is allowed. This is controlled
  // by the chrome command-line flag and constraints:
  // 1. If chrome command-line switch 'enable-sctp-data-channels' is enabled,
  // constraint kEnableDtlsSrtp is true, and constaint kEnableRtpDataChannels is
  // not set or false, SCTP is allowed (DCT_SCTP);
  // 2. If constraint kEnableRtpDataChannels is true, RTP is allowed (DCT_RTP);
  // 3. If both 1&2 are false, data channel is not allowed (DCT_NONE).
  cricket::DataChannelType data_channel_type_;
  // List of content names for which the remote side triggered an ICE restart.
  std::set<std::string> pending_ice_restarts_;

  std::unique_ptr<WebRtcSessionDescriptionFactory> webrtc_session_desc_factory_;

  // Member variables for caching global options.
  cricket::AudioOptions audio_options_;
  cricket::VideoOptions video_options_;

  RTC_DISALLOW_COPY_AND_ASSIGN(WebRtcSession);
};
}  // namespace webrtc

#endif  // PC_WEBRTCSESSION_H_
