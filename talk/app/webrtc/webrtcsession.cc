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

#include "talk/app/webrtc/webrtcsession.h"

#include <limits.h>

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "talk/app/webrtc/jsepicecandidate.h"
#include "talk/app/webrtc/jsepsessiondescription.h"
#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/sctputils.h"
#include "talk/app/webrtc/webrtcsessiondescriptionfactory.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/videocapturer.h"
#include "talk/session/media/channel.h"
#include "talk/session/media/channelmanager.h"
#include "talk/session/media/mediasession.h"
#include "webrtc/audio/audio_sink.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/call.h"
#include "webrtc/p2p/base/portallocator.h"
#include "webrtc/p2p/base/transportchannel.h"

using cricket::ContentInfo;
using cricket::ContentInfos;
using cricket::MediaContentDescription;
using cricket::SessionDescription;
using cricket::TransportInfo;

using cricket::LOCAL_PORT_TYPE;
using cricket::STUN_PORT_TYPE;
using cricket::RELAY_PORT_TYPE;
using cricket::PRFLX_PORT_TYPE;

namespace webrtc {

// Error messages
const char kBundleWithoutRtcpMux[] = "RTCP-MUX must be enabled when BUNDLE "
                                     "is enabled.";
const char kCreateChannelFailed[] = "Failed to create channels.";
const char kInvalidCandidates[] = "Description contains invalid candidates.";
const char kInvalidSdp[] = "Invalid session description.";
const char kMlineMismatch[] =
    "Offer and answer descriptions m-lines are not matching. Rejecting answer.";
const char kPushDownTDFailed[] =
    "Failed to push down transport description:";
const char kSdpWithoutDtlsFingerprint[] =
    "Called with SDP without DTLS fingerprint.";
const char kSdpWithoutSdesCrypto[] =
    "Called with SDP without SDES crypto.";
const char kSdpWithoutIceUfragPwd[] =
    "Called with SDP without ice-ufrag and ice-pwd.";
const char kSessionError[] = "Session error code: ";
const char kSessionErrorDesc[] = "Session error description: ";
const char kDtlsSetupFailureRtp[] =
    "Couldn't set up DTLS-SRTP on RTP channel.";
const char kDtlsSetupFailureRtcp[] =
    "Couldn't set up DTLS-SRTP on RTCP channel.";
const char kEnableBundleFailed[] = "Failed to enable BUNDLE.";
const int kMaxUnsignalledRecvStreams = 20;

IceCandidatePairType GetIceCandidatePairCounter(
    const cricket::Candidate& local,
    const cricket::Candidate& remote) {
  const auto& l = local.type();
  const auto& r = remote.type();
  const auto& host = LOCAL_PORT_TYPE;
  const auto& srflx = STUN_PORT_TYPE;
  const auto& relay = RELAY_PORT_TYPE;
  const auto& prflx = PRFLX_PORT_TYPE;
  if (l == host && r == host) {
    bool local_private = IPIsPrivate(local.address().ipaddr());
    bool remote_private = IPIsPrivate(remote.address().ipaddr());
    if (local_private) {
      if (remote_private) {
        return kIceCandidatePairHostPrivateHostPrivate;
      } else {
        return kIceCandidatePairHostPrivateHostPublic;
      }
    } else {
      if (remote_private) {
        return kIceCandidatePairHostPublicHostPrivate;
      } else {
        return kIceCandidatePairHostPublicHostPublic;
      }
    }
  }
  if (l == host && r == srflx)
    return kIceCandidatePairHostSrflx;
  if (l == host && r == relay)
    return kIceCandidatePairHostRelay;
  if (l == host && r == prflx)
    return kIceCandidatePairHostPrflx;
  if (l == srflx && r == host)
    return kIceCandidatePairSrflxHost;
  if (l == srflx && r == srflx)
    return kIceCandidatePairSrflxSrflx;
  if (l == srflx && r == relay)
    return kIceCandidatePairSrflxRelay;
  if (l == srflx && r == prflx)
    return kIceCandidatePairSrflxPrflx;
  if (l == relay && r == host)
    return kIceCandidatePairRelayHost;
  if (l == relay && r == srflx)
    return kIceCandidatePairRelaySrflx;
  if (l == relay && r == relay)
    return kIceCandidatePairRelayRelay;
  if (l == relay && r == prflx)
    return kIceCandidatePairRelayPrflx;
  if (l == prflx && r == host)
    return kIceCandidatePairPrflxHost;
  if (l == prflx && r == srflx)
    return kIceCandidatePairPrflxSrflx;
  if (l == prflx && r == relay)
    return kIceCandidatePairPrflxRelay;
  return kIceCandidatePairMax;
}

// Compares |answer| against |offer|. Comparision is done
// for number of m-lines in answer against offer. If matches true will be
// returned otherwise false.
static bool VerifyMediaDescriptions(
    const SessionDescription* answer, const SessionDescription* offer) {
  if (offer->contents().size() != answer->contents().size())
    return false;

  for (size_t i = 0; i < offer->contents().size(); ++i) {
    if ((offer->contents()[i].name) != answer->contents()[i].name) {
      return false;
    }
    const MediaContentDescription* offer_mdesc =
        static_cast<const MediaContentDescription*>(
            offer->contents()[i].description);
    const MediaContentDescription* answer_mdesc =
        static_cast<const MediaContentDescription*>(
            answer->contents()[i].description);
    if (offer_mdesc->type() != answer_mdesc->type()) {
      return false;
    }
  }
  return true;
}

// Checks that each non-rejected content has SDES crypto keys or a DTLS
// fingerprint. Mismatches, such as replying with a DTLS fingerprint to SDES
// keys, will be caught in Transport negotiation, and backstopped by Channel's
// |secure_required| check.
static bool VerifyCrypto(const SessionDescription* desc,
                         bool dtls_enabled,
                         std::string* error) {
  const ContentInfos& contents = desc->contents();
  for (size_t index = 0; index < contents.size(); ++index) {
    const ContentInfo* cinfo = &contents[index];
    if (cinfo->rejected) {
      continue;
    }

    // If the content isn't rejected, crypto must be present.
    const MediaContentDescription* media =
        static_cast<const MediaContentDescription*>(cinfo->description);
    const TransportInfo* tinfo = desc->GetTransportInfoByName(cinfo->name);
    if (!media || !tinfo) {
      // Something is not right.
      LOG(LS_ERROR) << kInvalidSdp;
      *error = kInvalidSdp;
      return false;
    }
    if (dtls_enabled) {
      if (!tinfo->description.identity_fingerprint) {
        LOG(LS_WARNING) <<
            "Session description must have DTLS fingerprint if DTLS enabled.";
        *error = kSdpWithoutDtlsFingerprint;
        return false;
      }
    } else {
      if (media->cryptos().empty()) {
        LOG(LS_WARNING) <<
            "Session description must have SDES when DTLS disabled.";
        *error = kSdpWithoutSdesCrypto;
        return false;
      }
    }
  }

  return true;
}

// Checks that each non-rejected content has ice-ufrag and ice-pwd set.
static bool VerifyIceUfragPwdPresent(const SessionDescription* desc) {
  const ContentInfos& contents = desc->contents();
  for (size_t index = 0; index < contents.size(); ++index) {
    const ContentInfo* cinfo = &contents[index];
    if (cinfo->rejected) {
      continue;
    }

    // If the content isn't rejected, ice-ufrag and ice-pwd must be present.
    const TransportInfo* tinfo = desc->GetTransportInfoByName(cinfo->name);
    if (!tinfo) {
      // Something is not right.
      LOG(LS_ERROR) << kInvalidSdp;
      return false;
    }
    if (tinfo->description.ice_ufrag.empty() ||
        tinfo->description.ice_pwd.empty()) {
      LOG(LS_ERROR) << "Session description must have ice ufrag and pwd.";
      return false;
    }
  }
  return true;
}

// Forces |sdesc->crypto_required| to the appropriate state based on the
// current security policy, to ensure a failure occurs if there is an error
// in crypto negotiation.
// Called when processing the local session description.
static void UpdateSessionDescriptionSecurePolicy(cricket::CryptoType type,
                                                 SessionDescription* sdesc) {
  if (!sdesc) {
    return;
  }

  // Updating the |crypto_required_| in MediaContentDescription to the
  // appropriate state based on the current security policy.
  for (cricket::ContentInfos::iterator iter = sdesc->contents().begin();
       iter != sdesc->contents().end(); ++iter) {
    if (cricket::IsMediaContent(&*iter)) {
      MediaContentDescription* mdesc =
          static_cast<MediaContentDescription*> (iter->description);
      if (mdesc) {
        mdesc->set_crypto_required(type);
      }
    }
  }
}

static bool GetAudioSsrcByTrackId(const SessionDescription* session_description,
                                  const std::string& track_id,
                                  uint32_t* ssrc) {
  const cricket::ContentInfo* audio_info =
      cricket::GetFirstAudioContent(session_description);
  if (!audio_info) {
    LOG(LS_ERROR) << "Audio not used in this call";
    return false;
  }

  const cricket::MediaContentDescription* audio_content =
      static_cast<const cricket::MediaContentDescription*>(
          audio_info->description);
  const cricket::StreamParams* stream =
      cricket::GetStreamByIds(audio_content->streams(), "", track_id);
  if (!stream) {
    return false;
  }

  *ssrc = stream->first_ssrc();
  return true;
}

static bool GetTrackIdBySsrc(const SessionDescription* session_description,
                             uint32_t ssrc,
                             std::string* track_id) {
  ASSERT(track_id != NULL);

  const cricket::ContentInfo* audio_info =
      cricket::GetFirstAudioContent(session_description);
  if (audio_info) {
    const cricket::MediaContentDescription* audio_content =
        static_cast<const cricket::MediaContentDescription*>(
            audio_info->description);

    const auto* found =
        cricket::GetStreamBySsrc(audio_content->streams(), ssrc);
    if (found) {
      *track_id = found->id;
      return true;
    }
  }

  const cricket::ContentInfo* video_info =
      cricket::GetFirstVideoContent(session_description);
  if (video_info) {
    const cricket::MediaContentDescription* video_content =
        static_cast<const cricket::MediaContentDescription*>(
            video_info->description);

    const auto* found =
        cricket::GetStreamBySsrc(video_content->streams(), ssrc);
    if (found) {
      *track_id = found->id;
      return true;
    }
  }
  return false;
}

static bool BadSdp(const std::string& source,
                   const std::string& type,
                   const std::string& reason,
                   std::string* err_desc) {
  std::ostringstream desc;
  desc << "Failed to set " << source;
  if (!type.empty()) {
    desc << " " << type;
  }
  desc << " sdp: " << reason;

  if (err_desc) {
    *err_desc = desc.str();
  }
  LOG(LS_ERROR) << desc.str();
  return false;
}

static bool BadSdp(cricket::ContentSource source,
                   const std::string& type,
                   const std::string& reason,
                   std::string* err_desc) {
  if (source == cricket::CS_LOCAL) {
    return BadSdp("local", type, reason, err_desc);
  } else {
    return BadSdp("remote", type, reason, err_desc);
  }
}

static bool BadLocalSdp(const std::string& type,
                        const std::string& reason,
                        std::string* err_desc) {
  return BadSdp(cricket::CS_LOCAL, type, reason, err_desc);
}

static bool BadRemoteSdp(const std::string& type,
                         const std::string& reason,
                         std::string* err_desc) {
  return BadSdp(cricket::CS_REMOTE, type, reason, err_desc);
}

static bool BadOfferSdp(cricket::ContentSource source,
                        const std::string& reason,
                        std::string* err_desc) {
  return BadSdp(source, SessionDescriptionInterface::kOffer, reason, err_desc);
}

static bool BadPranswerSdp(cricket::ContentSource source,
                           const std::string& reason,
                           std::string* err_desc) {
  return BadSdp(source, SessionDescriptionInterface::kPrAnswer,
                reason, err_desc);
}

static bool BadAnswerSdp(cricket::ContentSource source,
                         const std::string& reason,
                         std::string* err_desc) {
  return BadSdp(source, SessionDescriptionInterface::kAnswer, reason, err_desc);
}

#define GET_STRING_OF_STATE(state)   \
  case webrtc::WebRtcSession::state: \
    result = #state;                 \
    break;

static std::string GetStateString(webrtc::WebRtcSession::State state) {
  std::string result;
  switch (state) {
    GET_STRING_OF_STATE(STATE_INIT)
    GET_STRING_OF_STATE(STATE_SENTOFFER)
    GET_STRING_OF_STATE(STATE_RECEIVEDOFFER)
    GET_STRING_OF_STATE(STATE_SENTPRANSWER)
    GET_STRING_OF_STATE(STATE_RECEIVEDPRANSWER)
    GET_STRING_OF_STATE(STATE_INPROGRESS)
    GET_STRING_OF_STATE(STATE_CLOSED)
    default:
      ASSERT(false);
      break;
  }
  return result;
}

#define GET_STRING_OF_ERROR_CODE(err) \
  case webrtc::WebRtcSession::err:    \
    result = #err;                    \
    break;

static std::string GetErrorCodeString(webrtc::WebRtcSession::Error err) {
  std::string result;
  switch (err) {
    GET_STRING_OF_ERROR_CODE(ERROR_NONE)
    GET_STRING_OF_ERROR_CODE(ERROR_CONTENT)
    GET_STRING_OF_ERROR_CODE(ERROR_TRANSPORT)
    default:
      RTC_DCHECK(false);
      break;
  }
  return result;
}

static std::string MakeErrorString(const std::string& error,
                                   const std::string& desc) {
  std::ostringstream ret;
  ret << error << " " << desc;
  return ret.str();
}

static std::string MakeTdErrorString(const std::string& desc) {
  return MakeErrorString(kPushDownTDFailed, desc);
}

// Set |option| to the highest-priority value of |key| in the optional
// constraints if the key is found and has a valid value.
template <typename T>
static void SetOptionFromOptionalConstraint(
    const MediaConstraintsInterface* constraints,
    const std::string& key,
    rtc::Optional<T>* option) {
  if (!constraints) {
    return;
  }
  std::string string_value;
  T value;
  if (constraints->GetOptional().FindFirst(key, &string_value)) {
    if (rtc::FromString(string_value, &value)) {
      *option = rtc::Optional<T>(value);
    }
  }
}

uint32_t ConvertIceTransportTypeToCandidateFilter(
    PeerConnectionInterface::IceTransportsType type) {
  switch (type) {
    case PeerConnectionInterface::kNone:
        return cricket::CF_NONE;
    case PeerConnectionInterface::kRelay:
        return cricket::CF_RELAY;
    case PeerConnectionInterface::kNoHost:
        return (cricket::CF_ALL & ~cricket::CF_HOST);
    case PeerConnectionInterface::kAll:
        return cricket::CF_ALL;
    default: ASSERT(false);
  }
  return cricket::CF_NONE;
}

// Help class used to remember if a a remote peer has requested ice restart by
// by sending a description with new ice ufrag and password.
class IceRestartAnswerLatch {
 public:
  IceRestartAnswerLatch() : ice_restart_(false) { }

