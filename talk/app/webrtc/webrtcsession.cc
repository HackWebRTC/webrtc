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

typedef cricket::MediaSessionOptions::Stream Stream;
typedef cricket::MediaSessionOptions::Streams Streams;

namespace webrtc {

static const uint64 kInitSessionVersion = 2;

const char kInternalConstraintPrefix[] = "internal";

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

// Arbitrary constant used as prefix for the identity.
// Chosen to make the certificates more readable.
const char kWebRTCIdentityPrefix[] = "WebRTC";

// Error messages
const char kSetLocalSdpFailed[] = "SetLocalDescription failed: ";
const char kSetRemoteSdpFailed[] = "SetRemoteDescription failed: ";
const char kCreateChannelFailed[] = "Failed to create channels.";
const char kInvalidCandidates[] = "Description contains invalid candidates.";
const char kInvalidSdp[] = "Invalid session description.";
const char kMlineMismatch[] =
    "Offer and answer descriptions m-lines are not matching. "
    "Rejecting answer.";
const char kSdpWithoutCrypto[] = "Called with a SDP without crypto enabled.";
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

static void CopyCandidatesFromSessionDescription(
    const SessionDescriptionInterface* source_desc,
    SessionDescriptionInterface* dest_desc) {
  if (!source_desc)
    return;
  for (size_t m = 0; m < source_desc->number_of_mediasections() &&
                     m < dest_desc->number_of_mediasections(); ++m) {
    const IceCandidateCollection* source_candidates =
        source_desc->candidates(m);
    const IceCandidateCollection* dest_candidates = dest_desc->candidates(m);
    for  (size_t n = 0; n < source_candidates->count(); ++n) {
      const IceCandidateInterface* new_candidate = source_candidates->at(n);
      if (!dest_candidates->HasCandidate(new_candidate))
        dest_desc->AddCandidate(source_candidates->at(n));
    }
  }
}

// Checks that each non-rejected content has SDES crypto keys or a DTLS
// fingerprint. Mismatches, such as replying with a DTLS fingerprint to SDES
// keys, will be caught in Transport negotiation, and backstopped by Channel's
// |secure_required| check.
static bool VerifyCrypto(const SessionDescription* desc) {
  if (!desc) {
    return false;
  }
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
      return false;
    }
    if (media->cryptos().empty() &&
        !tinfo->description.identity_fingerprint) {
      // Crypto must be supplied.
      LOG(LS_WARNING) << "Session description must have SDES or DTLS-SRTP.";
      return false;
    }
  }

  return true;
}

static bool CompareStream(const Stream& stream1, const Stream& stream2) {
  return (stream1.id < stream2.id);
}

static bool SameId(const Stream& stream1, const Stream& stream2) {
  return (stream1.id == stream2.id);
}

