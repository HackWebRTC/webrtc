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

#ifndef TALK_APP_WEBRTC_WEBRTCSESSIONDESCRIPTIONFACTORY_H_
#define TALK_APP_WEBRTC_WEBRTCSESSIONDESCRIPTIONFACTORY_H_

#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/base/messagehandler.h"
#include "talk/p2p/base/transportdescriptionfactory.h"
#include "talk/session/media/mediasession.h"

namespace cricket {
class ChannelManager;
class TransportDescriptionFactory;
}  // namespace cricket

namespace webrtc {
class CreateSessionDescriptionObserver;
class MediaConstraintsInterface;
class MediaStreamSignaling;
class SessionDescriptionInterface;
class WebRtcSession;

// DTLS identity request callback class.
class WebRtcIdentityRequestObserver : public DTLSIdentityRequestObserver,
                                      public sigslot::has_slots<> {
 public:
  // DTLSIdentityRequestObserver overrides.
  virtual void OnFailure(int error) {
    SignalRequestFailed(error);
  }
  virtual void OnSuccess(const std::string& der_cert,
                         const std::string& der_private_key) {
    SignalIdentityReady(der_cert, der_private_key);
  }

  sigslot::signal1<int> SignalRequestFailed;
  sigslot::signal2<const std::string&, const std::string&> SignalIdentityReady;
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
  talk_base::scoped_refptr<CreateSessionDescriptionObserver> observer;
  cricket::MediaSessionOptions options;
};

// This class is used to create offer/answer session description with regards to
// the async DTLS identity generation for WebRtcSession.
// It queues the create offer/answer request until the DTLS identity
// request has completed, i.e. when OnIdentityRequestFailed or OnIdentityReady
// is called.
class WebRtcSessionDescriptionFactory : public talk_base::MessageHandler,
                                        public sigslot::has_slots<>  {
 public:
  WebRtcSessionDescriptionFactory(
      talk_base::Thread* signaling_thread,
      cricket::ChannelManager* channel_manager,
      MediaStreamSignaling* mediastream_signaling,
      DTLSIdentityServiceInterface* dtls_identity_service,
      // TODO(jiayl): remove the dependency on session once b/10226852 is fixed.
      WebRtcSession* session,
      const std::string& session_id,
      cricket::DataChannelType dct,
      bool dtls_enabled);
  virtual ~WebRtcSessionDescriptionFactory();

  static void CopyCandidatesFromSessionDescription(
    const SessionDescriptionInterface* source_desc,
    SessionDescriptionInterface* dest_desc);

  void CreateOffer(
      CreateSessionDescriptionObserver* observer,
      const MediaConstraintsInterface* constraints);
  void CreateAnswer(
      CreateSessionDescriptionObserver* observer,
      const MediaConstraintsInterface* constraints);

  void SetSecure(cricket::SecureMediaPolicy secure_policy);
  cricket::SecureMediaPolicy Secure() const;

  sigslot::signal1<talk_base::SSLIdentity*> SignalIdentityReady;

  // For testing.
  bool waiting_for_identity() const {
    return identity_request_state_ == IDENTITY_WAITING;
  }

 private:
  enum IdentityRequestState {
    IDENTITY_NOT_NEEDED,
    IDENTITY_WAITING,
    IDENTITY_SUCCEEDED,
    IDENTITY_FAILED,
  };

  // MessageHandler implementation.
  virtual void OnMessage(talk_base::Message* msg);

  void InternalCreateOffer(CreateSessionDescriptionRequest request);
  void InternalCreateAnswer(CreateSessionDescriptionRequest request);
  void PostCreateSessionDescriptionFailed(
      CreateSessionDescriptionObserver* observer,
      const std::string& error);
  void PostCreateSessionDescriptionSucceeded(
      CreateSessionDescriptionObserver* observer,
      SessionDescriptionInterface* description);

  void OnIdentityRequestFailed(int error);
  void OnIdentityReady(const std::string& der_cert,
                       const std::string& der_private_key);
  void SetIdentity(talk_base::SSLIdentity* identity);

  std::queue<CreateSessionDescriptionRequest>
      create_session_description_requests_;
  talk_base::Thread* signaling_thread_;
  MediaStreamSignaling* mediastream_signaling_;
  cricket::TransportDescriptionFactory transport_desc_factory_;
  cricket::MediaSessionDescriptionFactory session_desc_factory_;
  uint64 session_version_;
  talk_base::scoped_ptr<DTLSIdentityServiceInterface> identity_service_;
  talk_base::scoped_refptr<WebRtcIdentityRequestObserver>
      identity_request_observer_;
  WebRtcSession* session_;
  std::string session_id_;
  cricket::DataChannelType data_channel_type_;
  IdentityRequestState identity_request_state_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcSessionDescriptionFactory);
};
}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_WEBRTCSESSIONDESCRIPTIONFACTORY_H_