  // Returns true if CheckForRemoteIceRestart has been called with a new session
  // description where ice password and ufrag has changed since last time
  // Reset() was called.
  bool Get() const {
    return ice_restart_;
  }

  void Reset() {
    if (ice_restart_) {
      ice_restart_ = false;
    }
  }

  // This method has two purposes: 1. Return whether |new_desc| requests
  // an ICE restart (i.e., new ufrag/pwd). 2. If it requests an ICE restart
  // and it is an OFFER, remember this in |ice_restart_| so that the next
  // Local Answer will be created with new ufrag and pwd.
  bool CheckForRemoteIceRestart(const SessionDescriptionInterface* old_desc,
                                const SessionDescriptionInterface* new_desc) {
    if (!old_desc) {
      return false;
    }
    const SessionDescription* new_sd = new_desc->description();
    const SessionDescription* old_sd = old_desc->description();
    const ContentInfos& contents = new_sd->contents();
    for (size_t index = 0; index < contents.size(); ++index) {
      const ContentInfo* cinfo = &contents[index];
      if (cinfo->rejected) {
        continue;
      }
      // If the content isn't rejected, check if ufrag and password has
      // changed.
      const cricket::TransportDescription* new_transport_desc =
          new_sd->GetTransportDescriptionByName(cinfo->name);
      const cricket::TransportDescription* old_transport_desc =
          old_sd->GetTransportDescriptionByName(cinfo->name);
      if (!new_transport_desc || !old_transport_desc) {
        // No transport description exist. This is not an ice restart.
        continue;
      }
      if (cricket::IceCredentialsChanged(old_transport_desc->ice_ufrag,
                                         old_transport_desc->ice_pwd,
                                         new_transport_desc->ice_ufrag,
                                         new_transport_desc->ice_pwd)) {
        LOG(LS_INFO) << "Remote peer request ice restart.";
        if (new_desc->type() == SessionDescriptionInterface::kOffer) {
          ice_restart_ = true;
        }
        return true;
      }
    }
    return false;
  }

