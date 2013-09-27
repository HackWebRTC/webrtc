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

#include "talk/app/webrtc/webrtcsession.h"

#include <algorithm>
#include <climits>
#include <vector>

#include "talk/app/webrtc/jsepicecandidate.h"
#include "talk/app/webrtc/jsepsessiondescription.h"
#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/app/webrtc/mediastreamsignaling.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/webrtcsessiondescriptionfactory.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/base/stringencode.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/videocapturer.h"
#include "talk/session/media/channel.h"
#include "talk/session/media/channelmanager.h"
#include "talk/session/media/mediasession.h"

using cricket::ContentInfo;
using cricket::ContentInfos;
using cricket::MediaContentDescription;
using cricket::SessionDescription;
using cricket::TransportInfo;

namespace webrtc {

const char MediaConstraintsInterface::kInternalConstraintPrefix[] = "internal";

// Supported MediaConstraints.
// DTLS-SRTP pseudo-constraints.
const char MediaConstraintsInterface::kEnableDtlsSrtp[] =
    "DtlsSrtpKeyAgreement";
// DataChannel pseudo constraints.
const char MediaConstraintsInterface::kEnableRtpDataChannels[] =
    "RtpDataChannels";
// This constraint is for internal use only, representing the Chrome command
// line flag. So it is prefixed with kInternalConstraintPrefix so JS values
// will be removed.
const char MediaConstraintsInterface::kEnableSctpDataChannels[] =
    "internalSctpDataChannels";

// Error messages
const char kSetLocalSdpFailed[] = "SetLocalDescription failed: ";
const char kSetRemoteSdpFailed[] = "SetRemoteDescription failed: ";
const char kCreateChannelFailed[] = "Failed to create channels.";
const char kBundleWithoutRtcpMux[] = "RTCP-MUX must be enabled when BUNDLE "
                                     "is enabled.";
const char kInvalidCandidates[] = "Description contains invalid candidates.";
const char kInvalidSdp[] = "Invalid session description.";
const char kMlineMismatch[] =
    "Offer and answer descriptions m-lines are not matching. "
    "Rejecting answer.";
const char kSdpWithoutCrypto[] = "Called with a SDP without crypto enabled.";
const char kSdpWithoutSdesAndDtlsDisabled[] =
    "Called with a SDP without SDES crypto and DTLS disabled locally.";
const char kSessionError[] = "Session error code: ";
const char kUpdateStateFailed[] = "Failed to update session state: ";
const char kPushDownOfferTDFailed[] =
    "Failed to push down offer transport description.";
const char kPushDownPranswerTDFailed[] =
    "Failed to push down pranswer transport description.";
const char kPushDownAnswerTDFailed[] =
    "Failed to push down answer transport description.";

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
    if (media->cryptos().empty()) {
      if (!tinfo->description.identity_fingerprint) {
        // Crypto must be supplied.
        LOG(LS_WARNING) << "Session description must have SDES or DTLS-SRTP.";
        *error = kSdpWithoutCrypto;
        return false;
      }
      if (!dtls_enabled) {
        LOG(LS_WARNING) <<
            "Session description must have SDES when DTLS disabled.";
        *error = kSdpWithoutSdesAndDtlsDisabled;
        return false;
      }
    }
  }

  return true;
}

// Forces |sdesc->crypto_required| to the appropriate state based on the
// current security policy, to ensure a failure occurs if there is an error
// in crypto negotiation.
// Called when processing the local session description.
static void UpdateSessionDescriptionSecurePolicy(
    cricket::SecureMediaPolicy secure_policy,
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
        mdesc->set_crypto_required(secure_policy == cricket::SEC_REQUIRED);
      }
    }
  }
}

static bool GetAudioSsrcByTrackId(
    const SessionDescription* session_description,
    const std::string& track_id, uint32 *ssrc) {
  const cricket::ContentInfo* audio_info =
      cricket::GetFirstAudioContent(session_description);
  if (!audio_info) {
    LOG(LS_ERROR) << "Audio not used in this call";
    return false;
  }

  const cricket::MediaContentDescription* audio_content =
      static_cast<const cricket::MediaContentDescription*>(
          audio_info->description);
  cricket::StreamParams stream;
  if (!cricket::GetStreamByIds(audio_content->streams(), "", track_id,
                               &stream)) {
    return false;
  }
  *ssrc = stream.first_ssrc();
  return true;
}

static bool GetTrackIdBySsrc(const SessionDescription* session_description,
                             uint32 ssrc, std::string* track_id) {
  ASSERT(track_id != NULL);

  cricket::StreamParams stream_out;
  const cricket::ContentInfo* audio_info =
      cricket::GetFirstAudioContent(session_description);
  if (!audio_info) {
    return false;
  }
  const cricket::MediaContentDescription* audio_content =
      static_cast<const cricket::MediaContentDescription*>(
          audio_info->description);

  if (cricket::GetStreamBySsrc(audio_content->streams(), ssrc, &stream_out)) {
    *track_id = stream_out.id;
    return true;
  }

  const cricket::ContentInfo* video_info =
      cricket::GetFirstVideoContent(session_description);
  if (!video_info) {
    return false;
  }
  const cricket::MediaContentDescription* video_content =
      static_cast<const cricket::MediaContentDescription*>(
          video_info->description);

  if (cricket::GetStreamBySsrc(video_content->streams(), ssrc, &stream_out)) {
    *track_id = stream_out.id;
    return true;
  }
  return false;
}