// Checks if each Stream within the |streams| has unique id.
static bool ValidStreams(const Streams& streams) {
  Streams sorted_streams = streams;
  std::sort(sorted_streams.begin(), sorted_streams.end(), CompareStream);
  Streams::iterator it =
      std::adjacent_find(sorted_streams.begin(), sorted_streams.end(),
                         SameId);
  return (it == sorted_streams.end());
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

  // Returns true if CheckForRemoteIceRestart has been called since last
  // time this method was called with a new session description where
  // ice password and ufrag has changed.
  bool AnswerWithIceRestartLatch() {
    if (ice_restart_) {
      ice_restart_ = false;
      return true;
    }
    return false;
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

WebRtcSession::WebRtcSession(cricket::ChannelManager* channel_manager,
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
      session_desc_factory_(channel_manager, &transport_desc_factory_),
      mediastream_signaling_(mediastream_signaling),
      ice_observer_(NULL),
      ice_connection_state_(PeerConnectionInterface::kIceConnectionNew),
      // RFC 4566 suggested a Network Time Protocol (NTP) format timestamp
      // as the session id and session version. To simplify, it should be fine
      // to just use a random number as session id and start version from
      // |kInitSessionVersion|.
      session_version_(kInitSessionVersion),
      older_version_remote_peer_(false),
      data_channel_type_(cricket::DCT_NONE),
      ice_restart_latch_(new IceRestartAnswerLatch) {
  transport_desc_factory_.set_protocol(cricket::ICEPROTO_HYBRID);
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
  set_identity(NULL);
  transport_desc_factory_.set_identity(NULL);
}

bool WebRtcSession::Initialize(const MediaConstraintsInterface* constraints) {
  // TODO(perkj): Take |constraints| into consideration. Return false if not all
  // mandatory constraints can be fulfilled. Note that |constraints|
  // can be null.

  // By default SRTP-SDES is enabled in WebRtc.
  set_secure_policy(cricket::SEC_REQUIRED);

  // Enable DTLS-SRTP if the constraint is set.
  bool value;
  if (FindConstraint(constraints, MediaConstraintsInterface::kEnableDtlsSrtp,
      &value, NULL) && value) {
    LOG(LS_INFO) << "DTLS-SRTP enabled; generating identity";
    std::string identity_name = kWebRTCIdentityPrefix +
        talk_base::ToString(talk_base::CreateRandomId());
    transport_desc_factory_.set_identity(talk_base::SSLIdentity::Generate(
        identity_name));
    LOG(LS_INFO) << "Finished generating identity";
    set_identity(transport_desc_factory_.identity());
    transport_desc_factory_.set_digest_algorithm(talk_base::DIGEST_SHA_256);

    transport_desc_factory_.set_secure(cricket::SEC_ENABLED);
  }

  // Enable creation of RTP data channels if the kEnableRtpDataChannels is set.
  // It takes precendence over the kEnableSctpDataChannels constraint.
  if (FindConstraint(
      constraints, MediaConstraintsInterface::kEnableRtpDataChannels,
      &value, NULL) && value) {
    LOG(LS_INFO) << "Allowing RTP data engine.";
    data_channel_type_ = cricket::DCT_RTP;
  } else if (
      FindConstraint(
          constraints,
          MediaConstraintsInterface::kEnableSctpDataChannels,
          &value, NULL) && value &&
      // DTLS has to be enabled to use SCTP.
      (transport_desc_factory_.secure() == cricket::SEC_ENABLED)) {
    LOG(LS_INFO) << "Allowing SCTP data engine.";
    data_channel_type_ = cricket::DCT_SCTP;
  }
  if (data_channel_type_ != cricket::DCT_NONE) {
    mediastream_signaling_->SetDataChannelFactory(this);
  }

  // Make sure SessionDescriptions only contains the StreamParams we negotiate.
  session_desc_factory_.set_add_legacy_streams(false);

  const cricket::VideoCodec default_codec(
      JsepSessionDescription::kDefaultVideoCodecId,
      JsepSessionDescription::kDefaultVideoCodecName,
      JsepSessionDescription::kMaxVideoCodecWidth,
      JsepSessionDescription::kMaxVideoCodecHeight,
      JsepSessionDescription::kDefaultVideoCodecFramerate,
      JsepSessionDescription::kDefaultVideoCodecPreference);
  channel_manager_->SetDefaultVideoEncoderConfig(
      cricket::VideoEncoderConfig(default_codec));
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
  session_desc_factory_.set_secure(secure_policy);
}

SessionDescriptionInterface* WebRtcSession::CreateOffer(
    const MediaConstraintsInterface* constraints) {
  cricket::MediaSessionOptions options;

  if (!mediastream_signaling_->GetOptionsForOffer(constraints, &options)) {
    LOG(LS_ERROR) << "CreateOffer called with invalid constraints.";
    return NULL;
  }

  if (!ValidStreams(options.streams)) {
    LOG(LS_ERROR) << "CreateOffer called with invalid media streams.";
    return NULL;
  }

  if (data_channel_type_ == cricket::DCT_SCTP) {
    options.data_channel_type = cricket::DCT_SCTP;
  }
  SessionDescription* desc(
      session_desc_factory_.CreateOffer(options,
                                        BaseSession::local_description()));
  // RFC 3264
  // When issuing an offer that modifies the session,
  // the "o=" line of the new SDP MUST be identical to that in the
  // previous SDP, except that the version in the origin field MUST
  // increment by one from the previous SDP.

  // Just increase the version number by one each time when a new offer
  // is created regardless if it's identical to the previous one or not.
  // The |session_version_| is a uint64, the wrap around should not happen.
  ASSERT(session_version_ + 1 > session_version_);
  JsepSessionDescription* offer(new JsepSessionDescription(
      JsepSessionDescription::kOffer));
  if (!offer->Initialize(desc, id(),
                         talk_base::ToString(session_version_++))) {
    delete offer;
    return NULL;
  }
  if (local_description() && !options.transport_options.ice_restart) {
    // Include all local ice candidates in the SessionDescription unless
    // the an ice restart has been requested.
    CopyCandidatesFromSessionDescription(local_description(), offer);
  }
  return offer;
}

SessionDescriptionInterface* WebRtcSession::CreateAnswer(
    const MediaConstraintsInterface* constraints) {
  if (!remote_description()) {
    LOG(LS_ERROR) << "CreateAnswer can't be called before"
                  << " SetRemoteDescription.";
    return NULL;
  }
  if (remote_description()->type() != JsepSessionDescription::kOffer) {
    LOG(LS_ERROR) << "CreateAnswer failed because remote_description is not an"
                  << " offer.";
    return NULL;
  }

  cricket::MediaSessionOptions options;
  if (!mediastream_signaling_->GetOptionsForAnswer(constraints, &options)) {
    LOG(LS_ERROR) << "CreateAnswer called with invalid constraints.";
    return NULL;
  }
  if (!ValidStreams(options.streams)) {
    LOG(LS_ERROR) << "CreateAnswer called with invalid media streams.";
    return NULL;
  }
  if (data_channel_type_ == cricket::DCT_SCTP) {
    options.data_channel_type = cricket::DCT_SCTP;
  }
  // According to http://tools.ietf.org/html/rfc5245#section-9.2.1.1
  // an answer should also contain new ice ufrag and password if an offer has
  // been received with new ufrag and password.
  options.transport_options.ice_restart =
      ice_restart_latch_->AnswerWithIceRestartLatch();
  SessionDescription* desc(
      session_desc_factory_.CreateAnswer(BaseSession::remote_description(),
                                         options,
                                         BaseSession::local_description()));
  // RFC 3264
  // If the answer is different from the offer in any way (different IP
  // addresses, ports, etc.), the origin line MUST be different in the answer.
  // In that case, the version number in the "o=" line of the answer is
  // unrelated to the version number in the o line of the offer.
  // Get a new version number by increasing the |session_version_answer_|.
  // The |session_version_| is a uint64, the wrap around should not happen.
  ASSERT(session_version_ + 1 > session_version_);
  JsepSessionDescription* answer(new JsepSessionDescription(
      JsepSessionDescription::kAnswer));
  if (!answer->Initialize(desc, id(),
                          talk_base::ToString(session_version_++))) {
    delete answer;
    return NULL;
  }
  if (local_description() && !options.transport_options.ice_restart) {
    // Include all local ice candidates in the SessionDescription unless
    // the remote peer has requested an ice restart.
    CopyCandidatesFromSessionDescription(local_description(), answer);
  }
  return answer;
}

bool WebRtcSession::SetLocalDescription(SessionDescriptionInterface* desc,
                                        std::string* err_desc) {
  if (error() != cricket::BaseSession::ERROR_NONE) {
    delete desc;
    return BadLocalSdp(SessionErrorMsg(error()), err_desc);
  }

  if (!desc || !desc->description()) {
    delete desc;
    return BadLocalSdp(kInvalidSdp, err_desc);
  }
  Action action = GetAction(desc->type());
  if (!ExpectSetLocalDescription(action)) {
    std::string type = desc->type();
    delete desc;
    return BadLocalSdp(BadStateErrMsg(type, state()), err_desc);
  }

  if (session_desc_factory_.secure() == cricket::SEC_REQUIRED &&
      !VerifyCrypto(desc->description())) {
    delete desc;
    return BadLocalSdp(kSdpWithoutCrypto, err_desc);
  }

  if (action == kAnswer && !VerifyMediaDescriptions(
          desc->description(), remote_description()->description())) {
    return BadLocalSdp(kMlineMismatch, err_desc);
  }

  // Update the initiator flag if this session is the initiator.
  if (state() == STATE_INIT && action == kOffer) {
    set_initiator(true);
  }

  // Update the MediaContentDescription crypto settings as per the policy set.
  UpdateSessionDescriptionSecurePolicy(desc->description());

  set_local_description(desc->description()->Copy());
  local_desc_.reset(desc);

  // Transport and Media channels will be created only when offer is set.
  if (action == kOffer && !CreateChannels(desc->description())) {
    // TODO(mallinath) - Handle CreateChannel failure, as new local description
    // is applied. Restore back to old description.
    return BadLocalSdp(kCreateChannelFailed, err_desc);
  }

  // Remove channel and transport proxies, if MediaContentDescription is
  // rejected.
  RemoveUnusedChannelsAndTransports(desc->description());

  if (!UpdateSessionState(action, cricket::CS_LOCAL,
                          desc->description(), err_desc)) {
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
  if (error() != cricket::BaseSession::ERROR_NONE) {
    delete desc;
    return BadRemoteSdp(SessionErrorMsg(error()), err_desc);
  }

  if (!desc || !desc->description()) {
    delete desc;
    return BadRemoteSdp(kInvalidSdp, err_desc);
  }
  Action action = GetAction(desc->type());
  if (!ExpectSetRemoteDescription(action)) {
    std::string type = desc->type();
    delete desc;
    return BadRemoteSdp(BadStateErrMsg(type, state()), err_desc);
  }

  if (action == kAnswer && !VerifyMediaDescriptions(
          desc->description(), local_description()->description())) {
    return BadRemoteSdp(kMlineMismatch, err_desc);
  }

  if (session_desc_factory_.secure() == cricket::SEC_REQUIRED &&
      !VerifyCrypto(desc->description())) {
    delete desc;
    return BadRemoteSdp(kSdpWithoutCrypto, err_desc);
  }

  // Transport and Media channels will be created only when offer is set.
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
    delete desc;
    return BadRemoteSdp(kInvalidCandidates, err_desc);
  }

  // Copy all saved candidates.
  CopySavedCandidates(desc);
  // We retain all received candidates.
  CopyCandidatesFromSessionDescription(remote_desc_.get(), desc);
  // Check if this new SessionDescription contains new ice ufrag and password
  // that indicates the remote peer requests ice restart.
  ice_restart_latch_->CheckForRemoteIceRestart(remote_desc_.get(),
                                               desc);
  remote_desc_.reset(desc);
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

void WebRtcSession::SetAudioPlayout(uint32 ssrc, bool enable) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_) {
    LOG(LS_ERROR) << "SetAudioPlayout: No audio channel exists.";
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
                                 const cricket::AudioOptions& options) {
  ASSERT(signaling_thread()->IsCurrent());
  if (!voice_channel_) {
    LOG(LS_ERROR) << "SetAudioSend: No audio channel exists.";
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

bool WebRtcSession::SetAudioRenderer(uint32 ssrc,
                                     cricket::AudioRenderer* renderer) {
  if (!voice_channel_) {
    LOG(LS_ERROR) << "SetAudioRenderer: No audio channel exists.";
    return false;
  }

  if (!voice_channel_->SetRenderer(ssrc, renderer)) {
    // SetRenderer() can fail if the ssrc is not mapping to the playout channel.
    LOG(LS_ERROR) << "SetAudioRenderer: ssrc is incorrect: " << ssrc;
    return false;
  }

  return true;
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
  return channel;
}

cricket::DataChannelType WebRtcSession::data_channel_type() const {
  return data_channel_type_;
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
      *sdp_mline_index = index;
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

bool WebRtcSession::CreateChannels(const SessionDescription* desc) {
  // Disabling the BUNDLE flag in PortAllocator if offer disabled it.
  if (state() == STATE_INIT && !desc->HasGroup(cricket::GROUP_TYPE_BUNDLE)) {
    port_allocator()->set_flags(port_allocator()->flags() &
                                ~cricket::PORTALLOCATOR_ENABLE_BUNDLE);
  }

  // Creating the media channels and transport proxies.
  const cricket::ContentInfo* voice = cricket::GetFirstAudioContent(desc);
  if (voice && !voice->rejected && !voice_channel_) {
    if (!CreateVoiceChannel(desc)) {
      LOG(LS_ERROR) << "Failed to create voice channel.";
      return false;
    }
  }

  const cricket::ContentInfo* video = cricket::GetFirstVideoContent(desc);
  if (video && !video->rejected && !video_channel_) {
    if (!CreateVideoChannel(desc)) {
      LOG(LS_ERROR) << "Failed to create video channel.";
      return false;
    }
  }

  const cricket::ContentInfo* data = cricket::GetFirstDataContent(desc);
  if (data_channel_type_ != cricket::DCT_NONE &&
      data && !data->rejected && !data_channel_.get()) {
    if (!CreateDataChannel(desc)) {
      LOG(LS_ERROR) << "Failed to create data channel.";
      return false;
    }
  }

  return true;
}

bool WebRtcSession::CreateVoiceChannel(const SessionDescription* desc) {
  const cricket::ContentInfo* voice = cricket::GetFirstAudioContent(desc);
  voice_channel_.reset(channel_manager_->CreateVoiceChannel(
      this, voice->name, true));
  return voice_channel_ ? true : false;
}

bool WebRtcSession::CreateVideoChannel(const SessionDescription* desc) {
  const cricket::ContentInfo* video = cricket::GetFirstVideoContent(desc);
  video_channel_.reset(channel_manager_->CreateVideoChannel(
      this, video->name, true, voice_channel_.get()));
  return video_channel_ ? true : false;
}

bool WebRtcSession::CreateDataChannel(const SessionDescription* desc) {
  const cricket::ContentInfo* data = cricket::GetFirstDataContent(desc);
  bool rtcp = (data_channel_type_ == cricket::DCT_RTP);
  data_channel_.reset(channel_manager_->CreateDataChannel(
      this, data->name, rtcp, data_channel_type_));
  if (!data_channel_.get()) {
    return false;
  }
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

void WebRtcSession::UpdateSessionDescriptionSecurePolicy(
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
        mdesc->set_crypto_required(
            session_desc_factory_.secure() == cricket::SEC_REQUIRED);
      }
    }
  }
}

}  // namespace webrtc