 private:
  bool ice_restart_;
};

WebRtcSession::WebRtcSession(webrtc::MediaControllerInterface* media_controller,
                             rtc::Thread* signaling_thread,
                             rtc::Thread* worker_thread,
                             cricket::PortAllocator* port_allocator)
    : signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      port_allocator_(port_allocator),
      // RFC 3264: The numeric value of the session id and version in the
      // o line MUST be representable with a "64 bit signed integer".
      // Due to this constraint session id |sid_| is max limited to LLONG_MAX.
      sid_(rtc::ToString(rtc::CreateRandomId64() & LLONG_MAX)),
      transport_controller_(new cricket::TransportController(signaling_thread,
                                                             worker_thread,
                                                             port_allocator)),
      media_controller_(media_controller),
      channel_manager_(media_controller_->channel_manager()),
      ice_observer_(NULL),
      ice_connection_state_(PeerConnectionInterface::kIceConnectionNew),
      ice_connection_receiving_(true),
      older_version_remote_peer_(false),
      dtls_enabled_(false),
      data_channel_type_(cricket::DCT_NONE),
      ice_restart_latch_(new IceRestartAnswerLatch),
      metrics_observer_(NULL) {
  transport_controller_->SetIceRole(cricket::ICEROLE_CONTROLLED);
  transport_controller_->SignalConnectionState.connect(
      this, &WebRtcSession::OnTransportControllerConnectionState);
  transport_controller_->SignalReceiving.connect(
      this, &WebRtcSession::OnTransportControllerReceiving);
  transport_controller_->SignalGatheringState.connect(
      this, &WebRtcSession::OnTransportControllerGatheringState);
  transport_controller_->SignalCandidatesGathered.connect(
      this, &WebRtcSession::OnTransportControllerCandidatesGathered);
}

WebRtcSession::~WebRtcSession() {
  ASSERT(signaling_thread()->IsCurrent());
  // Destroy video_channel_ first since it may have a pointer to the
  // voice_channel_.
  if (video_channel_) {
    SignalVideoChannelDestroyed();
    channel_manager_->DestroyVideoChannel(video_channel_.release());
  }
  if (voice_channel_) {
    SignalVoiceChannelDestroyed();
    channel_manager_->DestroyVoiceChannel(voice_channel_.release());
  }
  if (data_channel_) {
    SignalDataChannelDestroyed();
    channel_manager_->DestroyDataChannel(data_channel_.release());
  }

  LOG(LS_INFO) << "Session: " << id() << " is destroyed.";
}

bool WebRtcSession::Initialize(
    const PeerConnectionFactoryInterface::Options& options,
    const MediaConstraintsInterface* constraints,
    rtc::scoped_ptr<DtlsIdentityStoreInterface> dtls_identity_store,
    const PeerConnectionInterface::RTCConfiguration& rtc_configuration) {
  bundle_policy_ = rtc_configuration.bundle_policy;
  rtcp_mux_policy_ = rtc_configuration.rtcp_mux_policy;
  video_options_.disable_prerenderer_smoothing =
      rtc::Optional<bool>(rtc_configuration.disable_prerenderer_smoothing);
  transport_controller_->SetSslMaxProtocolVersion(options.ssl_max_version);

  // Obtain a certificate from RTCConfiguration if any were provided (optional).
  rtc::scoped_refptr<rtc::RTCCertificate> certificate;
  if (!rtc_configuration.certificates.empty()) {
    // TODO(hbos,torbjorng): Decide on certificate-selection strategy instead of
    // just picking the first one. The decision should be made based on the DTLS
    // handshake. The DTLS negotiations need to know about all certificates.
    certificate = rtc_configuration.certificates[0];
  }

  SetIceConfig(ParseIceConfig(rtc_configuration));

  // TODO(perkj): Take |constraints| into consideration. Return false if not all
  // mandatory constraints can be fulfilled. Note that |constraints|
  // can be null.
  bool value;

  if (options.disable_encryption) {
    dtls_enabled_ = false;
  } else {
    // Enable DTLS by default if we have an identity store or a certificate.
    dtls_enabled_ = (dtls_identity_store || certificate);
    // |constraints| can override the default |dtls_enabled_| value.
    if (FindConstraint(constraints, MediaConstraintsInterface::kEnableDtlsSrtp,
                       &value, nullptr)) {
      dtls_enabled_ = value;
    }
  }

  // Enable creation of RTP data channels if the kEnableRtpDataChannels is set.
  // It takes precendence over the disable_sctp_data_channels
  // PeerConnectionFactoryInterface::Options.
  if (FindConstraint(
      constraints, MediaConstraintsInterface::kEnableRtpDataChannels,
      &value, NULL) && value) {
    LOG(LS_INFO) << "Allowing RTP data engine.";
    data_channel_type_ = cricket::DCT_RTP;
  } else {
    // DTLS has to be enabled to use SCTP.
    if (!options.disable_sctp_data_channels && dtls_enabled_) {
      LOG(LS_INFO) << "Allowing SCTP data engine.";
      data_channel_type_ = cricket::DCT_SCTP;
    }
  }

  // Find DSCP constraint.
  if (FindConstraint(
        constraints,
        MediaConstraintsInterface::kEnableDscp,
        &value, NULL)) {
    audio_options_.dscp = rtc::Optional<bool>(value);
    video_options_.dscp = rtc::Optional<bool>(value);
  }

  // Find Suspend Below Min Bitrate constraint.
  if (FindConstraint(
          constraints,
          MediaConstraintsInterface::kEnableVideoSuspendBelowMinBitrate,
          &value,
          NULL)) {
    video_options_.suspend_below_min_bitrate = rtc::Optional<bool>(value);
  }

  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kScreencastMinBitrate,
      &video_options_.screencast_min_bitrate);

  // Find constraints for cpu overuse detection.
  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCpuUnderuseThreshold,
      &video_options_.cpu_underuse_threshold);
  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCpuOveruseThreshold,
      &video_options_.cpu_overuse_threshold);
  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCpuOveruseDetection,
      &video_options_.cpu_overuse_detection);
  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCpuOveruseEncodeUsage,
      &video_options_.cpu_overuse_encode_usage);
  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCpuUnderuseEncodeRsdThreshold,
      &video_options_.cpu_underuse_encode_rsd_threshold);
  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCpuOveruseEncodeRsdThreshold,
      &video_options_.cpu_overuse_encode_rsd_threshold);

  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kNumUnsignalledRecvStreams,
      &video_options_.unsignalled_recv_stream_limit);
  if (video_options_.unsignalled_recv_stream_limit) {
    video_options_.unsignalled_recv_stream_limit = rtc::Optional<int>(
        std::max(0, std::min(kMaxUnsignalledRecvStreams,
                             *video_options_.unsignalled_recv_stream_limit)));
  }

  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kHighStartBitrate,
      &video_options_.video_start_bitrate);

  SetOptionFromOptionalConstraint(constraints,
      MediaConstraintsInterface::kCombinedAudioVideoBwe,
      &audio_options_.combined_audio_video_bwe);

  audio_options_.audio_jitter_buffer_max_packets =
      rtc::Optional<int>(rtc_configuration.audio_jitter_buffer_max_packets);

  audio_options_.audio_jitter_buffer_fast_accelerate = rtc::Optional<bool>(
      rtc_configuration.audio_jitter_buffer_fast_accelerate);

  if (!dtls_enabled_) {
    // Construct with DTLS disabled.
    webrtc_session_desc_factory_.reset(new WebRtcSessionDescriptionFactory(
        signaling_thread(), channel_manager_, this, id()));
  } else {
    // Construct with DTLS enabled.
    if (!certificate) {
      // Use the |dtls_identity_store| to generate a certificate.
      RTC_DCHECK(dtls_identity_store);
      webrtc_session_desc_factory_.reset(new WebRtcSessionDescriptionFactory(
          signaling_thread(), channel_manager_, std::move(dtls_identity_store),
          this, id()));
    } else {
      // Use the already generated certificate.
      webrtc_session_desc_factory_.reset(new WebRtcSessionDescriptionFactory(
          signaling_thread(), channel_manager_, certificate, this, id()));
    }
  }

  webrtc_session_desc_factory_->SignalCertificateReady.connect(
      this, &WebRtcSession::OnCertificateReady);

  if (options.disable_encryption) {
    webrtc_session_desc_factory_->SetSdesPolicy(cricket::SEC_DISABLED);
  }
  port_allocator()->set_candidate_filter(
      ConvertIceTransportTypeToCandidateFilter(rtc_configuration.type));

  return true;
}

void WebRtcSession::Close() {
  SetState(STATE_CLOSED);
  RemoveUnusedChannels(nullptr);
  ASSERT(!voice_channel_);
  ASSERT(!video_channel_);
  ASSERT(!data_channel_);
}

void WebRtcSession::SetSdesPolicy(cricket::SecurePolicy secure_policy) {
  webrtc_session_desc_factory_->SetSdesPolicy(secure_policy);
}

cricket::SecurePolicy WebRtcSession::SdesPolicy() const {
  return webrtc_session_desc_factory_->SdesPolicy();
}

bool WebRtcSession::GetSslRole(const std::string& transport_name,
                               rtc::SSLRole* role) {
  if (!local_desc_ || !remote_desc_) {
    LOG(LS_INFO) << "Local and Remote descriptions must be applied to get "
                 << "SSL Role of the session.";
    return false;
  }

  return transport_controller_->GetSslRole(transport_name, role);
}

bool WebRtcSession::GetSslRole(const cricket::BaseChannel* channel,
                               rtc::SSLRole* role) {
  return channel && GetSslRole(channel->transport_name(), role);
}

void WebRtcSession::CreateOffer(
    CreateSessionDescriptionObserver* observer,
    const PeerConnectionInterface::RTCOfferAnswerOptions& options,
    const cricket::MediaSessionOptions& session_options) {
  webrtc_session_desc_factory_->CreateOffer(observer, options, session_options);
}

void WebRtcSession::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const MediaConstraintsInterface* constraints,
    const cricket::MediaSessionOptions& session_options) {
  webrtc_session_desc_factory_->CreateAnswer(observer, constraints,
                                             session_options);
}

bool WebRtcSession::SetLocalDescription(SessionDescriptionInterface* desc,
                                        std::string* err_desc) {
  ASSERT(signaling_thread()->IsCurrent());

  // Takes the ownership of |desc| regardless of the result.
  rtc::scoped_ptr<SessionDescriptionInterface> desc_temp(desc);

  // Validate SDP.
  if (!ValidateSessionDescription(desc, cricket::CS_LOCAL, err_desc)) {
    return false;
  }

  // Update the initial_offerer flag if this session is the initial_offerer.
  Action action = GetAction(desc->type());
  if (state() == STATE_INIT && action == kOffer) {
    initial_offerer_ = true;
    transport_controller_->SetIceRole(cricket::ICEROLE_CONTROLLING);
  }

  cricket::SecurePolicy sdes_policy =
      webrtc_session_desc_factory_->SdesPolicy();
  cricket::CryptoType crypto_required = dtls_enabled_ ?
      cricket::CT_DTLS : (sdes_policy == cricket::SEC_REQUIRED ?
          cricket::CT_SDES : cricket::CT_NONE);
  // Update the MediaContentDescription crypto settings as per the policy set.
  UpdateSessionDescriptionSecurePolicy(crypto_required, desc->description());

  local_desc_.reset(desc_temp.release());

  // Transport and Media channels will be created only when offer is set.
  if (action == kOffer && !CreateChannels(local_desc_->description())) {
    // TODO(mallinath) - Handle CreateChannel failure, as new local description
    // is applied. Restore back to old description.
    return BadLocalSdp(desc->type(), kCreateChannelFailed, err_desc);
  }

  // Remove unused channels if MediaContentDescription is rejected.
  RemoveUnusedChannels(local_desc_->description());

  if (!UpdateSessionState(action, cricket::CS_LOCAL, err_desc)) {
    return false;
  }

  if (remote_desc_) {
    // Now that we have a local description, we can push down remote candidates.
    UseCandidatesInSessionDescription(remote_desc_.get());
  }

  if (error() != ERROR_NONE) {
    return BadLocalSdp(desc->type(), GetSessionErrorMsg(), err_desc);
  }
  return true;
}