static bool BadSdp(const std::string& desc, std::string* err_desc) {
  if (err_desc) {
    *err_desc = desc;
  }
  LOG(LS_ERROR) << desc;
  return false;
}

static bool BadLocalSdp(const std::string& desc, std::string* err_desc) {
  std::string set_local_sdp_failed = kSetLocalSdpFailed;
  set_local_sdp_failed.append(desc);
  return BadSdp(set_local_sdp_failed, err_desc);
}

static bool BadRemoteSdp(const std::string& desc, std::string* err_desc) {
  std::string set_remote_sdp_failed = kSetRemoteSdpFailed;
  set_remote_sdp_failed.append(desc);
  return BadSdp(set_remote_sdp_failed, err_desc);
}

static bool BadSdp(cricket::ContentSource source,
                   const std::string& desc, std::string* err_desc) {
  if (source == cricket::CS_LOCAL) {
    return BadLocalSdp(desc, err_desc);
  } else {
    return BadRemoteSdp(desc, err_desc);
  }
}

static std::string SessionErrorMsg(cricket::BaseSession::Error error) {
  std::ostringstream desc;
  desc << kSessionError << error;
  return desc.str();
}

#define GET_STRING_OF_STATE(state)  \
  case cricket::BaseSession::state:  \
    result = #state;  \
    break;

static std::string GetStateString(cricket::BaseSession::State state) {
  std::string result;
  switch (state) {
    GET_STRING_OF_STATE(STATE_INIT)
    GET_STRING_OF_STATE(STATE_SENTINITIATE)
    GET_STRING_OF_STATE(STATE_RECEIVEDINITIATE)
    GET_STRING_OF_STATE(STATE_SENTPRACCEPT)
    GET_STRING_OF_STATE(STATE_SENTACCEPT)
    GET_STRING_OF_STATE(STATE_RECEIVEDPRACCEPT)
    GET_STRING_OF_STATE(STATE_RECEIVEDACCEPT)
    GET_STRING_OF_STATE(STATE_SENTMODIFY)
    GET_STRING_OF_STATE(STATE_RECEIVEDMODIFY)
    GET_STRING_OF_STATE(STATE_SENTREJECT)
    GET_STRING_OF_STATE(STATE_RECEIVEDREJECT)
    GET_STRING_OF_STATE(STATE_SENTREDIRECT)
    GET_STRING_OF_STATE(STATE_SENTTERMINATE)
    GET_STRING_OF_STATE(STATE_RECEIVEDTERMINATE)
    GET_STRING_OF_STATE(STATE_INPROGRESS)
    GET_STRING_OF_STATE(STATE_DEINIT)
    default:
      ASSERT(false);
      break;
  }
  return result;
}

#define GET_STRING_OF_ERROR(err)  \
  case cricket::BaseSession::err:  \
    result = #err;  \
    break;

static std::string GetErrorString(cricket::BaseSession::Error err) {
  std::string result;
  switch (err) {
    GET_STRING_OF_ERROR(ERROR_NONE)
    GET_STRING_OF_ERROR(ERROR_TIME)
    GET_STRING_OF_ERROR(ERROR_RESPONSE)
    GET_STRING_OF_ERROR(ERROR_NETWORK)
    GET_STRING_OF_ERROR(ERROR_CONTENT)
    GET_STRING_OF_ERROR(ERROR_TRANSPORT)
    default:
      ASSERT(false);
      break;
  }
  return result;
}

static bool SetSessionStateFailed(cricket::ContentSource source,
                                  cricket::BaseSession::Error err,
                                  std::string* err_desc) {
  std::string set_state_err = kUpdateStateFailed;
  set_state_err.append(GetErrorString(err));
  return BadSdp(source, set_state_err, err_desc);
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

  void CheckForRemoteIceRestart(
      const SessionDescriptionInterface* old_desc,
      const SessionDescriptionInterface* new_desc) {
    if (!old_desc || new_desc->type() != SessionDescriptionInterface::kOffer) {
      return;
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
      if (new_transport_desc->ice_pwd != old_transport_desc->ice_pwd &&
          new_transport_desc->ice_ufrag != old_transport_desc->ice_ufrag) {
        LOG(LS_INFO) << "Remote peer request ice restart.";
        ice_restart_ = true;
        break;
      }
    }
  }

 private:
  bool ice_restart_;
};

WebRtcSession::WebRtcSession(
    cricket::ChannelManager* channel_manager,
    talk_base::Thread* signaling_thread,
    talk_base::Thread* worker_thread,
    cricket::PortAllocator* port_allocator,
    MediaStreamSignaling* mediastream_signaling)
    : cricket::BaseSession(signaling_thread, worker_thread, port_allocator,
                           talk_base::ToString(talk_base::CreateRandomId64() &
                                               LLONG_MAX),
                           cricket::NS_JINGLE_RTP, false),
      // RFC 3264: The numeric value of the session id and version in the
      // o line MUST be representable with a "64 bit signed integer".
      // Due to this constraint session id |sid_| is max limited to LLONG_MAX.
      channel_manager_(channel_manager),
      mediastream_signaling_(mediastream_signaling),
      ice_observer_(NULL),
      ice_connection_state_(PeerConnectionInterface::kIceConnectionNew),
      older_version_remote_peer_(false),
      dtls_enabled_(false),
      data_channel_type_(cricket::DCT_NONE),
      ice_restart_latch_(new IceRestartAnswerLatch) {
}

