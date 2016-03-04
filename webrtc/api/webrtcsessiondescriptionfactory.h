/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_WEBRTCSESSIONDESCRIPTIONFACTORY_H_
#define WEBRTC_API_WEBRTCSESSIONDESCRIPTIONFACTORY_H_

#include "webrtc/api/dtlsidentitystore.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/rtccertificate.h"
#include "webrtc/p2p/base/transportdescriptionfactory.h"
#include "webrtc/pc/mediasession.h"

namespace cricket {
class ChannelManager;
class TransportDescriptionFactory;
}  // namespace cricket

namespace webrtc {
class CreateSessionDescriptionObserver;
class MediaConstraintsInterface;
class SessionDescriptionInterface;
class WebRtcSession;

// DTLS identity request callback class.
class WebRtcIdentityRequestObserver : public DtlsIdentityRequestObserver,
                                      public sigslot::has_slots<> {
 public:
  // DtlsIdentityRequestObserver overrides.
  void OnFailure(int error) override;
  void OnSuccess(const std::string& der_cert,
                 const std::string& der_private_key) override;
  void OnSuccess(rtc::scoped_ptr<rtc::SSLIdentity> identity) override;

  sigslot::signal1<int> SignalRequestFailed;
  sigslot::signal1<const rtc::scoped_refptr<rtc::RTCCertificate>&>
      SignalCertificateReady;
};

struct CreateSessionDescriptionRequest {
  enum Type {
    kOffer,
    kAnswer,
  };

  CreateSessionDescriptionRequest(
      Type type,
      CreateSessionDescriptionObserver* observer,
      const cricket::MediaSessionOptions& options)
      : type(type),
        observer(observer),
        options(options) {}

  Type type;
  rtc::scoped_refptr<CreateSessionDescriptionObserver> observer;
  cricket::MediaSessionOptions options;
};

// This class is used to create offer/answer session description with regards to
// the async DTLS identity generation for WebRtcSession.
// It queues the create offer/answer request until the DTLS identity
// request has completed, i.e. when OnIdentityRequestFailed or OnIdentityReady
// is called.
class WebRtcSessionDescriptionFactory : public rtc::MessageHandler,
                                        public sigslot::has_slots<> {
 public:
  // Construct with DTLS disabled.
  WebRtcSessionDescriptionFactory(rtc::Thread* signaling_thread,
                                  cricket::ChannelManager* channel_manager,
                                  WebRtcSession* session,
                                  const std::string& session_id);

  // Construct with DTLS enabled using the specified |dtls_identity_store| to
  // generate a certificate.
  WebRtcSessionDescriptionFactory(
      rtc::Thread* signaling_thread,
      cricket::ChannelManager* channel_manager,
      rtc::scoped_ptr<DtlsIdentityStoreInterface> dtls_identity_store,
      WebRtcSession* session,
      const std::string& session_id);

  // Construct with DTLS enabled using the specified (already generated)
  // |certificate|.
  WebRtcSessionDescriptionFactory(
      rtc::Thread* signaling_thread,
      cricket::ChannelManager* channel_manager,
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate,
      WebRtcSession* session,
      const std::string& session_id);
  virtual ~WebRtcSessionDescriptionFactory();

  static void CopyCandidatesFromSessionDescription(
      const SessionDescriptionInterface* source_desc,
      const std::string& content_name,
      SessionDescriptionInterface* dest_desc);

  void CreateOffer(
      CreateSessionDescriptionObserver* observer,
      const PeerConnectionInterface::RTCOfferAnswerOptions& options,
      const cricket::MediaSessionOptions& session_options);
  void CreateAnswer(CreateSessionDescriptionObserver* observer,
                    const cricket::MediaSessionOptions& session_options);

  void SetSdesPolicy(cricket::SecurePolicy secure_policy);
  cricket::SecurePolicy SdesPolicy() const;

  sigslot::signal1<const rtc::scoped_refptr<rtc::RTCCertificate>&>
      SignalCertificateReady;

  // For testing.
  bool waiting_for_certificate_for_testing() const {
    return certificate_request_state_ == CERTIFICATE_WAITING;
  }

 private:
  enum CertificateRequestState {
    CERTIFICATE_NOT_NEEDED,
    CERTIFICATE_WAITING,
    CERTIFICATE_SUCCEEDED,
    CERTIFICATE_FAILED,
  };

  WebRtcSessionDescriptionFactory(
      rtc::Thread* signaling_thread,
      cricket::ChannelManager* channel_manager,
      rtc::scoped_ptr<DtlsIdentityStoreInterface> dtls_identity_store,
      const rtc::scoped_refptr<WebRtcIdentityRequestObserver>&
          identity_request_observer,
      WebRtcSession* session,
      const std::string& session_id,
      bool dtls_enabled);

  // MessageHandler implementation.
  virtual void OnMessage(rtc::Message* msg);

  void InternalCreateOffer(CreateSessionDescriptionRequest request);
  void InternalCreateAnswer(CreateSessionDescriptionRequest request);
  // Posts failure notifications for all pending session description requests.
  void FailPendingRequests(const std::string& reason);
  void PostCreateSessionDescriptionFailed(
      CreateSessionDescriptionObserver* observer,
      const std::string& error);
  void PostCreateSessionDescriptionSucceeded(
      CreateSessionDescriptionObserver* observer,
      SessionDescriptionInterface* description);

  void OnIdentityRequestFailed(int error);
  void SetCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);

  std::queue<CreateSessionDescriptionRequest>
      create_session_description_requests_;
  rtc::Thread* const signaling_thread_;
  cricket::TransportDescriptionFactory transport_desc_factory_;
  cricket::MediaSessionDescriptionFactory session_desc_factory_;
  uint64_t session_version_;
  const rtc::scoped_ptr<DtlsIdentityStoreInterface> dtls_identity_store_;
  const rtc::scoped_refptr<WebRtcIdentityRequestObserver>
      identity_request_observer_;
  // TODO(jiayl): remove the dependency on session once bug 2264 is fixed.
  WebRtcSession* const session_;
  const std::string session_id_;
  CertificateRequestState certificate_request_state_;

  RTC_DISALLOW_COPY_AND_ASSIGN(WebRtcSessionDescriptionFactory);
};
}  // namespace webrtc

#endif  // WEBRTC_API_WEBRTCSESSIONDESCRIPTIONFACTORY_H_