bool WebRtcSession::SetRemoteDescription(SessionDescriptionInterface* desc,
                                         std::string* err_desc) {
  ASSERT(signaling_thread()->IsCurrent());

  // Takes the ownership of |desc| regardless of the result.
  rtc::scoped_ptr<SessionDescriptionInterface> desc_temp(desc);

  // Validate SDP.
  if (!ValidateSessionDescription(desc, cricket::CS_REMOTE, err_desc)) {
    return false;
  }

  rtc::scoped_ptr<SessionDescriptionInterface> old_remote_desc(
      remote_desc_.release());
  remote_desc_.reset(desc_temp.release());

  // Transport and Media channels will be created only when offer is set.
  Action action = GetAction(desc->type());
  if (action == kOffer && !CreateChannels(desc->description())) {
    // TODO(mallinath) - Handle CreateChannel failure, as new local description
    // is applied. Restore back to old description.
    return BadRemoteSdp(desc->type(), kCreateChannelFailed, err_desc);
  }

  // Remove unused channels if MediaContentDescription is rejected.
  RemoveUnusedChannels(desc->description());

  // NOTE: Candidates allocation will be initiated only when SetLocalDescription
  // is called.
  if (!UpdateSessionState(action, cricket::CS_REMOTE, err_desc)) {
    return false;
  }

  if (local_desc_ && !UseCandidatesInSessionDescription(desc)) {
    return BadRemoteSdp(desc->type(), kInvalidCandidates, err_desc);
  }

  // Check if this new SessionDescription contains new ice ufrag and password
  // that indicates the remote peer requests ice restart.
  bool ice_restart =
      ice_restart_latch_->CheckForRemoteIceRestart(old_remote_desc.get(), desc);
  // We retain all received candidates only if ICE is not restarted.
  // When ICE is restarted, all previous candidates belong to an old generation
  // and should not be kept.
  // TODO(deadbeef): This goes against the W3C spec which says the remote
  // description should only contain candidates from the last set remote
  // description plus any candidates added since then. We should remove this
  // once we're sure it won't break anything.
  if (!ice_restart) {
    WebRtcSessionDescriptionFactory::CopyCandidatesFromSessionDescription(
        old_remote_desc.get(), desc);
  }

  if (error() != ERROR_NONE) {
    return BadRemoteSdp(desc->type(), GetSessionErrorMsg(), err_desc);
  }

  // Set the the ICE connection state to connecting since the connection may
  // become writable with peer reflexive candidates before any remote candidate
  // is signaled.
  // TODO(pthatcher): This is a short-term solution for crbug/446908. A real fix
  // is to have a new signal the indicates a change in checking state from the
  // transport and expose a new checking() member from transport that can be
  // read to determine the current checking state. The existing SignalConnecting
  // actually means "gathering candidates", so cannot be be used here.
  if (desc->type() != SessionDescriptionInterface::kOffer &&
      ice_connection_state_ == PeerConnectionInterface::kIceConnectionNew) {
    SetIceConnectionState(PeerConnectionInterface::kIceConnectionChecking);
  }
  return true;
}

void WebRtcSession::LogState(State old_state, State new_state) {
  LOG(LS_INFO) << "Session:" << id()
               << " Old state:" << GetStateString(old_state)
               << " New state:" << GetStateString(new_state);
}

void WebRtcSession::SetState(State state) {
  ASSERT(signaling_thread_->IsCurrent());
  if (state != state_) {
    LogState(state_, state);
    state_ = state;
    SignalState(this, state_);
  }
}

void WebRtcSession::SetError(Error error, const std::string& error_desc) {
  ASSERT(signaling_thread_->IsCurrent());
  if (error != error_) {
    error_ = error;
    error_desc_ = error_desc;
  }
}

bool WebRtcSession::UpdateSessionState(
    Action action, cricket::ContentSource source,
    std::string* err_desc) {
  ASSERT(signaling_thread()->IsCurrent());

  // If there's already a pending error then no state transition should happen.
  // But all call-sites should be verifying this before calling us!
  ASSERT(error() == ERROR_NONE);
  std::string td_err;
  if (action == kOffer) {
    if (!PushdownTransportDescription(source, cricket::CA_OFFER, &td_err)) {
      return BadOfferSdp(source, MakeTdErrorString(td_err), err_desc);
    }
    SetState(source == cricket::CS_LOCAL ? STATE_SENTOFFER
                                         : STATE_RECEIVEDOFFER);
    if (!PushdownMediaDescription(cricket::CA_OFFER, source, err_desc)) {
      SetError(ERROR_CONTENT, *err_desc);
    }
    if (error() != ERROR_NONE) {
      return BadOfferSdp(source, GetSessionErrorMsg(), err_desc);
    }
  } else if (action == kPrAnswer) {
    if (!PushdownTransportDescription(source, cricket::CA_PRANSWER, &td_err)) {
      return BadPranswerSdp(source, MakeTdErrorString(td_err), err_desc);
    }
    EnableChannels();
    SetState(source == cricket::CS_LOCAL ? STATE_SENTPRANSWER
                                         : STATE_RECEIVEDPRANSWER);
    if (!PushdownMediaDescription(cricket::CA_PRANSWER, source, err_desc)) {
      SetError(ERROR_CONTENT, *err_desc);
    }
    if (error() != ERROR_NONE) {
      return BadPranswerSdp(source, GetSessionErrorMsg(), err_desc);
    }
  } else if (action == kAnswer) {
    const cricket::ContentGroup* local_bundle =
        local_desc_->description()->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
    const cricket::ContentGroup* remote_bundle =
        remote_desc_->description()->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
    if (local_bundle && remote_bundle) {
      // The answerer decides the transport to bundle on.
      const cricket::ContentGroup* answer_bundle =
          (source == cricket::CS_LOCAL ? local_bundle : remote_bundle);
      if (!EnableBundle(*answer_bundle)) {
        LOG(LS_WARNING) << "Failed to enable BUNDLE.";
        return BadAnswerSdp(source, kEnableBundleFailed, err_desc);
      }
    }
    // Only push down the transport description after enabling BUNDLE; we don't
    // want to push down a description on a transport about to be destroyed.
    if (!PushdownTransportDescription(source, cricket::CA_ANSWER, &td_err)) {
      return BadAnswerSdp(source, MakeTdErrorString(td_err), err_desc);
    }
    EnableChannels();
    SetState(STATE_INPROGRESS);
    if (!PushdownMediaDescription(cricket::CA_ANSWER, source, err_desc)) {
      SetError(ERROR_CONTENT, *err_desc);
    }
    if (error() != ERROR_NONE) {
      return BadAnswerSdp(source, GetSessionErrorMsg(), err_desc);
    }
  }
  return true;
}

WebRtcSession::Action WebRtcSession::GetAction(const std::string& type) {
  if (type == SessionDescriptionInterface::kOffer) {
    return WebRtcSession::kOffer;
  } else if (type == SessionDescriptionInterface::kPrAnswer) {
    return WebRtcSession::kPrAnswer;
  } else if (type == SessionDescriptionInterface::kAnswer) {
    return WebRtcSession::kAnswer;
  }
  ASSERT(false && "unknown action type");
  return WebRtcSession::kOffer;
}

bool WebRtcSession::PushdownMediaDescription(
    cricket::ContentAction action,
    cricket::ContentSource source,
    std::string* err) {
  auto set_content = [this, action, source, err](cricket::BaseChannel* ch) {
    if (!ch) {
      return true;
    } else if (source == cricket::CS_LOCAL) {
      return ch->PushdownLocalDescription(local_desc_->description(), action,
                                          err);
    } else {
      return ch->PushdownRemoteDescription(remote_desc_->description(), action,
                                           err);
    }
  };

  return (set_content(voice_channel()) &&
          set_content(video_channel()) &&
          set_content(data_channel()));
}

bool WebRtcSession::PushdownTransportDescription(cricket::ContentSource source,
                                                 cricket::ContentAction action,
                                                 std::string* error_desc) {
  RTC_DCHECK(signaling_thread()->IsCurrent());

  if (source == cricket::CS_LOCAL) {
    return PushdownLocalTransportDescription(local_desc_->description(), action,
                                             error_desc);
  }
  return PushdownRemoteTransportDescription(remote_desc_->description(), action,
                                            error_desc);
}

bool WebRtcSession::PushdownLocalTransportDescription(
    const SessionDescription* sdesc,
    cricket::ContentAction action,
    std::string* err) {
  RTC_DCHECK(signaling_thread()->IsCurrent());

  if (!sdesc) {
    return false;
  }

  for (const TransportInfo& tinfo : sdesc->transport_infos()) {
    if (!transport_controller_->SetLocalTransportDescription(
            tinfo.content_name, tinfo.description, action, err)) {
      return false;
    }
  }

  return true;
}

bool WebRtcSession::PushdownRemoteTransportDescription(
    const SessionDescription* sdesc,
    cricket::ContentAction action,
    std::string* err) {
  RTC_DCHECK(signaling_thread()->IsCurrent());

  if (!sdesc) {
    return false;
  }

  for (const TransportInfo& tinfo : sdesc->transport_infos()) {
    if (!transport_controller_->SetRemoteTransportDescription(
            tinfo.content_name, tinfo.description, action, err)) {
      return false;
    }
  }

  return true;
}

bool WebRtcSession::GetTransportDescription(
    const SessionDescription* description,
    const std::string& content_name,
    cricket::TransportDescription* tdesc) {
  if (!description || !tdesc) {
    return false;
  }
  const TransportInfo* transport_info =
      description->GetTransportInfoByName(content_name);
  if (!transport_info) {
    return false;
  }
  *tdesc = transport_info->description;
  return true;
}

bool WebRtcSession::GetTransportStats(SessionStats* stats) {
  ASSERT(signaling_thread()->IsCurrent());
  return (GetChannelTransportStats(voice_channel(), stats) &&
          GetChannelTransportStats(video_channel(), stats) &&
          GetChannelTransportStats(data_channel(), stats));
}