WebRtcSession::~WebRtcSession() {
  if (voice_channel_.get()) {
    SignalVoiceChannelDestroyed();
    channel_manager_->DestroyVoiceChannel(voice_channel_.release());
  }
  if (video_channel_.get()) {
    SignalVideoChannelDestroyed();
    channel_manager_->DestroyVideoChannel(video_channel_.release());
  }
  if (data_channel_.get()) {
    SignalDataChannelDestroyed();
    channel_manager_->DestroyDataChannel(data_channel_.release());
  }
  for (size_t i = 0; i < saved_candidates_.size(); ++i) {
    delete saved_candidates_[i];
  }
  delete identity();
}

bool WebRtcSession::Initialize(
    const MediaConstraintsInterface* constraints,
    DTLSIdentityServiceInterface* dtls_identity_service) {
  // TODO(perkj): Take |constraints| into consideration. Return false if not all
  // mandatory constraints can be fulfilled. Note that |constraints|
  // can be null.
  bool value;

  // Enable DTLS by default if |dtls_identity_service| is valid.
  dtls_enabled_ = (dtls_identity_service != NULL);
  // |constraints| can override the default |dtls_enabled_| value.
  if (FindConstraint(
        constraints,
        MediaConstraintsInterface::kEnableDtlsSrtp,
        &value, NULL)) {
    dtls_enabled_ = value;
  }

  // Enable creation of RTP data channels if the kEnableRtpDataChannels is set.
  // It takes precendence over the kEnableSctpDataChannels constraint.
  if (FindConstraint(
      constraints, MediaConstraintsInterface::kEnableRtpDataChannels,
      &value, NULL) && value) {
    LOG(LS_INFO) << "Allowing RTP data engine.";
    data_channel_type_ = cricket::DCT_RTP;
  } else {
    bool sctp_enabled = FindConstraint(
        constraints,
        MediaConstraintsInterface::kEnableSctpDataChannels,
        &value, NULL) && value;
    // DTLS has to be enabled to use SCTP.
    if (sctp_enabled && dtls_enabled_) {
      LOG(LS_INFO) << "Allowing SCTP data engine.";
      data_channel_type_ = cricket::DCT_SCTP;
    }
  }
  if (data_channel_type_ != cricket::DCT_NONE) {
    mediastream_signaling_->SetDataChannelFactory(this);
  }

  const cricket::VideoCodec default_codec(
      JsepSessionDescription::kDefaultVideoCodecId,
      JsepSessionDescription::kDefaultVideoCodecName,
      JsepSessionDescription::kMaxVideoCodecWidth,
      JsepSessionDescription::kMaxVideoCodecHeight,
      JsepSessionDescription::kDefaultVideoCodecFramerate,
      JsepSessionDescription::kDefaultVideoCodecPreference);
  channel_manager_->SetDefaultVideoEncoderConfig(
      cricket::VideoEncoderConfig(default_codec));

  webrtc_session_desc_factory_.reset(new WebRtcSessionDescriptionFactory(
      signaling_thread(),
      channel_manager_,
      mediastream_signaling_,
      dtls_identity_service,
      this,
      id(),
      data_channel_type_,
      dtls_enabled_));

  webrtc_session_desc_factory_->SignalIdentityReady.connect(
      this, &WebRtcSession::OnIdentityReady);
  return true;
}

void WebRtcSession::Terminate() {
  SetState(STATE_RECEIVEDTERMINATE);
  RemoveUnusedChannelsAndTransports(NULL);
  ASSERT(voice_channel_.get() == NULL);
  ASSERT(video_channel_.get() == NULL);
  ASSERT(data_channel_.get() == NULL);
}

bool WebRtcSession::StartCandidatesAllocation() {
  // SpeculativelyConnectTransportChannels, will call ConnectChannels method
  // from TransportProxy to start gathering ice candidates.
  SpeculativelyConnectAllTransportChannels();
  if (!saved_candidates_.empty()) {
    // If there are saved candidates which arrived before local description is
    // set, copy those to remote description.
    CopySavedCandidates(remote_desc_.get());
  }
  // Push remote candidates present in remote description to transport channels.
  UseCandidatesInSessionDescription(remote_desc_.get());
  return true;
}

void WebRtcSession::set_secure_policy(
    cricket::SecureMediaPolicy secure_policy) {
  webrtc_session_desc_factory_->set_secure(secure_policy);
}

cricket::SecureMediaPolicy WebRtcSession::secure_policy() const {
  return webrtc_session_desc_factory_->secure();
}

bool WebRtcSession::GetSslRole(talk_base::SSLRole* role) {
  if (local_description() == NULL || remote_description() == NULL) {
    LOG(LS_INFO) << "Local and Remote descriptions must be applied to get "
                 << "SSL Role of the session.";
    return false;
  }

  // TODO(mallinath) - Return role of each transport, as role may differ from
  // one another.
  // In current implementaion we just return the role of first transport in the
  // transport map.
  for (cricket::TransportMap::const_iterator iter = transport_proxies().begin();
       iter != transport_proxies().end(); ++iter) {
    if (iter->second->impl()) {
      return iter->second->impl()->GetSslRole(role);
    }
  }
  return false;
}

void WebRtcSession::CreateOffer(CreateSessionDescriptionObserver* observer,
                                const MediaConstraintsInterface* constraints) {
  webrtc_session_desc_factory_->CreateOffer(observer, constraints);
}

