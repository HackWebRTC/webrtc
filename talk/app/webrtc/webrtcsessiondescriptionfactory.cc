/*
 * libjingle
 * Copyright 2013 Google Inc.
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

#include "talk/app/webrtc/dtlsidentitystore.h"
#include "talk/app/webrtc/jsep.h"
#include "talk/app/webrtc/jsepsessiondescription.h"
#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/app/webrtc/webrtcsession.h"
#include "webrtc/base/sslidentity.h"

using cricket::MediaSessionOptions;

namespace webrtc {
namespace {
static const char kFailedDueToIdentityFailed[] =
    " failed because DTLS identity request failed";
static const char kFailedDueToSessionShutdown[] =
    " failed because the session was shut down";

static const uint64_t kInitSessionVersion = 2;

static bool CompareStream(const MediaSessionOptions::Stream& stream1,
                          const MediaSessionOptions::Stream& stream2) {
  return stream1.id < stream2.id;
}

static bool SameId(const MediaSessionOptions::Stream& stream1,
                   const MediaSessionOptions::Stream& stream2) {
  return stream1.id == stream2.id;
}

// Checks if each Stream within the |streams| has unique id.
static bool ValidStreams(const MediaSessionOptions::Streams& streams) {
  MediaSessionOptions::Streams sorted_streams = streams;
  std::sort(sorted_streams.begin(), sorted_streams.end(), CompareStream);
  MediaSessionOptions::Streams::iterator it =
      std::adjacent_find(sorted_streams.begin(), sorted_streams.end(),
                         SameId);
  return it == sorted_streams.end();
}

enum {
  MSG_CREATE_SESSIONDESCRIPTION_SUCCESS,
  MSG_CREATE_SESSIONDESCRIPTION_FAILED,
  MSG_USE_CONSTRUCTOR_CERTIFICATE
};

struct CreateSessionDescriptionMsg : public rtc::MessageData {
  explicit CreateSessionDescriptionMsg(
      webrtc::CreateSessionDescriptionObserver* observer)
      : observer(observer) {
  }

  rtc::scoped_refptr<webrtc::CreateSessionDescriptionObserver> observer;
  std::string error;
  rtc::scoped_ptr<webrtc::SessionDescriptionInterface> description;
};
}  // namespace

void WebRtcIdentityRequestObserver::OnFailure(int error) {
  SignalRequestFailed(error);
}

void WebRtcIdentityRequestObserver::OnSuccess(
    const std::string& der_cert, const std::string& der_private_key) {
  std::string pem_cert = rtc::SSLIdentity::DerToPem(
      rtc::kPemTypeCertificate,
      reinterpret_cast<const unsigned char*>(der_cert.data()),
      der_cert.length());
  std::string pem_key = rtc::SSLIdentity::DerToPem(
      rtc::kPemTypeRsaPrivateKey,
      reinterpret_cast<const unsigned char*>(der_private_key.data()),
      der_private_key.length());
  rtc::scoped_ptr<rtc::SSLIdentity> identity(
      rtc::SSLIdentity::FromPEMStrings(pem_key, pem_cert));
  SignalCertificateReady(rtc::RTCCertificate::Create(identity.Pass()));
}

void WebRtcIdentityRequestObserver::OnSuccess(
    rtc::scoped_ptr<rtc::SSLIdentity> identity) {
  SignalCertificateReady(rtc::RTCCertificate::Create(identity.Pass()));
}

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

// Private constructor called by other constructors.
WebRtcSessionDescriptionFactory::WebRtcSessionDescriptionFactory(
    rtc::Thread* signaling_thread,
    cricket::ChannelManager* channel_manager,
    rtc::scoped_ptr<DtlsIdentityStoreInterface> dtls_identity_store,
    const rtc::scoped_refptr<WebRtcIdentityRequestObserver>&
        identity_request_observer,
    WebRtcSession* session,
    const std::string& session_id,
    bool dtls_enabled)
    : signaling_thread_(signaling_thread),
      session_desc_factory_(channel_manager, &transport_desc_factory_),
      // RFC 4566 suggested a Network Time Protocol (NTP) format timestamp
      // as the session id and session version. To simplify, it should be fine
      // to just use a random number as session id and start version from
      // |kInitSessionVersion|.
      session_version_(kInitSessionVersion),
      dtls_identity_store_(dtls_identity_store.Pass()),
      identity_request_observer_(identity_request_observer),
      session_(session),
      session_id_(session_id),
      certificate_request_state_(CERTIFICATE_NOT_NEEDED) {
  session_desc_factory_.set_add_legacy_streams(false);
  // SRTP-SDES is disabled if DTLS is on.
  SetSdesPolicy(dtls_enabled ? cricket::SEC_DISABLED : cricket::SEC_REQUIRED);
}

WebRtcSessionDescriptionFactory::WebRtcSessionDescriptionFactory(
    rtc::Thread* signaling_thread,
    cricket::ChannelManager* channel_manager,
    WebRtcSession* session,
    const std::string& session_id)
    : WebRtcSessionDescriptionFactory(signaling_thread,
                                      channel_manager,
                                      nullptr,
                                      nullptr,
                                      session,
                                      session_id,
                                      false) {
  LOG(LS_VERBOSE) << "DTLS-SRTP disabled.";
}

WebRtcSessionDescriptionFactory::WebRtcSessionDescriptionFactory(
    rtc::Thread* signaling_thread,
    cricket::ChannelManager* channel_manager,
    rtc::scoped_ptr<DtlsIdentityStoreInterface> dtls_identity_store,
    WebRtcSession* session,
    const std::string& session_id)
    : WebRtcSessionDescriptionFactory(
          signaling_thread,
          channel_manager,
          dtls_identity_store.Pass(),
          new rtc::RefCountedObject<WebRtcIdentityRequestObserver>(),
          session,
          session_id,
          true) {
  RTC_DCHECK(dtls_identity_store_);

  certificate_request_state_ = CERTIFICATE_WAITING;

  identity_request_observer_->SignalRequestFailed.connect(
      this, &WebRtcSessionDescriptionFactory::OnIdentityRequestFailed);
  identity_request_observer_->SignalCertificateReady.connect(
      this, &WebRtcSessionDescriptionFactory::SetCertificate);

  rtc::KeyType key_type = rtc::KT_DEFAULT;
  LOG(LS_VERBOSE) << "DTLS-SRTP enabled; sending DTLS identity request (key "
                  << "type: " << key_type << ").";

  // Request identity. This happens asynchronously, so the caller will have a
  // chance to connect to SignalIdentityReady.
  dtls_identity_store_->RequestIdentity(key_type, identity_request_observer_);
}

WebRtcSessionDescriptionFactory::WebRtcSessionDescriptionFactory(
    rtc::Thread* signaling_thread,
    cricket::ChannelManager* channel_manager,
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate,
    WebRtcSession* session,
    const std::string& session_id)
    : WebRtcSessionDescriptionFactory(signaling_thread,
                                      channel_manager,
                                      nullptr,
                                      nullptr,
                                      session,
                                      session_id,
                                      true) {
  RTC_DCHECK(certificate);

  certificate_request_state_ = CERTIFICATE_WAITING;

  LOG(LS_VERBOSE) << "DTLS-SRTP enabled; has certificate parameter.";
  // We already have a certificate but we wait to do SetIdentity; if we do
  // it in the constructor then the caller has not had a chance to connect to
  // SignalIdentityReady.
  signaling_thread_->Post(
      this, MSG_USE_CONSTRUCTOR_CERTIFICATE,
      new rtc::ScopedRefMessageData<rtc::RTCCertificate>(certificate));
}

WebRtcSessionDescriptionFactory::~WebRtcSessionDescriptionFactory() {
  ASSERT(signaling_thread_->IsCurrent());

  // Fail any requests that were asked for before identity generation completed.
  FailPendingRequests(kFailedDueToSessionShutdown);

  // Process all pending notifications in the message queue.  If we don't do
  // this, requests will linger and not know they succeeded or failed.
  rtc::MessageList list;
  signaling_thread_->Clear(this, rtc::MQID_ANY, &list);
  for (auto& msg : list) {
    if (msg.message_id != MSG_USE_CONSTRUCTOR_CERTIFICATE) {
      OnMessage(&msg);
    } else {
      // Skip MSG_USE_CONSTRUCTOR_CERTIFICATE because we don't want to trigger
      // SetIdentity-related callbacks in the destructor. This can be a problem
      // when WebRtcSession listens to the callback but it was the WebRtcSession
      // destructor that caused WebRtcSessionDescriptionFactory's destruction.
      // The callback is then ignored, leaking memory allocated by OnMessage for
      // MSG_USE_CONSTRUCTOR_CERTIFICATE.
      delete msg.pdata;
    }
  }
}

void WebRtcSessionDescriptionFactory::CreateOffer(
    CreateSessionDescriptionObserver* observer,
    const PeerConnectionInterface::RTCOfferAnswerOptions& options,
    const cricket::MediaSessionOptions& session_options) {
  std::string error = "CreateOffer";
  if (certificate_request_state_ == CERTIFICATE_FAILED) {
    error += kFailedDueToIdentityFailed;
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }

  if (!ValidStreams(session_options.streams)) {
    error += " called with invalid media streams.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }

  CreateSessionDescriptionRequest request(
      CreateSessionDescriptionRequest::kOffer, observer, session_options);
  if (certificate_request_state_ == CERTIFICATE_WAITING) {
    create_session_description_requests_.push(request);
  } else {
    ASSERT(certificate_request_state_ == CERTIFICATE_SUCCEEDED ||
           certificate_request_state_ == CERTIFICATE_NOT_NEEDED);
    InternalCreateOffer(request);
  }
}

void WebRtcSessionDescriptionFactory::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const MediaConstraintsInterface* constraints,
    const cricket::MediaSessionOptions& session_options) {
  std::string error = "CreateAnswer";
  if (certificate_request_state_ == CERTIFICATE_FAILED) {
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

  if (!ValidStreams(session_options.streams)) {
    error += " called with invalid media streams.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailed(observer, error);
    return;
  }

  CreateSessionDescriptionRequest request(
      CreateSessionDescriptionRequest::kAnswer, observer, session_options);
  if (certificate_request_state_ == CERTIFICATE_WAITING) {
    create_session_description_requests_.push(request);
  } else {
    ASSERT(certificate_request_state_ == CERTIFICATE_SUCCEEDED ||
           certificate_request_state_ == CERTIFICATE_NOT_NEEDED);
    InternalCreateAnswer(request);
  }
}

void WebRtcSessionDescriptionFactory::SetSdesPolicy(
    cricket::SecurePolicy secure_policy) {
  session_desc_factory_.set_secure(secure_policy);
}

cricket::SecurePolicy WebRtcSessionDescriptionFactory::SdesPolicy() const {
  return session_desc_factory_.secure();
}

void WebRtcSessionDescriptionFactory::OnMessage(rtc::Message* msg) {
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
    case MSG_USE_CONSTRUCTOR_CERTIFICATE: {
      rtc::ScopedRefMessageData<rtc::RTCCertificate>* param =
          static_cast<rtc::ScopedRefMessageData<rtc::RTCCertificate>*>(
              msg->pdata);
      LOG(LS_INFO) << "Using certificate supplied to the constructor.";
      SetCertificate(param->data());
      delete param;
      break;
    }
    default:
      ASSERT(false);
      break;
  }
}

void WebRtcSessionDescriptionFactory::InternalCreateOffer(
    CreateSessionDescriptionRequest request) {
  cricket::SessionDescription* desc(session_desc_factory_.CreateOffer(
      request.options, session_->local_description()
                           ? session_->local_description()->description()
                           : nullptr));
  // RFC 3264
  // When issuing an offer that modifies the session,
  // the "o=" line of the new SDP MUST be identical to that in the
  // previous SDP, except that the version in the origin field MUST
  // increment by one from the previous SDP.

  // Just increase the version number by one each time when a new offer
  // is created regardless if it's identical to the previous one or not.
  // The |session_version_| is a uint64_t, the wrap around should not happen.
  ASSERT(session_version_ + 1 > session_version_);
  JsepSessionDescription* offer(new JsepSessionDescription(
      JsepSessionDescription::kOffer));
  if (!offer->Initialize(desc, session_id_,
                         rtc::ToString(session_version_++))) {
    delete offer;
    PostCreateSessionDescriptionFailed(request.observer,
                                       "Failed to initialize the offer.");
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
  // We should pass current ssl role to the transport description factory, if
  // there is already an existing ongoing session.
  rtc::SSLRole ssl_role;
  if (session_->GetSslRole(&ssl_role)) {
    request.options.transport_options.prefer_passive_role =
        (rtc::SSL_SERVER == ssl_role);
  }

  cricket::SessionDescription* desc(session_desc_factory_.CreateAnswer(
      session_->remote_description()
          ? session_->remote_description()->description()
          : nullptr,
      request.options, session_->local_description()
                           ? session_->local_description()->description()
                           : nullptr));
  // RFC 3264
  // If the answer is different from the offer in any way (different IP
  // addresses, ports, etc.), the origin line MUST be different in the answer.
  // In that case, the version number in the "o=" line of the answer is
  // unrelated to the version number in the o line of the offer.
  // Get a new version number by increasing the |session_version_answer_|.
  // The |session_version_| is a uint64_t, the wrap around should not happen.
  ASSERT(session_version_ + 1 > session_version_);
  JsepSessionDescription* answer(new JsepSessionDescription(
      JsepSessionDescription::kAnswer));
  if (!answer->Initialize(desc, session_id_,
                          rtc::ToString(session_version_++))) {
    delete answer;
    PostCreateSessionDescriptionFailed(request.observer,
                                       "Failed to initialize the answer.");
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

void WebRtcSessionDescriptionFactory::FailPendingRequests(
    const std::string& reason) {
  ASSERT(signaling_thread_->IsCurrent());
  while (!create_session_description_requests_.empty()) {
    const CreateSessionDescriptionRequest& request =
        create_session_description_requests_.front();
    PostCreateSessionDescriptionFailed(request.observer,
        ((request.type == CreateSessionDescriptionRequest::kOffer) ?
            "CreateOffer" : "CreateAnswer") + reason);
    create_session_description_requests_.pop();
  }
}

void WebRtcSessionDescriptionFactory::PostCreateSessionDescriptionFailed(
    CreateSessionDescriptionObserver* observer, const std::string& error) {
  CreateSessionDescriptionMsg* msg = new CreateSessionDescriptionMsg(observer);
  msg->error = error;
  signaling_thread_->Post(this, MSG_CREATE_SESSIONDESCRIPTION_FAILED, msg);
  LOG(LS_ERROR) << "Create SDP failed: " << error;
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
  certificate_request_state_ = CERTIFICATE_FAILED;

  FailPendingRequests(kFailedDueToIdentityFailed);
}

void WebRtcSessionDescriptionFactory::SetCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  RTC_DCHECK(certificate);
  LOG(LS_VERBOSE) << "Setting new certificate";

  certificate_request_state_ = CERTIFICATE_SUCCEEDED;
  SignalCertificateReady(certificate);

  transport_desc_factory_.set_certificate(certificate);
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