bool WebRtcSession::GetChannelTransportStats(cricket::BaseChannel* ch,
                                             SessionStats* stats) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!ch) {
    // Not using this channel.
    return true;
  }

  const std::string& content_name = ch->content_name();
  const std::string& transport_name = ch->transport_name();
  stats->proxy_to_transport[content_name] = transport_name;
  if (stats->transport_stats.find(transport_name) !=
      stats->transport_stats.end()) {
    // Transport stats already done for this transport.
    return true;
  }

  cricket::TransportStats tstats;
  if (!transport_controller_->GetStats(transport_name, &tstats)) {
    return false;
  }

  stats->transport_stats[transport_name] = tstats;
  return true;
}

bool WebRtcSession::GetLocalCertificate(
    const std::string& transport_name,
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
  ASSERT(signaling_thread()->IsCurrent());
  return transport_controller_->GetLocalCertificate(transport_name,
                                                    certificate);
}

bool WebRtcSession::GetRemoteSSLCertificate(const std::string& transport_name,
                                            rtc::SSLCertificate** cert) {
  ASSERT(signaling_thread()->IsCurrent());
  return transport_controller_->GetRemoteSSLCertificate(transport_name, cert);
}

cricket::BaseChannel* WebRtcSession::GetChannel(
    const std::string& content_name) {
  if (voice_channel() && voice_channel()->content_name() == content_name) {
    return voice_channel();
  }
  if (video_channel() && video_channel()->content_name() == content_name) {
    return video_channel();
  }
  if (data_channel() && data_channel()->content_name() == content_name) {
    return data_channel();
  }
  return nullptr;
}

bool WebRtcSession::EnableBundle(const cricket::ContentGroup& bundle) {
  const std::string* first_content_name = bundle.FirstContentName();
  if (!first_content_name) {
    LOG(LS_WARNING) << "Tried to BUNDLE with no contents.";
    return false;
  }
  const std::string& transport_name = *first_content_name;
  cricket::BaseChannel* first_channel = GetChannel(transport_name);

  auto maybe_set_transport = [this, bundle, transport_name,
                              first_channel](cricket::BaseChannel* ch) {
    if (!ch || !bundle.HasContentName(ch->content_name())) {
      return true;
    }

    if (ch->transport_name() == transport_name) {
      LOG(LS_INFO) << "BUNDLE already enabled for " << ch->content_name()
                   << " on " << transport_name << ".";
      return true;
    }

    if (!ch->SetTransport(transport_name)) {
      LOG(LS_WARNING) << "Failed to enable BUNDLE for " << ch->content_name();
      return false;
    }
    LOG(LS_INFO) << "Enabled BUNDLE for " << ch->content_name() << " on "
                 << transport_name << ".";
    return true;
  };

  if (!maybe_set_transport(voice_channel()) ||
      !maybe_set_transport(video_channel()) ||
      !maybe_set_transport(data_channel())) {
    return false;
  }

  return true;
}

bool WebRtcSession::ProcessIceMessage(const IceCandidateInterface* candidate) {
  if (!remote_desc_) {
    LOG(LS_ERROR) << "ProcessIceMessage: ICE candidates can't be added "
                  << "without any remote session description.";
     return false;
  }

  if (!candidate) {
    LOG(LS_ERROR) << "ProcessIceMessage: Candidate is NULL.";
    return false;
  }

  bool valid = false;
  bool ready = ReadyToUseRemoteCandidate(candidate, NULL, &valid);
  if (!valid) {
    return false;
  }

  // Add this candidate to the remote session description.
  if (!remote_desc_->AddCandidate(candidate)) {
    LOG(LS_ERROR) << "ProcessIceMessage: Candidate cannot be used.";
    return false;
  }

  if (ready) {
    return UseCandidate(candidate);
  } else {
    LOG(LS_INFO) << "ProcessIceMessage: Not ready to use candidate.";
    return true;
  }
}

bool WebRtcSession::SetIceTransports(
    PeerConnectionInterface::IceTransportsType type) {
  return port_allocator()->set_candidate_filter(
        ConvertIceTransportTypeToCandidateFilter(type));
}

cricket::IceConfig WebRtcSession::ParseIceConfig(
    const PeerConnectionInterface::RTCConfiguration& config) const {
  cricket::IceConfig ice_config;
  ice_config.receiving_timeout_ms = config.ice_connection_receiving_timeout;
  ice_config.backup_connection_ping_interval =
      config.ice_backup_candidate_pair_ping_interval;
  ice_config.gather_continually = (config.continual_gathering_policy ==
                                   PeerConnectionInterface::GATHER_CONTINUALLY);
  return ice_config;
}

void WebRtcSession::SetIceConfig(const cricket::IceConfig& config) {
  transport_controller_->SetIceConfig(config);
}

void WebRtcSession::MaybeStartGathering() {
  transport_controller_->MaybeStartGathering();
}

bool WebRtcSession::GetLocalTrackIdBySsrc(uint32_t ssrc,
                                          std::string* track_id) {
  if (!local_desc_) {
    return false;
  }
  return webrtc::GetTrackIdBySsrc(local_desc_->description(), ssrc, track_id);
}

bool WebRtcSession::GetRemoteTrackIdBySsrc(uint32_t ssrc,
                                           std::string* track_id) {
  if (!remote_desc_) {
    return false;
  }
  return webrtc::GetTrackIdBySsrc(remote_desc_->description(), ssrc, track_id);
}

std::string WebRtcSession::BadStateErrMsg(State state) {
  std::ostringstream desc;
  desc << "Called in wrong state: " << GetStateString(state);
  return desc.str();
}

void WebRtcSession::SetAudioPlayout(uint32_t ssrc, bool enable) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_) {
    LOG(LS_ERROR) << "SetAudioPlayout: No audio channel exists.";
    return;
  }
  if (!voice_channel_->SetOutputVolume(ssrc, enable ? 1 : 0)) {
    // Allow that SetOutputVolume fail if |enable| is false but assert
    // otherwise. This in the normal case when the underlying media channel has
    // already been deleted.
    ASSERT(enable == false);
  }
}

void WebRtcSession::SetAudioSend(uint32_t ssrc,
                                 bool enable,
                                 const cricket::AudioOptions& options,
                                 cricket::AudioRenderer* renderer) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_) {
    LOG(LS_ERROR) << "SetAudioSend: No audio channel exists.";
    return;
  }
  if (!voice_channel_->SetAudioSend(ssrc, enable, &options, renderer)) {
    LOG(LS_ERROR) << "SetAudioSend: ssrc is incorrect: " << ssrc;
  }
}

void WebRtcSession::SetAudioPlayoutVolume(uint32_t ssrc, double volume) {
  ASSERT(signaling_thread()->IsCurrent());
  ASSERT(volume >= 0 && volume <= 10);
  if (!voice_channel_) {
    LOG(LS_ERROR) << "SetAudioPlayoutVolume: No audio channel exists.";
    return;
  }

  if (!voice_channel_->SetOutputVolume(ssrc, volume)) {
    ASSERT(false);
  }
}

void WebRtcSession::SetRawAudioSink(uint32_t ssrc,
                                    rtc::scoped_ptr<AudioSinkInterface> sink) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_)
    return;

  voice_channel_->SetRawAudioSink(ssrc, std::move(sink));
}

bool WebRtcSession::SetCaptureDevice(uint32_t ssrc,
                                     cricket::VideoCapturer* camera) {
  ASSERT(signaling_thread()->IsCurrent());

  if (!video_channel_) {
    // |video_channel_| doesnt't exist. Probably because the remote end doesnt't
    // support video.
    LOG(LS_WARNING) << "Video not used in this call.";
    return false;
  }
  if (!video_channel_->SetCapturer(ssrc, camera)) {
    // Allow that SetCapturer fail if |camera| is NULL but assert otherwise.
    // This in the normal case when the underlying media channel has already
    // been deleted.
    ASSERT(camera == NULL);
    return false;
  }
  return true;
}

void WebRtcSession::SetVideoPlayout(uint32_t ssrc,
                                    bool enable,
                                    cricket::VideoRenderer* renderer) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!video_channel_) {
    LOG(LS_WARNING) << "SetVideoPlayout: No video channel exists.";
    return;
  }
  if (!video_channel_->SetRenderer(ssrc, enable ? renderer : NULL)) {
    // Allow that SetRenderer fail if |renderer| is NULL but assert otherwise.
    // This in the normal case when the underlying media channel has already
    // been deleted.
    ASSERT(renderer == NULL);
  }
}

void WebRtcSession::SetVideoSend(uint32_t ssrc,
                                 bool enable,
                                 const cricket::VideoOptions* options) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!video_channel_) {
    LOG(LS_WARNING) << "SetVideoSend: No video channel exists.";
    return;
  }
  if (!video_channel_->SetVideoSend(ssrc, enable, options)) {
    // Allow that MuteStream fail if |enable| is false but assert otherwise.
    // This in the normal case when the underlying media channel has already
    // been deleted.
    ASSERT(enable == false);
  }
}

bool WebRtcSession::CanInsertDtmf(const std::string& track_id) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_) {
    LOG(LS_ERROR) << "CanInsertDtmf: No audio channel exists.";
    return false;
  }
  uint32_t send_ssrc = 0;
  // The Dtmf is negotiated per channel not ssrc, so we only check if the ssrc
  // exists.
  if (!local_desc_ ||
      !GetAudioSsrcByTrackId(local_desc_->description(), track_id,
                             &send_ssrc)) {
    LOG(LS_ERROR) << "CanInsertDtmf: Track does not exist: " << track_id;
    return false;
  }
  return voice_channel_->CanInsertDtmf();
}

bool WebRtcSession::InsertDtmf(const std::string& track_id,
                               int code, int duration) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_) {
    LOG(LS_ERROR) << "InsertDtmf: No audio channel exists.";
    return false;
  }
  uint32_t send_ssrc = 0;
  if (!VERIFY(local_desc_ && GetAudioSsrcByTrackId(local_desc_->description(),
                                                   track_id, &send_ssrc))) {
    LOG(LS_ERROR) << "InsertDtmf: Track does not exist: " << track_id;
    return false;
  }
  if (!voice_channel_->InsertDtmf(send_ssrc, code, duration)) {
    LOG(LS_ERROR) << "Failed to insert DTMF to channel.";
    return false;
  }
  return true;
}

