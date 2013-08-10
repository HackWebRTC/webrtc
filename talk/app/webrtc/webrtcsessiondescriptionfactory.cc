/*
 * libjingle
 * Copyright 2013, Google Inc.
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

#include "talk/app/webrtc/webrtcsessiondescriptionfactory.h"

#include "talk/app/webrtc/jsep.h"
#include "talk/app/webrtc/jsepsessiondescription.h"
#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/app/webrtc/mediastreamsignaling.h"
#include "talk/app/webrtc/webrtcsession.h"

namespace webrtc {

namespace {

static const char kFailedDueToIdentityFailed[] =
    " failed because DTLS identity request failed";

// Arbitrary constant used as common name for the identity.
// Chosen to make the certificates more readable.
static const char kWebRTCIdentityName[] = "WebRTC";

static const uint64 kInitSessionVersion = 2;

typedef cricket::MediaSessionOptions::Stream Stream;
typedef cricket::MediaSessionOptions::Streams Streams;

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

enum {
  MSG_CREATE_SESSIONDESCRIPTION_SUCCESS,
  MSG_CREATE_SESSIONDESCRIPTION_FAILED,
  MSG_GENERATE_IDENTITY,
};

struct CreateSessionDescriptionMsg : public talk_base::MessageData {
  explicit CreateSessionDescriptionMsg(
      webrtc::CreateSessionDescriptionObserver* observer)
      : observer(observer) {
  }

  talk_base::scoped_refptr<webrtc::CreateSessionDescriptionObserver> observer;
  std::string error;
  talk_base::scoped_ptr<webrtc::SessionDescriptionInterface> description;
};

}  // namespace

// static
void WebRtcSessionDescriptionFactory::CopyCandidatesFromSessionDescription(
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

WebRtcSessionDescriptionFactory::WebRtcSessionDescriptionFactory(
    talk_base::Thread* signaling_thread,
    cricket::ChannelManager* channel_manager,
    MediaStreamSignaling* mediastream_signaling,
    DTLSIdentityServiceInterface* dtls_identity_service,
    WebRtcSession* session,
    const std::string& session_id,
    cricket::DataChannelType dct,
    const MediaConstraintsInterface* constraints)
    : signaling_thread_(signaling_thread),
      mediastream_signaling_(mediastream_signaling),
      session_desc_factory_(channel_manager, &transport_desc_factory_),
      // RFC 4566 suggested a Network Time Protocol (NTP) format timestamp
      // as the session id and session version. To simplify, it should be fine
      // to just use a random number as session id and start version from
      // |kInitSessionVersion|.
      session_version_(kInitSessionVersion),
      identity_service_(dtls_identity_service),
      session_(session),
      session_id_(session_id),
      data_channel_type_(dct),
      identity_request_state_(IDENTITY_NOT_NEEDED) {
  transport_desc_factory_.set_protocol(cricket::ICEPROTO_HYBRID);
  session_desc_factory_.set_add_legacy_streams(false);
  // By default SRTP-SDES is enabled in WebRtc.
  set_secure(cricket::SEC_REQUIRED);

  // Enable DTLS-SRTP if the constraint is set.
  bool dtls_enabled = false;
  if (!FindConstraint(
           constraints, MediaConstraintsInterface::kEnableDtlsSrtp,
           &dtls_enabled, NULL) ||
      !dtls_enabled) {
    return;
  }
  // DTLS is enabled.
  if (identity_service_.get()) {
    identity_request_observer_ =
      new talk_base::RefCountedObject<WebRtcIdentityRequestObserver>();

    identity_request_observer_->SignalRequestFailed.connect(
        this, &WebRtcSessionDescriptionFactory::OnIdentityRequestFailed);
    identity_request_observer_->SignalIdentityReady.connect(
        this, &WebRtcSessionDescriptionFactory::OnIdentityReady);

    if (identity_service_->RequestIdentity(kWebRTCIdentityName,
                                           kWebRTCIdentityName,
                                           identity_request_observer_)) {
      LOG(LS_VERBOSE) << "DTLS-SRTP enabled; sent DTLS identity request.";
      identity_request_state_ = IDENTITY_WAITING;
    } else {
      LOG(LS_ERROR) << "Failed to send DTLS identity request.";
      identity_request_state_ = IDENTITY_FAILED;
    }
  } else {
    identity_request_state_ = IDENTITY_WAITING;
    // Do not generate the identity in the constructor since the caller has
    // not got a chance to connect to SignalIdentityReady.
    signaling_thread_->Post(this, MSG_GENERATE_IDENTITY, NULL);
  }
}

WebRtcSessionDescriptionFactory::~WebRtcSessionDescriptionFactory() {
  transport_desc_factory_.set_identity(NULL);
}

void WebRtcSessionDescriptionFactory::CreateOffer(
    CreateSessionDescriptionObserver* observer,
    const MediaConstraintsInterface* constraints) {
  cricket::MediaSessionOptions options;
  std::string error = "CreateOffer";
  if (identity_request_state_ == IDENTITY_FAILED) {
    error += kFailedDueToIdentityFailed;
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }

  if (!mediastream_signaling_->GetOptionsForOffer(constraints, &options)) {
    error += " called with invalid constraints.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }

  if (!ValidStreams(options.streams)) {
    error += " called with invalid media streams.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }

  if (data_channel_type_ == cricket::DCT_SCTP) {
    options.data_channel_type = cricket::DCT_SCTP;
  }

  CreateSessionDescriptionRequest request(
      CreateSessionDescriptionRequest::kOffer, observer, options);
  if (identity_request_state_ == IDENTITY_WAITING) {
    create_session_description_requests_.push(request);
  } else {
    ASSERT(identity_request_state_ == IDENTITY_SUCCEEDED ||
           identity_request_state_ == IDENTITY_NOT_NEEDED);
    InternalCreateOffer(request);
  }
}

void WebRtcSessionDescriptionFactory::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const MediaConstraintsInterface* constraints) {
  std::string error = "CreateAnswer";
  if (identity_request_state_ == IDENTITY_FAILED) {
    error += kFailedDueToIdentityFailed;
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }
  if (!session_->remote_description()) {
    error += " can't be called before SetRemoteDescription.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }
  if (session_->remote_description()->type() !=
      JsepSessionDescription::kOffer) {
    error += " failed because remote_description is not an offer.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }

  cricket::MediaSessionOptions options;
  if (!mediastream_signaling_->GetOptionsForAnswer(constraints, &options)) {
    error += " called with invalid constraints.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }
  if (!ValidStreams(options.streams)) {
    error += " called with invalid media streams.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }
  if (data_channel_type_ == cricket::DCT_SCTP) {
    options.data_channel_type = cricket::DCT_SCTP;
  }

  CreateSessionDescriptionRequest request(
      CreateSessionDescriptionRequest::kAnswer, observer, options);
  if (identity_request_state_ == IDENTITY_WAITING) {
    create_session_description_requests_.push(request);
  } else {
    ASSERT(identity_request_state_ == IDENTITY_SUCCEEDED ||
           identity_request_state_ == IDENTITY_NOT_NEEDED);
    InternalCreateAnswer(request);
  }
}

void WebRtcSessionDescriptionFactory::set_secure(
    cricket::SecureMediaPolicy secure_policy) {
  session_desc_factory_.set_secure(secure_policy);
}

cricket::SecureMediaPolicy WebRtcSessionDescriptionFactory::secure() const {
  return session_desc_factory_.secure();
}

bool WebRtcSessionDescriptionFactory::waiting_for_identity() const {
  return identity_request_state_ == IDENTITY_WAITING;
}

void WebRtcSessionDescriptionFactory::OnMessage(talk_base::Message* msg) {
  switch (msg->message_id) {
    case MSG_CREATE_SESSIONDESCRIPTION_SUCCESS: {
      CreateSessionDescriptionMsg* param =
          static_cast<CreateSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnSuccess(param->description.release());
      delete param;
      break;
    }
    case MSG_CREATE_SESSIONDESCRIPTION_FAILED: {
      CreateSessionDescriptionMsg* param =
          static_cast<CreateSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnFailure(param->error);
      delete param;
      break;
    }
    case MSG_GENERATE_IDENTITY: {
      LOG(LS_INFO) << "Generating identity.";
      SetIdentity(talk_base::SSLIdentity::Generate(kWebRTCIdentityName));
      break;
    }
    default:
      ASSERT(false);
      break;
  }
}

void WebRtcSessionDescriptionFactory::InternalCreateOffer(
    CreateSessionDescriptionRequest request) {
  cricket::SessionDescription* desc(
      session_desc_factory_.CreateOffer(
          request.options,
          static_cast<cricket::BaseSession*>(session_)->local_description()));
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
  if (!offer->Initialize(desc, session_id_,
                         talk_base::ToString(session_version_++))) {
    delete offer;
    PostCreateSessionDescriptionFailed(request.observer, "CreateOffer failed.");
    return;
  }
  if (session_->local_description() &&
      !request.options.transport_options.ice_restart) {
    // Include all local ice candidates in the SessionDescription unless
    // the an ice restart has been requested.
    CopyCandidatesFromSessionDescription(session_->local_description(), offer);
  }
  PostCreateSessionDescriptionSucceeded(request.observer, offer);
}

void WebRtcSessionDescriptionFactory::InternalCreateAnswer(
    CreateSessionDescriptionRequest request) {
  // According to http://tools.ietf.org/html/rfc5245#section-9.2.1.1
  // an answer should also contain new ice ufrag and password if an offer has
  // been received with new ufrag and password.
  request.options.transport_options.ice_restart = session_->IceRestartPending();

  cricket::SessionDescription* desc(session_desc_factory_.CreateAnswer(
      static_cast<cricket::BaseSession*>(session_)->remote_description(),
      request.options,
      static_cast<cricket::BaseSession*>(session_)->local_description()));
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
  if (!answer->Initialize(desc, session_id_,
                          talk_base::ToString(session_version_++))) {
    delete answer;
    PostCreateSessionDescriptionFailed(request.observer,
                                       "CreateAnswer failed.");
    return;
  }
  if (session_->local_description() &&
      !request.options.transport_options.ice_restart) {
    // Include all local ice candidates in the SessionDescription unless
    // the remote peer has requested an ice restart.
    CopyCandidatesFromSessionDescription(session_->local_description(), answer);
  }
  session_->ResetIceRestartLatch();
  PostCreateSessionDescriptionSucceeded(request.observer, answer);
}

void WebRtcSessionDescriptionFactory::PostCreateSessionDescriptionFailed(
    CreateSessionDescriptionObserver* observer, const std::string& error) {
  CreateSessionDescriptionMsg* msg = new CreateSessionDescriptionMsg(observer);
  msg->error = error;
  signaling_thread_->Post(this, MSG_CREATE_SESSIONDESCRIPTION_FAILED, msg);
}

void WebRtcSessionDescriptionFactory::PostCreateSessionDescriptionSucceeded(
    CreateSessionDescriptionObserver* observer,
    SessionDescriptionInterface* description) {
  CreateSessionDescriptionMsg* msg = new CreateSessionDescriptionMsg(observer);
  msg->description.reset(description);
  signaling_thread_->Post(this, MSG_CREATE_SESSIONDESCRIPTION_SUCCESS, msg);
}

void WebRtcSessionDescriptionFactory::OnIdentityRequestFailed(int error) {
  ASSERT(signaling_thread_->IsCurrent());

  LOG(LS_ERROR) << "Async identity request failed: error = " << error;
  identity_request_state_ = IDENTITY_FAILED;

  std::string msg = kFailedDueToIdentityFailed;
  while (!create_session_description_requests_.empty()) {
    const CreateSessionDescriptionRequest& request =
        create_session_description_requests_.front();
    PostCreateSessionDescriptionFailed(
        request.observer,
        ((request.type == CreateSessionDescriptionRequest::kOffer) ?
            "CreateOffer" : "CreateAnswer") + msg);
    create_session_description_requests_.pop();
  }
}

void WebRtcSessionDescriptionFactory::OnIdentityReady(
    const std::string& der_cert,
    const std::string& der_private_key) {
  ASSERT(signaling_thread_->IsCurrent());
  LOG(LS_VERBOSE) << "Identity is successfully generated.";

  std::string pem_cert = talk_base::SSLIdentity::DerToPem(
      talk_base::kPemTypeCertificate,
      reinterpret_cast<const unsigned char*>(der_cert.data()),
      der_cert.length());
  std::string pem_key = talk_base::SSLIdentity::DerToPem(
      talk_base::kPemTypeRsaPrivateKey,
      reinterpret_cast<const unsigned char*>(der_private_key.data()),
      der_private_key.length());

  talk_base::SSLIdentity* identity =
      talk_base::SSLIdentity::FromPEMStrings(pem_key, pem_cert);
  SetIdentity(identity);
}

void WebRtcSessionDescriptionFactory::SetIdentity(
    talk_base::SSLIdentity* identity) {
  identity_request_state_ = IDENTITY_SUCCEEDED;
  SignalIdentityReady(identity);

  transport_desc_factory_.set_identity(identity);
  transport_desc_factory_.set_digest_algorithm(talk_base::DIGEST_SHA_256);
  transport_desc_factory_.set_secure(cricket::SEC_ENABLED);

  while (!create_session_description_requests_.empty()) {
    if (create_session_description_requests_.front().type ==
        CreateSessionDescriptionRequest::kOffer) {
      InternalCreateOffer(create_session_description_requests_.front());
    } else {
      InternalCreateAnswer(create_session_description_requests_.front());
    }
    create_session_description_requests_.pop();
  }
}

}  // namespace webrtc