void WebRtcSession::CreateAnswer(CreateSessionDescriptionObserver* observer,
                                 const MediaConstraintsInterface* constraints) {
  webrtc_session_desc_factory_->CreateAnswer(observer, constraints);
}

bool WebRtcSession::SetLocalDescription(SessionDescriptionInterface* desc,
                                        std::string* err_desc) {
  // Takes the ownership of |desc| regardless of the result.
  talk_base::scoped_ptr<SessionDescriptionInterface> desc_temp(desc);

  // Validate SDP.
  if (!ValidateSessionDescription(desc, cricket::CS_LOCAL, err_desc)) {
    return false;
  }

  // Update the initiator flag if this session is the initiator.
  Action action = GetAction(desc->type());
  if (state() == STATE_INIT && action == kOffer) {
    set_initiator(true);
  }

  cricket::SecureMediaPolicy secure_policy =
      webrtc_session_desc_factory_->secure();
  // Update the MediaContentDescription crypto settings as per the policy set.
  UpdateSessionDescriptionSecurePolicy(secure_policy, desc->description());

  set_local_description(desc->description()->Copy());
  local_desc_.reset(desc_temp.release());

  // Transport and Media channels will be created only when offer is set.
  if (action == kOffer && !CreateChannels(local_desc_->description())) {
    // TODO(mallinath) - Handle CreateChannel failure, as new local description
    // is applied. Restore back to old description.
    return BadLocalSdp(kCreateChannelFailed, err_desc);
  }

  // Remove channel and transport proxies, if MediaContentDescription is
  // rejected.
  RemoveUnusedChannelsAndTransports(local_desc_->description());

  if (!UpdateSessionState(action, cricket::CS_LOCAL,
                          local_desc_->description(), err_desc)) {
    return false;
  }
  // Kick starting the ice candidates allocation.
  StartCandidatesAllocation();

  // Update state and SSRC of local MediaStreams and DataChannels based on the
  // local session description.
  mediastream_signaling_->OnLocalDescriptionChanged(local_desc_.get());

  if (error() != cricket::BaseSession::ERROR_NONE) {
    return BadLocalSdp(SessionErrorMsg(error()), err_desc);
  }
  return true;
}

bool WebRtcSession::SetRemoteDescription(SessionDescriptionInterface* desc,
                                         std::string* err_desc) {
  // Takes the ownership of |desc| regardless of the result.
  talk_base::scoped_ptr<SessionDescriptionInterface> desc_temp(desc);

  // Validate SDP.
  if (!ValidateSessionDescription(desc, cricket::CS_REMOTE, err_desc)) {
    return false;
  }

  // Transport and Media channels will be created only when offer is set.
  Action action = GetAction(desc->type());
  if (action == kOffer && !CreateChannels(desc->description())) {
    // TODO(mallinath) - Handle CreateChannel failure, as new local description
    // is applied. Restore back to old description.
    return BadRemoteSdp(kCreateChannelFailed, err_desc);
  }

  // Remove channel and transport proxies, if MediaContentDescription is
  // rejected.
  RemoveUnusedChannelsAndTransports(desc->description());

  // NOTE: Candidates allocation will be initiated only when SetLocalDescription
  // is called.
  set_remote_description(desc->description()->Copy());
  if (!UpdateSessionState(action, cricket::CS_REMOTE,
                          desc->description(), err_desc)) {
    return false;
  }

  // Update remote MediaStreams.
  mediastream_signaling_->OnRemoteDescriptionChanged(desc);
  if (local_description() && !UseCandidatesInSessionDescription(desc)) {
    return BadRemoteSdp(kInvalidCandidates, err_desc);
  }

  // Copy all saved candidates.
  CopySavedCandidates(desc);
  // We retain all received candidates.
  WebRtcSessionDescriptionFactory::CopyCandidatesFromSessionDescription(
      remote_desc_.get(), desc);
  // Check if this new SessionDescription contains new ice ufrag and password
  // that indicates the remote peer requests ice restart.
  ice_restart_latch_->CheckForRemoteIceRestart(remote_desc_.get(),
                                               desc);
  remote_desc_.reset(desc_temp.release());
  if (error() != cricket::BaseSession::ERROR_NONE) {
    return BadRemoteSdp(SessionErrorMsg(error()), err_desc);
  }
  return true;
}