sigslot::signal0<>* WebRtcSession::GetOnDestroyedSignal() {
  return &SignalVoiceChannelDestroyed;
}

bool WebRtcSession::SendData(const cricket::SendDataParams& params,
                             const rtc::Buffer& payload,
                             cricket::SendDataResult* result) {
  if (!data_channel_) {
    LOG(LS_ERROR) << "SendData called when data_channel_ is NULL.";
    return false;
  }
  return data_channel_->SendData(params, payload, result);
}

bool WebRtcSession::ConnectDataChannel(DataChannel* webrtc_data_channel) {
  if (!data_channel_) {
    LOG(LS_ERROR) << "ConnectDataChannel called when data_channel_ is NULL.";
    return false;
  }
  data_channel_->SignalReadyToSendData.connect(webrtc_data_channel,
                                               &DataChannel::OnChannelReady);
  data_channel_->SignalDataReceived.connect(webrtc_data_channel,
                                            &DataChannel::OnDataReceived);
  data_channel_->SignalStreamClosedRemotely.connect(
      webrtc_data_channel, &DataChannel::OnStreamClosedRemotely);
  return true;
}

void WebRtcSession::DisconnectDataChannel(DataChannel* webrtc_data_channel) {
  if (!data_channel_) {
    LOG(LS_ERROR) << "DisconnectDataChannel called when data_channel_ is NULL.";
    return;
  }
  data_channel_->SignalReadyToSendData.disconnect(webrtc_data_channel);
  data_channel_->SignalDataReceived.disconnect(webrtc_data_channel);
  data_channel_->SignalStreamClosedRemotely.disconnect(webrtc_data_channel);
}

void WebRtcSession::AddSctpDataStream(int sid) {
  if (!data_channel_) {
    LOG(LS_ERROR) << "AddDataChannelStreams called when data_channel_ is NULL.";
    return;
  }
  data_channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(sid));
  data_channel_->AddSendStream(cricket::StreamParams::CreateLegacy(sid));
}

void WebRtcSession::RemoveSctpDataStream(int sid) {
  if (!data_channel_) {
    LOG(LS_ERROR) << "RemoveDataChannelStreams called when data_channel_ is "
                  << "NULL.";
    return;
  }
  data_channel_->RemoveRecvStream(sid);
  data_channel_->RemoveSendStream(sid);
}

bool WebRtcSession::ReadyToSendData() const {
  return data_channel_ && data_channel_->ready_to_send_data();
}

cricket::DataChannelType WebRtcSession::data_channel_type() const {
  return data_channel_type_;
}

bool WebRtcSession::IceRestartPending() const {
  return ice_restart_latch_->Get();
}

void WebRtcSession::ResetIceRestartLatch() {
  ice_restart_latch_->Reset();
}

void WebRtcSession::OnCertificateReady(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  transport_controller_->SetLocalCertificate(certificate);
}

bool WebRtcSession::waiting_for_certificate_for_testing() const {
  return webrtc_session_desc_factory_->waiting_for_certificate_for_testing();
}

const rtc::scoped_refptr<rtc::RTCCertificate>&
WebRtcSession::certificate_for_testing() {
  return transport_controller_->certificate_for_testing();
}

void WebRtcSession::SetIceConnectionState(
      PeerConnectionInterface::IceConnectionState state) {
  if (ice_connection_state_ == state) {
    return;
  }

  // ASSERT that the requested transition is allowed.  Note that
  // WebRtcSession does not implement "kIceConnectionClosed" (that is handled
  // within PeerConnection).  This switch statement should compile away when
  // ASSERTs are disabled.
  LOG(LS_INFO) << "Changing IceConnectionState " << ice_connection_state_
               << " => " << state;
  switch (ice_connection_state_) {
    case PeerConnectionInterface::kIceConnectionNew:
      ASSERT(state == PeerConnectionInterface::kIceConnectionChecking);
      break;
    case PeerConnectionInterface::kIceConnectionChecking:
      ASSERT(state == PeerConnectionInterface::kIceConnectionFailed ||
             state == PeerConnectionInterface::kIceConnectionConnected);
      break;
    case PeerConnectionInterface::kIceConnectionConnected:
      ASSERT(state == PeerConnectionInterface::kIceConnectionDisconnected ||
             state == PeerConnectionInterface::kIceConnectionChecking ||
             state == PeerConnectionInterface::kIceConnectionCompleted);
      break;
    case PeerConnectionInterface::kIceConnectionCompleted:
      ASSERT(state == PeerConnectionInterface::kIceConnectionConnected ||
             state == PeerConnectionInterface::kIceConnectionDisconnected);
      break;
    case PeerConnectionInterface::kIceConnectionFailed:
      ASSERT(state == PeerConnectionInterface::kIceConnectionNew);
      break;
    case PeerConnectionInterface::kIceConnectionDisconnected:
      ASSERT(state == PeerConnectionInterface::kIceConnectionChecking ||
             state == PeerConnectionInterface::kIceConnectionConnected ||
             state == PeerConnectionInterface::kIceConnectionCompleted ||
             state == PeerConnectionInterface::kIceConnectionFailed);
      break;
    case PeerConnectionInterface::kIceConnectionClosed:
      ASSERT(false);
      break;
    default:
      ASSERT(false);
      break;
  }

  ice_connection_state_ = state;
  if (ice_observer_) {
    ice_observer_->OnIceConnectionChange(ice_connection_state_);
  }
}

void WebRtcSession::OnTransportControllerConnectionState(
    cricket::IceConnectionState state) {
  switch (state) {
    case cricket::kIceConnectionConnecting:
      // If the current state is Connected or Completed, then there were
      // writable channels but now there are not, so the next state must
      // be Disconnected.
      // kIceConnectionConnecting is currently used as the default,
      // un-connected state by the TransportController, so its only use is
      // detecting disconnections.
      if (ice_connection_state_ ==
              PeerConnectionInterface::kIceConnectionConnected ||
          ice_connection_state_ ==
              PeerConnectionInterface::kIceConnectionCompleted) {
        SetIceConnectionState(
            PeerConnectionInterface::kIceConnectionDisconnected);
      }
      break;
    case cricket::kIceConnectionFailed:
      SetIceConnectionState(PeerConnectionInterface::kIceConnectionFailed);
      break;
    case cricket::kIceConnectionConnected:
      LOG(LS_INFO) << "Changing to ICE connected state because "
                   << "all transports are writable.";
      SetIceConnectionState(PeerConnectionInterface::kIceConnectionConnected);
      break;
    case cricket::kIceConnectionCompleted:
      LOG(LS_INFO) << "Changing to ICE completed state because "
                   << "all transports are complete.";
      if (ice_connection_state_ !=
          PeerConnectionInterface::kIceConnectionConnected) {
        // If jumping directly from "checking" to "connected",
        // signal "connected" first.
        SetIceConnectionState(PeerConnectionInterface::kIceConnectionConnected);
      }
      SetIceConnectionState(PeerConnectionInterface::kIceConnectionCompleted);
      if (metrics_observer_) {
        ReportTransportStats();
      }
      break;
    default:
      ASSERT(false);
  }
}

void WebRtcSession::OnTransportControllerReceiving(bool receiving) {
  SetIceConnectionReceiving(receiving);
}

void WebRtcSession::SetIceConnectionReceiving(bool receiving) {
  if (ice_connection_receiving_ == receiving) {
    return;
  }
  ice_connection_receiving_ = receiving;
  if (ice_observer_) {
    ice_observer_->OnIceConnectionReceivingChange(receiving);
  }
}

void WebRtcSession::OnTransportControllerCandidatesGathered(
    const std::string& transport_name,
    const cricket::Candidates& candidates) {
  ASSERT(signaling_thread()->IsCurrent());
  int sdp_mline_index;
  if (!GetLocalCandidateMediaIndex(transport_name, &sdp_mline_index)) {
    LOG(LS_ERROR) << "OnTransportControllerCandidatesGathered: content name "
                  << transport_name << " not found";
    return;
  }

  for (cricket::Candidates::const_iterator citer = candidates.begin();
       citer != candidates.end(); ++citer) {
    // Use transport_name as the candidate media id.
    JsepIceCandidate candidate(transport_name, sdp_mline_index, *citer);
    if (ice_observer_) {
      ice_observer_->OnIceCandidate(&candidate);
    }
    if (local_desc_) {
      local_desc_->AddCandidate(&candidate);
    }
  }
}

// Enabling voice and video channel.
void WebRtcSession::EnableChannels() {
  if (voice_channel_ && !voice_channel_->enabled())
    voice_channel_->Enable(true);

  if (video_channel_ && !video_channel_->enabled())
    video_channel_->Enable(true);

  if (data_channel_ && !data_channel_->enabled())
    data_channel_->Enable(true);
}

// Returns the media index for a local ice candidate given the content name.
bool WebRtcSession::GetLocalCandidateMediaIndex(const std::string& content_name,
                                                int* sdp_mline_index) {
  if (!local_desc_ || !sdp_mline_index) {
    return false;
  }

  bool content_found = false;
  const ContentInfos& contents = local_desc_->description()->contents();
  for (size_t index = 0; index < contents.size(); ++index) {
    if (contents[index].name == content_name) {
      *sdp_mline_index = static_cast<int>(index);
      content_found = true;
      break;
    }
  }
  return content_found;
}

bool WebRtcSession::UseCandidatesInSessionDescription(
    const SessionDescriptionInterface* remote_desc) {
  if (!remote_desc) {
    return true;
  }
  bool ret = true;

  for (size_t m = 0; m < remote_desc->number_of_mediasections(); ++m) {
    const IceCandidateCollection* candidates = remote_desc->candidates(m);
    for (size_t n = 0; n < candidates->count(); ++n) {
      const IceCandidateInterface* candidate = candidates->at(n);
      bool valid = false;
      if (!ReadyToUseRemoteCandidate(candidate, remote_desc, &valid)) {
        if (valid) {
          LOG(LS_INFO) << "UseCandidatesInSessionDescription: Not ready to use "
                       << "candidate.";
        }
        continue;
      }
      ret = UseCandidate(candidate);
      if (!ret) {
        break;
      }
    }
  }
  return ret;
}