bool WebRtcSession::UpdateSessionState(
    Action action, cricket::ContentSource source,
    const cricket::SessionDescription* desc,
    std::string* err_desc) {
  // If there's already a pending error then no state transition should happen.
  // But all call-sites should be verifying this before calling us!
  ASSERT(error() == cricket::BaseSession::ERROR_NONE);
  if (action == kOffer) {
    if (!PushdownTransportDescription(source, cricket::CA_OFFER)) {
      return BadSdp(source, kPushDownOfferTDFailed, err_desc);
    }
    SetState(source == cricket::CS_LOCAL ?
        STATE_SENTINITIATE : STATE_RECEIVEDINITIATE);
    if (error() != cricket::BaseSession::ERROR_NONE) {
      return SetSessionStateFailed(source, error(), err_desc);
    }
  } else if (action == kPrAnswer) {
    if (!PushdownTransportDescription(source, cricket::CA_PRANSWER)) {
      return BadSdp(source, kPushDownPranswerTDFailed, err_desc);
    }
    EnableChannels();
    SetState(source == cricket::CS_LOCAL ?
        STATE_SENTPRACCEPT : STATE_RECEIVEDPRACCEPT);
    if (error() != cricket::BaseSession::ERROR_NONE) {
      return SetSessionStateFailed(source, error(), err_desc);
    }
  } else if (action == kAnswer) {
    if (!PushdownTransportDescription(source, cricket::CA_ANSWER)) {
      return BadSdp(source, kPushDownAnswerTDFailed, err_desc);
    }
    MaybeEnableMuxingSupport();
    EnableChannels();
    SetState(source == cricket::CS_LOCAL ?
        STATE_SENTACCEPT : STATE_RECEIVEDACCEPT);
    if (error() != cricket::BaseSession::ERROR_NONE) {
      return SetSessionStateFailed(source, error(), err_desc);
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

bool WebRtcSession::ProcessIceMessage(const IceCandidateInterface* candidate) {
  if (state() == STATE_INIT) {
     LOG(LS_ERROR) << "ProcessIceMessage: ICE candidates can't be added "
                   << "without any offer (local or remote) "
                   << "session description.";
     return false;
  }

  if (!candidate) {
    LOG(LS_ERROR) << "ProcessIceMessage: Candidate is NULL";
    return false;
  }

  if (!local_description() || !remote_description()) {
    LOG(LS_INFO) << "ProcessIceMessage: Remote description not set, "
                 << "save the candidate for later use.";
    saved_candidates_.push_back(
        new JsepIceCandidate(candidate->sdp_mid(), candidate->sdp_mline_index(),
                             candidate->candidate()));
    return true;
  }

  // Add this candidate to the remote session description.
  if (!remote_desc_->AddCandidate(candidate)) {
    LOG(LS_ERROR) << "ProcessIceMessage: Candidate cannot be used";
    return false;
  }

  return UseCandidatesInSessionDescription(remote_desc_.get());
}

bool WebRtcSession::GetTrackIdBySsrc(uint32 ssrc, std::string* id) {
  if (GetLocalTrackId(ssrc, id)) {
    if (GetRemoteTrackId(ssrc, id)) {
      LOG(LS_WARNING) << "SSRC " << ssrc
                      << " exists in both local and remote descriptions";
      return true;  // We return the remote track id.
    }
    return true;
  } else {
    return GetRemoteTrackId(ssrc, id);
  }
}

bool WebRtcSession::GetLocalTrackId(uint32 ssrc, std::string* track_id) {
  if (!BaseSession::local_description())
    return false;
  return webrtc::GetTrackIdBySsrc(
    BaseSession::local_description(), ssrc, track_id);
}

bool WebRtcSession::GetRemoteTrackId(uint32 ssrc, std::string* track_id) {
  if (!BaseSession::remote_description())
      return false;
  return webrtc::GetTrackIdBySsrc(
    BaseSession::remote_description(), ssrc, track_id);
}

std::string WebRtcSession::BadStateErrMsg(
    const std::string& type, State state) {
  std::ostringstream desc;
  desc << "Called with type in wrong state, "
       << "type: " << type << " state: " << GetStateString(state);
  return desc.str();
}

void WebRtcSession::SetAudioPlayout(uint32 ssrc, bool enable,
                                    cricket::AudioRenderer* renderer) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_) {
    LOG(LS_ERROR) << "SetAudioPlayout: No audio channel exists.";
    return;
  }
  if (!voice_channel_->SetRemoteRenderer(ssrc, renderer)) {
    // SetRenderer() can fail if the ssrc does not match any playout channel.
    LOG(LS_ERROR) << "SetAudioPlayout: ssrc is incorrect: " << ssrc;
    return;
  }
  if (!voice_channel_->SetOutputScaling(ssrc, enable ? 1 : 0, enable ? 1 : 0)) {
    // Allow that SetOutputScaling fail if |enable| is false but assert
    // otherwise. This in the normal case when the underlying media channel has
    // already been deleted.
    ASSERT(enable == false);
  }
}

void WebRtcSession::SetAudioSend(uint32 ssrc, bool enable,
                                 const cricket::AudioOptions& options,
                                 cricket::AudioRenderer* renderer) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_) {
    LOG(LS_ERROR) << "SetAudioSend: No audio channel exists.";
    return;
  }
  if (!voice_channel_->SetLocalRenderer(ssrc, renderer)) {
    // SetRenderer() can fail if the ssrc does not match any send channel.
    LOG(LS_ERROR) << "SetAudioSend: ssrc is incorrect: " << ssrc;
    return;
  }
  if (!voice_channel_->MuteStream(ssrc, !enable)) {
    // Allow that MuteStream fail if |enable| is false but assert otherwise.
    // This in the normal case when the underlying media channel has already
    // been deleted.
    ASSERT(enable == false);
    return;
  }
  if (enable)
    voice_channel_->SetChannelOptions(options);
}

bool WebRtcSession::SetCaptureDevice(uint32 ssrc,
                                     cricket::VideoCapturer* camera) {
  ASSERT(signaling_thread()->IsCurrent());

  if (!video_channel_.get()) {
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

void WebRtcSession::SetVideoPlayout(uint32 ssrc,
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

void WebRtcSession::SetVideoSend(uint32 ssrc, bool enable,
                                 const cricket::VideoOptions* options) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!video_channel_) {
    LOG(LS_WARNING) << "SetVideoSend: No video channel exists.";
    return;
  }
  if (!video_channel_->MuteStream(ssrc, !enable)) {
    // Allow that MuteStream fail if |enable| is false but assert otherwise.
    // This in the normal case when the underlying media channel has already
    // been deleted.
    ASSERT(enable == false);
    return;
  }
  if (enable && options)
    video_channel_->SetChannelOptions(*options);
}

bool WebRtcSession::CanInsertDtmf(const std::string& track_id) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_) {
    LOG(LS_ERROR) << "CanInsertDtmf: No audio channel exists.";
    return false;
  }
  uint32 send_ssrc = 0;
  // The Dtmf is negotiated per channel not ssrc, so we only check if the ssrc
  // exists.
  if (!GetAudioSsrcByTrackId(BaseSession::local_description(), track_id,
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
  uint32 send_ssrc = 0;
  if (!VERIFY(GetAudioSsrcByTrackId(BaseSession::local_description(),
                                    track_id, &send_ssrc))) {
    LOG(LS_ERROR) << "InsertDtmf: Track does not exist: " << track_id;
    return false;
  }
  if (!voice_channel_->InsertDtmf(send_ssrc, code, duration,
                                  cricket::DF_SEND)) {
    LOG(LS_ERROR) << "Failed to insert DTMF to channel.";
    return false;
  }
  return true;
}

sigslot::signal0<>* WebRtcSession::GetOnDestroyedSignal() {
  return &SignalVoiceChannelDestroyed;
}

talk_base::scoped_refptr<DataChannel> WebRtcSession::CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config) {
  if (state() == STATE_RECEIVEDTERMINATE) {
    return NULL;
  }
  if (data_channel_type_ == cricket::DCT_NONE) {
    LOG(LS_ERROR) << "CreateDataChannel: Data is not supported in this call.";
    return NULL;
  }
  DataChannelInit new_config = config ? (*config) : DataChannelInit();

  if (data_channel_type_ == cricket::DCT_SCTP) {
    if (new_config.id < 0) {
      if (!mediastream_signaling_->AllocateSctpId(&new_config.id)) {
        LOG(LS_ERROR) << "No id can be allocated for the SCTP data channel.";
        return NULL;
      }
    } else if (!mediastream_signaling_->IsSctpIdAvailable(new_config.id)) {
      LOG(LS_ERROR) << "Failed to create a SCTP data channel "
                    << "because the id is already in use or out of range.";
      return NULL;
    }
  }

  talk_base::scoped_refptr<DataChannel> channel(
      DataChannel::Create(this, label, &new_config));
  if (channel == NULL)
    return NULL;
  if (!mediastream_signaling_->AddDataChannel(channel))
    return NULL;
  if (data_channel_type_ == cricket::DCT_SCTP) {
    if (config == NULL) {
      LOG(LS_WARNING) << "Could not send data channel OPEN message"
                      << " because of NULL config.";
      return NULL;
    }
    if (data_channel_.get()) {
      channel->SetReceiveSsrc(new_config.id);
      channel->SetSendSsrc(new_config.id);
    }
    if (!config->negotiated) {
      talk_base::Buffer *payload = new talk_base::Buffer;
      if (!mediastream_signaling_->WriteDataChannelOpenMessage(
              label, *config, payload)) {
        LOG(LS_WARNING) << "Could not write data channel OPEN message";
      }
      // SendControl may queue the message until the data channel's set up,
      // or congestion clears.
      channel->SendControl(payload);
    }
  }
  return channel;
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

void WebRtcSession::OnIdentityReady(talk_base::SSLIdentity* identity) {
  SetIdentity(identity);
}

bool WebRtcSession::waiting_for_identity() const {
  return webrtc_session_desc_factory_->waiting_for_identity();
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

void WebRtcSession::OnTransportRequestSignaling(
    cricket::Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
  transport->OnSignalingReady();
  if (ice_observer_) {
    ice_observer_->OnIceGatheringChange(
      PeerConnectionInterface::kIceGatheringGathering);
  }
}

void WebRtcSession::OnTransportConnecting(cricket::Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
  // start monitoring for the write state of the transport.
  OnTransportWritable(transport);
}

void WebRtcSession::OnTransportWritable(cricket::Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
  // TODO(bemasc): Expose more API from Transport to detect when
  // candidate selection starts or stops, due to success or failure.
  if (transport->all_channels_writable()) {
    if (ice_connection_state_ ==
            PeerConnectionInterface::kIceConnectionChecking ||
        ice_connection_state_ ==
            PeerConnectionInterface::kIceConnectionDisconnected) {
      SetIceConnectionState(PeerConnectionInterface::kIceConnectionConnected);
    }
  } else if (transport->HasChannels()) {
    // If the current state is Connected or Completed, then there were writable
    // channels but now there are not, so the next state must be Disconnected.
    if (ice_connection_state_ ==
            PeerConnectionInterface::kIceConnectionConnected ||
        ice_connection_state_ ==
            PeerConnectionInterface::kIceConnectionCompleted) {
      SetIceConnectionState(
          PeerConnectionInterface::kIceConnectionDisconnected);
    }
  }
}

void WebRtcSession::OnTransportProxyCandidatesReady(
    cricket::TransportProxy* proxy, const cricket::Candidates& candidates) {
  ASSERT(signaling_thread()->IsCurrent());
  ProcessNewLocalCandidate(proxy->content_name(), candidates);
}

void WebRtcSession::OnCandidatesAllocationDone() {
  ASSERT(signaling_thread()->IsCurrent());
  if (ice_observer_) {
    ice_observer_->OnIceGatheringChange(
      PeerConnectionInterface::kIceGatheringComplete);
    ice_observer_->OnIceComplete();
  }
}

// Enabling voice and video channel.
void WebRtcSession::EnableChannels() {
  if (voice_channel_ && !voice_channel_->enabled())
    voice_channel_->Enable(true);

  if (video_channel_ && !video_channel_->enabled())
    video_channel_->Enable(true);

  if (data_channel_.get() && !data_channel_->enabled())
    data_channel_->Enable(true);
}

void WebRtcSession::ProcessNewLocalCandidate(
    const std::string& content_name,
    const cricket::Candidates& candidates) {
  int sdp_mline_index;
  if (!GetLocalCandidateMediaIndex(content_name, &sdp_mline_index)) {
    LOG(LS_ERROR) << "ProcessNewLocalCandidate: content name "
                  << content_name << " not found";
    return;
  }

  for (cricket::Candidates::const_iterator citer = candidates.begin();
      citer != candidates.end(); ++citer) {
    // Use content_name as the candidate media id.
    JsepIceCandidate candidate(content_name, sdp_mline_index, *citer);
    if (ice_observer_) {
      ice_observer_->OnIceCandidate(&candidate);
    }
    if (local_desc_) {
      local_desc_->AddCandidate(&candidate);
    }
  }
}

// Returns the media index for a local ice candidate given the content name.
bool WebRtcSession::GetLocalCandidateMediaIndex(const std::string& content_name,
                                                int* sdp_mline_index) {
  if (!BaseSession::local_description() || !sdp_mline_index)
    return false;

  bool content_found = false;
  const ContentInfos& contents = BaseSession::local_description()->contents();
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
  if (!remote_desc)
    return true;
  bool ret = true;
  for (size_t m = 0; m < remote_desc->number_of_mediasections(); ++m) {
    const IceCandidateCollection* candidates = remote_desc->candidates(m);
    for  (size_t n = 0; n < candidates->count(); ++n) {
      ret = UseCandidate(candidates->at(n));
      if (!ret)
        break;
    }
  }
  return ret;
}

bool WebRtcSession::UseCandidate(
    const IceCandidateInterface* candidate) {

  size_t mediacontent_index = static_cast<size_t>(candidate->sdp_mline_index());
  size_t remote_content_size =
      BaseSession::remote_description()->contents().size();
  if (mediacontent_index >= remote_content_size) {
    LOG(LS_ERROR)
        << "UseRemoteCandidateInSession: Invalid candidate media index.";
    return false;
  }

  cricket::ContentInfo content =
      BaseSession::remote_description()->contents()[mediacontent_index];
  std::vector<cricket::Candidate> candidates;
  candidates.push_back(candidate->candidate());
  // Invoking BaseSession method to handle remote candidates.
  std::string error;
  if (OnRemoteCandidates(content.name, candidates, &error)) {
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
    LOG(LS_WARNING) << error;
  }
  return true;
}

void WebRtcSession::RemoveUnusedChannelsAndTransports(
    const SessionDescription* desc) {
  const cricket::ContentInfo* voice_info =
      cricket::GetFirstAudioContent(desc);
  if ((!voice_info || voice_info->rejected) && voice_channel_) {
    mediastream_signaling_->OnAudioChannelClose();
    SignalVoiceChannelDestroyed();
    const std::string content_name = voice_channel_->content_name();
    channel_manager_->DestroyVoiceChannel(voice_channel_.release());
    DestroyTransportProxy(content_name);
  }

  const cricket::ContentInfo* video_info =
      cricket::GetFirstVideoContent(desc);
  if ((!video_info || video_info->rejected) && video_channel_) {
    mediastream_signaling_->OnVideoChannelClose();
    SignalVideoChannelDestroyed();
    const std::string content_name = video_channel_->content_name();
    channel_manager_->DestroyVideoChannel(video_channel_.release());
    DestroyTransportProxy(content_name);
  }

  const cricket::ContentInfo* data_info =
      cricket::GetFirstDataContent(desc);
  if ((!data_info || data_info->rejected) && data_channel_) {
    mediastream_signaling_->OnDataChannelClose();
    SignalDataChannelDestroyed();
    const std::string content_name = data_channel_->content_name();
    channel_manager_->DestroyDataChannel(data_channel_.release());
    DestroyTransportProxy(content_name);
  }
}

// TODO(mallinath) - Add a correct error code if the channels are not creatued
// due to BUNDLE is enabled but rtcp-mux is disabled.
bool WebRtcSession::CreateChannels(const SessionDescription* desc) {
  // Disabling the BUNDLE flag in PortAllocator if offer disabled it.
  bool bundle_enabled = desc->HasGroup(cricket::GROUP_TYPE_BUNDLE);
  if (state() == STATE_INIT && !bundle_enabled) {
    port_allocator()->set_flags(port_allocator()->flags() &
                                ~cricket::PORTALLOCATOR_ENABLE_BUNDLE);
  }

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
      data && !data->rejected && !data_channel_.get()) {
    if (!CreateDataChannel(data)) {
      LOG(LS_ERROR) << "Failed to create data channel.";
      return false;
    }
  }

  return true;
}

bool WebRtcSession::CreateVoiceChannel(const cricket::ContentInfo* content) {
  voice_channel_.reset(channel_manager_->CreateVoiceChannel(
      this, content->name, true));
  return (voice_channel_ != NULL);
}

bool WebRtcSession::CreateVideoChannel(const cricket::ContentInfo* content) {
  video_channel_.reset(channel_manager_->CreateVideoChannel(
      this, content->name, true, voice_channel_.get()));
  return (video_channel_ != NULL);
}

bool WebRtcSession::CreateDataChannel(const cricket::ContentInfo* content) {
  bool rtcp = (data_channel_type_ == cricket::DCT_RTP);
  data_channel_.reset(channel_manager_->CreateDataChannel(
      this, content->name, rtcp, data_channel_type_));
  if (!data_channel_.get()) {
    return false;
  }
  data_channel_->SignalDataReceived.connect(
      this, &WebRtcSession::OnDataReceived);
  return true;
}

void WebRtcSession::CopySavedCandidates(
    SessionDescriptionInterface* dest_desc) {
  if (!dest_desc) {
    ASSERT(false);
    return;
  }
  for (size_t i = 0; i < saved_candidates_.size(); ++i) {
    dest_desc->AddCandidate(saved_candidates_[i]);
    delete saved_candidates_[i];
  }
  saved_candidates_.clear();
}

// Look for OPEN messages and set up data channels in response.
void WebRtcSession::OnDataReceived(
    cricket::DataChannel* channel,
    const cricket::ReceiveDataParams& params,
    const talk_base::Buffer& payload) {
  if (params.type != cricket::DMT_CONTROL) {
    return;
  }

  std::string label;
  DataChannelInit config;
  if (!mediastream_signaling_->ParseDataChannelOpenMessage(
          payload, &label, &config)) {
    LOG(LS_WARNING) << "Failed to parse data channel OPEN message.";
    return;
  }

  config.negotiated = true;  // This is the negotiation.

  if (!mediastream_signaling_->AddDataChannelFromOpenMessage(
          label, config)) {
    LOG(LS_WARNING) << "Failed to create data channel from OPEN message.";
    return;
  }
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
    cricket::ContentSource source, std::string* error_desc) {

  if (error() != cricket::BaseSession::ERROR_NONE) {
    return BadSdp(source, SessionErrorMsg(error()), error_desc);
  }

  if (!sdesc || !sdesc->description()) {
    return BadSdp(source, kInvalidSdp, error_desc);
  }

  std::string type = sdesc->type();
  Action action = GetAction(sdesc->type());
  if (source == cricket::CS_LOCAL) {
    if (!ExpectSetLocalDescription(action))
      return BadSdp(source, BadStateErrMsg(type, state()), error_desc);
  } else {
    if (!ExpectSetRemoteDescription(action))
      return BadSdp(source, BadStateErrMsg(type, state()), error_desc);
  }

  // Verify crypto settings.
  std::string crypto_error;
  if (webrtc_session_desc_factory_->secure() == cricket::SEC_REQUIRED &&
      !VerifyCrypto(sdesc->description(), dtls_enabled_, &crypto_error)) {
    return BadSdp(source, crypto_error, error_desc);
  }

  if (!ValidateBundleSettings(sdesc->description())) {
    return BadSdp(source, kBundleWithoutRtcpMux, error_desc);
  }

  // Verify m-lines in Answer when compared against Offer.
  if (action == kAnswer) {
    const cricket::SessionDescription* offer_desc =
        (source == cricket::CS_LOCAL) ? remote_description()->description() :
            local_description()->description();
    if (!VerifyMediaDescriptions(sdesc->description(), offer_desc)) {
      return BadSdp(source, kMlineMismatch, error_desc);
    }
  }

  return true;
}

bool WebRtcSession::ExpectSetLocalDescription(Action action) {
  return ((action == kOffer && state() == STATE_INIT) ||
          // update local offer
          (action == kOffer && state() == STATE_SENTINITIATE) ||
          // update the current ongoing session.
          (action == kOffer && state() == STATE_RECEIVEDACCEPT) ||
          (action == kOffer && state() == STATE_SENTACCEPT) ||
          (action == kOffer && state() == STATE_INPROGRESS) ||
          // accept remote offer
          (action == kAnswer && state() == STATE_RECEIVEDINITIATE) ||
          (action == kAnswer && state() == STATE_SENTPRACCEPT) ||
          (action == kPrAnswer && state() == STATE_RECEIVEDINITIATE) ||
          (action == kPrAnswer && state() == STATE_SENTPRACCEPT));
}

bool WebRtcSession::ExpectSetRemoteDescription(Action action) {
  return ((action == kOffer && state() == STATE_INIT) ||
          // update remote offer
          (action == kOffer && state() == STATE_RECEIVEDINITIATE) ||
          // update the current ongoing session
          (action == kOffer && state() == STATE_RECEIVEDACCEPT) ||
          (action == kOffer && state() == STATE_SENTACCEPT) ||
          (action == kOffer && state() == STATE_INPROGRESS) ||
          // accept local offer
          (action == kAnswer && state() == STATE_SENTINITIATE) ||
          (action == kAnswer && state() == STATE_RECEIVEDPRACCEPT) ||
          (action == kPrAnswer && state() == STATE_SENTINITIATE) ||
          (action == kPrAnswer && state() == STATE_RECEIVEDPRACCEPT));
}

}  // namespace webrtc