bool WebRtcSession::UseCandidate(
    const IceCandidateInterface* candidate) {

  size_t mediacontent_index = static_cast<size_t>(candidate->sdp_mline_index());
  size_t remote_content_size = remote_desc_->description()->contents().size();
  if (mediacontent_index >= remote_content_size) {
    LOG(LS_ERROR)
        << "UseRemoteCandidateInSession: Invalid candidate media index.";
    return false;
  }

  cricket::ContentInfo content =
      remote_desc_->description()->contents()[mediacontent_index];
  std::vector<cricket::Candidate> candidates;
  candidates.push_back(candidate->candidate());
  // Invoking BaseSession method to handle remote candidates.
  std::string error;
  if (transport_controller_->AddRemoteCandidates(content.name, candidates,
                                                 &error)) {
    // Candidates successfully submitted for checking.
    if (ice_connection_state_ == PeerConnectionInterface::kIceConnectionNew ||
        ice_connection_state_ ==
            PeerConnectionInterface::kIceConnectionDisconnected) {
      // If state is New, then the session has just gotten its first remote ICE
      // candidates, so go to Checking.
      // If state is Disconnected, the session is re-using old candidates or
      // receiving additional ones, so go to Checking.
      // If state is Connected, stay Connected.
      // TODO(bemasc): If state is Connected, and the new candidates are for a
      // newly added transport, then the state actually _should_ move to
      // checking.  Add a way to distinguish that case.
      SetIceConnectionState(PeerConnectionInterface::kIceConnectionChecking);
    }
    // TODO(bemasc): If state is Completed, go back to Connected.
  } else {
    if (!error.empty()) {
      LOG(LS_WARNING) << error;
    }
  }
  return true;
}

void WebRtcSession::RemoveUnusedChannels(const SessionDescription* desc) {
  // Destroy video_channel_ first since it may have a pointer to the
  // voice_channel_.
  const cricket::ContentInfo* video_info =
      cricket::GetFirstVideoContent(desc);
  if ((!video_info || video_info->rejected) && video_channel_) {
    SignalVideoChannelDestroyed();
    channel_manager_->DestroyVideoChannel(video_channel_.release());
  }

  const cricket::ContentInfo* voice_info =
      cricket::GetFirstAudioContent(desc);
  if ((!voice_info || voice_info->rejected) && voice_channel_) {
    SignalVoiceChannelDestroyed();
    channel_manager_->DestroyVoiceChannel(voice_channel_.release());
  }

  const cricket::ContentInfo* data_info =
      cricket::GetFirstDataContent(desc);
  if ((!data_info || data_info->rejected) && data_channel_) {
    SignalDataChannelDestroyed();
    channel_manager_->DestroyDataChannel(data_channel_.release());
  }
}

// TODO(mallinath) - Add a correct error code if the channels are not created
// due to BUNDLE is enabled but rtcp-mux is disabled.
bool WebRtcSession::CreateChannels(const SessionDescription* desc) {
  // Creating the media channels and transport proxies.
  const cricket::ContentInfo* voice = cricket::GetFirstAudioContent(desc);
  if (voice && !voice->rejected && !voice_channel_) {
    if (!CreateVoiceChannel(voice)) {
      LOG(LS_ERROR) << "Failed to create voice channel.";
      return false;
    }
  }

  const cricket::ContentInfo* video = cricket::GetFirstVideoContent(desc);
  if (video && !video->rejected && !video_channel_) {
    if (!CreateVideoChannel(video)) {
      LOG(LS_ERROR) << "Failed to create video channel.";
      return false;
    }
  }

  const cricket::ContentInfo* data = cricket::GetFirstDataContent(desc);
  if (data_channel_type_ != cricket::DCT_NONE &&
      data && !data->rejected && !data_channel_) {
    if (!CreateDataChannel(data)) {
      LOG(LS_ERROR) << "Failed to create data channel.";
      return false;
    }
  }

  if (rtcp_mux_policy_ == PeerConnectionInterface::kRtcpMuxPolicyRequire) {
    if (voice_channel()) {
      voice_channel()->ActivateRtcpMux();
    }
    if (video_channel()) {
      video_channel()->ActivateRtcpMux();
    }
    if (data_channel()) {
      data_channel()->ActivateRtcpMux();
    }
  }

  // Enable BUNDLE immediately when kBundlePolicyMaxBundle is in effect.
  if (bundle_policy_ == PeerConnectionInterface::kBundlePolicyMaxBundle) {
    const cricket::ContentGroup* bundle_group = desc->GetGroupByName(
        cricket::GROUP_TYPE_BUNDLE);
    if (!bundle_group) {
      LOG(LS_WARNING) << "max-bundle specified without BUNDLE specified";
      return false;
    }
    if (!EnableBundle(*bundle_group)) {
      LOG(LS_WARNING) << "max-bundle failed to enable bundling.";
      return false;
    }
  }

  return true;
}

bool WebRtcSession::CreateVoiceChannel(const cricket::ContentInfo* content) {
  voice_channel_.reset(channel_manager_->CreateVoiceChannel(
      media_controller_, transport_controller_.get(), content->name, true,
      audio_options_));
  if (!voice_channel_) {
    return false;
  }

  voice_channel_->SignalDtlsSetupFailure.connect(
      this, &WebRtcSession::OnDtlsSetupFailure);

  SignalVoiceChannelCreated();
  voice_channel_->transport_channel()->SignalSentPacket.connect(
      this, &WebRtcSession::OnSentPacket_w);
  return true;
}

bool WebRtcSession::CreateVideoChannel(const cricket::ContentInfo* content) {
  video_channel_.reset(channel_manager_->CreateVideoChannel(
      media_controller_, transport_controller_.get(), content->name, true,
      video_options_));
  if (!video_channel_) {
    return false;
  }

  video_channel_->SignalDtlsSetupFailure.connect(
      this, &WebRtcSession::OnDtlsSetupFailure);

  SignalVideoChannelCreated();
  video_channel_->transport_channel()->SignalSentPacket.connect(
      this, &WebRtcSession::OnSentPacket_w);
  return true;
}

bool WebRtcSession::CreateDataChannel(const cricket::ContentInfo* content) {
  bool sctp = (data_channel_type_ == cricket::DCT_SCTP);
  data_channel_.reset(channel_manager_->CreateDataChannel(
      transport_controller_.get(), content->name, !sctp, data_channel_type_));
  if (!data_channel_) {
    return false;
  }

  if (sctp) {
    data_channel_->SignalDataReceived.connect(
        this, &WebRtcSession::OnDataChannelMessageReceived);
  }

  data_channel_->SignalDtlsSetupFailure.connect(
      this, &WebRtcSession::OnDtlsSetupFailure);

  SignalDataChannelCreated();
  data_channel_->transport_channel()->SignalSentPacket.connect(
      this, &WebRtcSession::OnSentPacket_w);
  return true;
}

void WebRtcSession::OnDtlsSetupFailure(cricket::BaseChannel*, bool rtcp) {
  SetError(ERROR_TRANSPORT,
           rtcp ? kDtlsSetupFailureRtcp : kDtlsSetupFailureRtp);
}

void WebRtcSession::OnDataChannelMessageReceived(
    cricket::DataChannel* channel,
    const cricket::ReceiveDataParams& params,
    const rtc::Buffer& payload) {
  RTC_DCHECK(data_channel_type_ == cricket::DCT_SCTP);
  if (params.type == cricket::DMT_CONTROL && IsOpenMessage(payload)) {
    // Received OPEN message; parse and signal that a new data channel should
    // be created.
    std::string label;
    InternalDataChannelInit config;
    config.id = params.ssrc;
    if (!ParseDataChannelOpenMessage(payload, &label, &config)) {
      LOG(LS_WARNING) << "Failed to parse the OPEN message for sid "
                      << params.ssrc;
      return;
    }
    config.open_handshake_role = InternalDataChannelInit::kAcker;
    SignalDataChannelOpenMessage(label, config);
  }
  // Otherwise ignore the message.
}

// Returns false if bundle is enabled and rtcp_mux is disabled.
bool WebRtcSession::ValidateBundleSettings(const SessionDescription* desc) {
  bool bundle_enabled = desc->HasGroup(cricket::GROUP_TYPE_BUNDLE);
  if (!bundle_enabled)
    return true;

  const cricket::ContentGroup* bundle_group =
      desc->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  ASSERT(bundle_group != NULL);

  const cricket::ContentInfos& contents = desc->contents();
  for (cricket::ContentInfos::const_iterator citer = contents.begin();
       citer != contents.end(); ++citer) {
    const cricket::ContentInfo* content = (&*citer);
    ASSERT(content != NULL);
    if (bundle_group->HasContentName(content->name) &&
        !content->rejected && content->type == cricket::NS_JINGLE_RTP) {
      if (!HasRtcpMuxEnabled(content))
        return false;
    }
  }
  // RTCP-MUX is enabled in all the contents.
  return true;
}

bool WebRtcSession::HasRtcpMuxEnabled(
    const cricket::ContentInfo* content) {
  const cricket::MediaContentDescription* description =
      static_cast<cricket::MediaContentDescription*>(content->description);
  return description->rtcp_mux();
}

bool WebRtcSession::ValidateSessionDescription(
    const SessionDescriptionInterface* sdesc,
    cricket::ContentSource source, std::string* err_desc) {
  std::string type;
  if (error() != ERROR_NONE) {
    return BadSdp(source, type, GetSessionErrorMsg(), err_desc);
  }

  if (!sdesc || !sdesc->description()) {
    return BadSdp(source, type, kInvalidSdp, err_desc);
  }

  type = sdesc->type();
  Action action = GetAction(sdesc->type());
  if (source == cricket::CS_LOCAL) {
    if (!ExpectSetLocalDescription(action))
      return BadLocalSdp(type, BadStateErrMsg(state()), err_desc);
  } else {
    if (!ExpectSetRemoteDescription(action))
      return BadRemoteSdp(type, BadStateErrMsg(state()), err_desc);
  }

  // Verify crypto settings.
  std::string crypto_error;
  if ((webrtc_session_desc_factory_->SdesPolicy() == cricket::SEC_REQUIRED ||
       dtls_enabled_) &&
      !VerifyCrypto(sdesc->description(), dtls_enabled_, &crypto_error)) {
    return BadSdp(source, type, crypto_error, err_desc);
  }

  // Verify ice-ufrag and ice-pwd.
  if (!VerifyIceUfragPwdPresent(sdesc->description())) {
    return BadSdp(source, type, kSdpWithoutIceUfragPwd, err_desc);
  }

  if (!ValidateBundleSettings(sdesc->description())) {
    return BadSdp(source, type, kBundleWithoutRtcpMux, err_desc);
  }

  // Verify m-lines in Answer when compared against Offer.
  if (action == kAnswer) {
    const cricket::SessionDescription* offer_desc =
        (source == cricket::CS_LOCAL) ? remote_desc_->description()
                                      : local_desc_->description();
    if (!VerifyMediaDescriptions(sdesc->description(), offer_desc)) {
      return BadAnswerSdp(source, kMlineMismatch, err_desc);
    }
  }

  return true;
}

bool WebRtcSession::ExpectSetLocalDescription(Action action) {
  return ((action == kOffer && state() == STATE_INIT) ||
          // update local offer
          (action == kOffer && state() == STATE_SENTOFFER) ||
          // update the current ongoing session.
          (action == kOffer && state() == STATE_INPROGRESS) ||
          // accept remote offer
          (action == kAnswer && state() == STATE_RECEIVEDOFFER) ||
          (action == kAnswer && state() == STATE_SENTPRANSWER) ||
          (action == kPrAnswer && state() == STATE_RECEIVEDOFFER) ||
          (action == kPrAnswer && state() == STATE_SENTPRANSWER));
}

bool WebRtcSession::ExpectSetRemoteDescription(Action action) {
  return ((action == kOffer && state() == STATE_INIT) ||
          // update remote offer
          (action == kOffer && state() == STATE_RECEIVEDOFFER) ||
          // update the current ongoing session
          (action == kOffer && state() == STATE_INPROGRESS) ||
          // accept local offer
          (action == kAnswer && state() == STATE_SENTOFFER) ||
          (action == kAnswer && state() == STATE_RECEIVEDPRANSWER) ||
          (action == kPrAnswer && state() == STATE_SENTOFFER) ||
          (action == kPrAnswer && state() == STATE_RECEIVEDPRANSWER));
}

std::string WebRtcSession::GetSessionErrorMsg() {
  std::ostringstream desc;
  desc << kSessionError << GetErrorCodeString(error()) << ". ";
  desc << kSessionErrorDesc << error_desc() << ".";
  return desc.str();
}

// We need to check the local/remote description for the Transport instead of
// the session, because a new Transport added during renegotiation may have
// them unset while the session has them set from the previous negotiation.
// Not doing so may trigger the auto generation of transport description and
// mess up DTLS identity information, ICE credential, etc.
bool WebRtcSession::ReadyToUseRemoteCandidate(
    const IceCandidateInterface* candidate,
    const SessionDescriptionInterface* remote_desc,
    bool* valid) {
  *valid = true;;

  const SessionDescriptionInterface* current_remote_desc =
      remote_desc ? remote_desc : remote_desc_.get();

  if (!current_remote_desc) {
    return false;
  }

  size_t mediacontent_index =
      static_cast<size_t>(candidate->sdp_mline_index());
  size_t remote_content_size =
      current_remote_desc->description()->contents().size();
  if (mediacontent_index >= remote_content_size) {
    LOG(LS_ERROR)
        << "ReadyToUseRemoteCandidate: Invalid candidate media index.";

    *valid = false;
    return false;
  }

  cricket::ContentInfo content =
      current_remote_desc->description()->contents()[mediacontent_index];
  cricket::BaseChannel* channel = GetChannel(content.name);
  if (!channel) {
    return false;
  }

  return transport_controller_->ReadyForRemoteCandidates(
      channel->transport_name());
}

void WebRtcSession::OnTransportControllerGatheringState(
    cricket::IceGatheringState state) {
  ASSERT(signaling_thread()->IsCurrent());
  if (state == cricket::kIceGatheringGathering) {
    if (ice_observer_) {
      ice_observer_->OnIceGatheringChange(
          PeerConnectionInterface::kIceGatheringGathering);
    }
  } else if (state == cricket::kIceGatheringComplete) {
    if (ice_observer_) {
      ice_observer_->OnIceGatheringChange(
          PeerConnectionInterface::kIceGatheringComplete);
      ice_observer_->OnIceComplete();
    }
  }
}

void WebRtcSession::ReportTransportStats() {
  // Use a set so we don't report the same stats twice if two channels share
  // a transport.
  std::set<std::string> transport_names;
  if (voice_channel()) {
    transport_names.insert(voice_channel()->transport_name());
  }
  if (video_channel()) {
    transport_names.insert(video_channel()->transport_name());
  }
  if (data_channel()) {
    transport_names.insert(data_channel()->transport_name());
  }
  for (const auto& name : transport_names) {
    cricket::TransportStats stats;
    if (transport_controller_->GetStats(name, &stats)) {
      ReportBestConnectionState(stats);
      ReportNegotiatedCiphers(stats);
    }
  }
}
// Walk through the ConnectionInfos to gather best connection usage
// for IPv4 and IPv6.
void WebRtcSession::ReportBestConnectionState(
    const cricket::TransportStats& stats) {
  RTC_DCHECK(metrics_observer_ != NULL);
  for (cricket::TransportChannelStatsList::const_iterator it =
         stats.channel_stats.begin();
       it != stats.channel_stats.end(); ++it) {
    for (cricket::ConnectionInfos::const_iterator it_info =
           it->connection_infos.begin();
         it_info != it->connection_infos.end(); ++it_info) {
      if (!it_info->best_connection) {
        continue;
      }

      PeerConnectionEnumCounterType type = kPeerConnectionEnumCounterMax;
      const cricket::Candidate& local = it_info->local_candidate;
      const cricket::Candidate& remote = it_info->remote_candidate;

      // Increment the counter for IceCandidatePairType.
      if (local.protocol() == cricket::TCP_PROTOCOL_NAME ||
          (local.type() == RELAY_PORT_TYPE &&
           local.relay_protocol() == cricket::TCP_PROTOCOL_NAME)) {
        type = kEnumCounterIceCandidatePairTypeTcp;
      } else if (local.protocol() == cricket::UDP_PROTOCOL_NAME) {
        type = kEnumCounterIceCandidatePairTypeUdp;
      } else {
        RTC_CHECK(0);
      }
      metrics_observer_->IncrementEnumCounter(
          type, GetIceCandidatePairCounter(local, remote),
          kIceCandidatePairMax);

      // Increment the counter for IP type.
      if (local.address().family() == AF_INET) {
        metrics_observer_->IncrementEnumCounter(
            kEnumCounterAddressFamily, kBestConnections_IPv4,
            kPeerConnectionAddressFamilyCounter_Max);

      } else if (local.address().family() == AF_INET6) {
        metrics_observer_->IncrementEnumCounter(
            kEnumCounterAddressFamily, kBestConnections_IPv6,
            kPeerConnectionAddressFamilyCounter_Max);
      } else {
        RTC_CHECK(0);
      }

      return;
    }
  }
}

void WebRtcSession::ReportNegotiatedCiphers(
    const cricket::TransportStats& stats) {
  RTC_DCHECK(metrics_observer_ != NULL);
  if (!dtls_enabled_ || stats.channel_stats.empty()) {
    return;
  }

  int srtp_crypto_suite = stats.channel_stats[0].srtp_crypto_suite;
  int ssl_cipher_suite = stats.channel_stats[0].ssl_cipher_suite;
  if (srtp_crypto_suite == rtc::SRTP_INVALID_CRYPTO_SUITE &&
      ssl_cipher_suite == rtc::TLS_NULL_WITH_NULL_NULL) {
    return;
  }

  PeerConnectionEnumCounterType srtp_counter_type;
  PeerConnectionEnumCounterType ssl_counter_type;
  if (stats.transport_name == cricket::CN_AUDIO) {
    srtp_counter_type = kEnumCounterAudioSrtpCipher;
    ssl_counter_type = kEnumCounterAudioSslCipher;
  } else if (stats.transport_name == cricket::CN_VIDEO) {
    srtp_counter_type = kEnumCounterVideoSrtpCipher;
    ssl_counter_type = kEnumCounterVideoSslCipher;
  } else if (stats.transport_name == cricket::CN_DATA) {
    srtp_counter_type = kEnumCounterDataSrtpCipher;
    ssl_counter_type = kEnumCounterDataSslCipher;
  } else {
    RTC_NOTREACHED();
    return;
  }

  if (srtp_crypto_suite != rtc::SRTP_INVALID_CRYPTO_SUITE) {
    metrics_observer_->IncrementSparseEnumCounter(srtp_counter_type,
                                                  srtp_crypto_suite);
  }
  if (ssl_cipher_suite != rtc::TLS_NULL_WITH_NULL_NULL) {
    metrics_observer_->IncrementSparseEnumCounter(ssl_counter_type,
                                                  ssl_cipher_suite);
  }
}

void WebRtcSession::OnSentPacket_w(cricket::TransportChannel* channel,
                                   const rtc::SentPacket& sent_packet) {
  RTC_DCHECK(worker_thread()->IsCurrent());
  media_controller_->call_w()->OnSentPacket(sent_packet);
}

}  // namespace webrtc
