/*
 * libjingle
 * Copyright 2004--2008, Google Inc.
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

// SecureTunnelSessionClient and SecureTunnelSession implementation.

#include "talk/session/tunnel/securetunnelsessionclient.h"
#include "talk/base/basicdefs.h"
#include "talk/base/basictypes.h"
#include "talk/base/common.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/base/stringutils.h"
#include "talk/base/sslidentity.h"
#include "talk/base/sslstreamadapter.h"
#include "talk/p2p/base/transportchannel.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/session/tunnel/pseudotcpchannel.h"

namespace cricket {

// XML elements and namespaces for XMPP stanzas used in content exchanges.

const char NS_SECURE_TUNNEL[] = "http://www.google.com/talk/securetunnel";
const buzz::StaticQName QN_SECURE_TUNNEL_DESCRIPTION =
    { NS_SECURE_TUNNEL, "description" };
const buzz::StaticQName QN_SECURE_TUNNEL_TYPE =
    { NS_SECURE_TUNNEL, "type" };
const buzz::StaticQName QN_SECURE_TUNNEL_CLIENT_CERT =
    { NS_SECURE_TUNNEL, "client-cert" };
const buzz::StaticQName QN_SECURE_TUNNEL_SERVER_CERT =
    { NS_SECURE_TUNNEL, "server-cert" };
const char CN_SECURE_TUNNEL[] = "securetunnel";

// SecureTunnelContentDescription

// TunnelContentDescription is extended to hold string forms of the
// client and server certificate, PEM encoded.

struct SecureTunnelContentDescription : public ContentDescription {
  std::string description;
  std::string client_pem_certificate;
  std::string server_pem_certificate;

  SecureTunnelContentDescription(const std::string& desc,
                                 const std::string& client_pem_cert,
                                 const std::string& server_pem_cert)
      : description(desc),
        client_pem_certificate(client_pem_cert),
        server_pem_certificate(server_pem_cert) {
  }
  virtual ContentDescription* Copy() const {
    return new SecureTunnelContentDescription(*this);
  }
};

// SecureTunnelSessionClient

SecureTunnelSessionClient::SecureTunnelSessionClient(
    const buzz::Jid& jid, SessionManager* manager)
    : TunnelSessionClient(jid, manager, NS_SECURE_TUNNEL) {
}

void SecureTunnelSessionClient::SetIdentity(talk_base::SSLIdentity* identity) {
  ASSERT(identity_.get() == NULL);
  identity_.reset(identity);
}

bool SecureTunnelSessionClient::GenerateIdentity() {
  ASSERT(identity_.get() == NULL);
  identity_.reset(talk_base::SSLIdentity::Generate(
      // The name on the certificate does not matter: the peer will
      // make sure the cert it gets during SSL negotiation matches the
      // one it got from XMPP. It would be neat to put something
      // recognizable in there such as the JID, except this will show
      // in clear during the SSL negotiation and so it could be a
      // privacy issue. Specifying an empty string here causes
      // it to use a random string.
#ifdef _DEBUG
      jid().Str()
#else
      ""
#endif
      ));
  if (identity_.get() == NULL) {
    LOG(LS_ERROR) << "Failed to generate SSL identity";
    return false;
  }
  return true;
}

talk_base::SSLIdentity& SecureTunnelSessionClient::GetIdentity() const {
  ASSERT(identity_.get() != NULL);
  return *identity_;
}

// Parses a certificate from a PEM encoded string.
// Returns NULL on failure.
// The caller is responsible for freeing the returned object.
static talk_base::SSLCertificate* ParseCertificate(
    const std::string& pem_cert) {
  if (pem_cert.empty())
    return NULL;
  return talk_base::SSLCertificate::FromPEMString(pem_cert);
}

TunnelSession* SecureTunnelSessionClient::MakeTunnelSession(
    Session* session, talk_base::Thread* stream_thread,
    TunnelSessionRole role) {
  return new SecureTunnelSession(this, session, stream_thread, role);
}

bool FindSecureTunnelContent(const cricket::SessionDescription* sdesc,
                             std::string* name,
                             const SecureTunnelContentDescription** content) {
  const ContentInfo* cinfo = sdesc->FirstContentByType(NS_SECURE_TUNNEL);
  if (cinfo == NULL)
    return false;

  *name = cinfo->name;
  *content = static_cast<const SecureTunnelContentDescription*>(
      cinfo->description);
  return true;
}

void SecureTunnelSessionClient::OnIncomingTunnel(const buzz::Jid &jid,
                                                 Session *session) {
  std::string content_name;
  const SecureTunnelContentDescription* content = NULL;
  if (!FindSecureTunnelContent(session->remote_description(),
                               &content_name, &content)) {
    ASSERT(false);
  }

  // Validate the certificate
  talk_base::scoped_ptr<talk_base::SSLCertificate> peer_cert(
      ParseCertificate(content->client_pem_certificate));
  if (peer_cert.get() == NULL) {
    LOG(LS_ERROR)
        << "Rejecting incoming secure tunnel with invalid cetificate";
    DeclineTunnel(session);
    return;
  }
  // If there were a convenient place we could have cached the
  // peer_cert so as not to have to parse it a second time when
  // configuring the tunnel.
  SignalIncomingTunnel(this, jid, content->description, session);
}

// The XML representation of a session initiation request (XMPP IQ),
// containing the initiator's SecureTunnelContentDescription,
// looks something like this:
// <iq from="INITIATOR@gmail.com/pcpE101B7F4"
//       to="RECIPIENT@gmail.com/pcp8B87F0A3"
//       type="set" id="3">
//   <session xmlns="http://www.google.com/session"
//       type="initiate" id="2508605813"
//       initiator="INITIATOR@gmail.com/pcpE101B7F4">
//     <description xmlns="http://www.google.com/talk/securetunnel">
//       <type>send:filename</type>
//       <client-cert>
//         -----BEGIN CERTIFICATE-----
//         INITIATOR'S CERTIFICATE IN PERM FORMAT (ASCII GIBBERISH)
//         -----END CERTIFICATE-----
//       </client-cert>
//     </description>
//     <transport xmlns="http://www.google.com/transport/p2p"/>
//   </session>
// </iq>

// The session accept iq, containing the recipient's certificate and
// echoing the initiator's certificate, looks something like this:
// <iq from="RECIPIENT@gmail.com/pcpE101B7F4"
//     to="INITIATOR@gmail.com/pcpE101B7F4"
//     type="set" id="5">
//   <session xmlns="http://www.google.com/session"
//       type="accept" id="2508605813"
//       initiator="sdoyon911@gmail.com/pcpE101B7F4">
//     <description xmlns="http://www.google.com/talk/securetunnel">
//       <type>send:FILENAME</type>
//       <client-cert>
//         -----BEGIN CERTIFICATE-----
//         INITIATOR'S CERTIFICATE IN PERM FORMAT (ASCII GIBBERISH)
//         -----END CERTIFICATE-----
//       </client-cert>
//       <server-cert>
//         -----BEGIN CERTIFICATE-----
//         RECIPIENT'S CERTIFICATE IN PERM FORMAT (ASCII GIBBERISH)
//         -----END CERTIFICATE-----
//       </server-cert>
//     </description>
//   </session>
// </iq>


bool SecureTunnelSessionClient::ParseContent(SignalingProtocol protocol,
                                             const buzz::XmlElement* elem,
                                             ContentDescription** content,
                                             ParseError* error) {
  const buzz::XmlElement* type_elem = elem->FirstNamed(QN_SECURE_TUNNEL_TYPE);

  if (type_elem == NULL)
    // Missing mandatory XML element.
    return false;

  // Here we consider the certificate components to be optional. In
  // practice the client certificate is always present, and the server
  // certificate is initially missing from the session description
  // sent during session initiation. OnAccept() will enforce that we
  // have a certificate for our peer.
  const buzz::XmlElement* client_cert_elem =
      elem->FirstNamed(QN_SECURE_TUNNEL_CLIENT_CERT);
  const buzz::XmlElement* server_cert_elem =
      elem->FirstNamed(QN_SECURE_TUNNEL_SERVER_CERT);
  *content = new SecureTunnelContentDescription(
      type_elem->BodyText(),
      client_cert_elem ? client_cert_elem->BodyText() : "",
      server_cert_elem ? server_cert_elem->BodyText() : "");
  return true;
}

bool SecureTunnelSessionClient::WriteContent(
    SignalingProtocol protocol, const ContentDescription* untyped_content,
    buzz::XmlElement** elem, WriteError* error) {
  const SecureTunnelContentDescription* content =
      static_cast<const SecureTunnelContentDescription*>(untyped_content);

  buzz::XmlElement* root =
      new buzz::XmlElement(QN_SECURE_TUNNEL_DESCRIPTION, true);
  buzz::XmlElement* type_elem = new buzz::XmlElement(QN_SECURE_TUNNEL_TYPE);
  type_elem->SetBodyText(content->description);
  root->AddElement(type_elem);
  if (!content->client_pem_certificate.empty()) {
    buzz::XmlElement* client_cert_elem =
        new buzz::XmlElement(QN_SECURE_TUNNEL_CLIENT_CERT);
    client_cert_elem->SetBodyText(content->client_pem_certificate);
    root->AddElement(client_cert_elem);
  }
  if (!content->server_pem_certificate.empty()) {
    buzz::XmlElement* server_cert_elem =
        new buzz::XmlElement(QN_SECURE_TUNNEL_SERVER_CERT);
    server_cert_elem->SetBodyText(content->server_pem_certificate);
    root->AddElement(server_cert_elem);
  }
  *elem = root;
  return true;
}

SessionDescription* NewSecureTunnelSessionDescription(
    const std::string& content_name, ContentDescription* content) {
  SessionDescription* sdesc = new SessionDescription();
  sdesc->AddContent(content_name, NS_SECURE_TUNNEL, content);
  return sdesc;
}

SessionDescription* SecureTunnelSessionClient::CreateOffer(
    const buzz::Jid &jid, const std::string &description) {
  // We are the initiator so we are the client. Put our cert into the
  // description.
  std::string pem_cert = GetIdentity().certificate().ToPEMString();
  return NewSecureTunnelSessionDescription(
      CN_SECURE_TUNNEL,
      new SecureTunnelContentDescription(description, pem_cert, ""));
}

SessionDescription* SecureTunnelSessionClient::CreateAnswer(
    const SessionDescription* offer) {
  std::string content_name;
  const SecureTunnelContentDescription* offer_tunnel = NULL;
  if (!FindSecureTunnelContent(offer, &content_name, &offer_tunnel))
    return NULL;

  // We are accepting a session request. We need to add our cert, the
  // server cert, into the description. The client cert was validated
  // in OnIncomingTunnel().
  ASSERT(!offer_tunnel->client_pem_certificate.empty());
  return NewSecureTunnelSessionDescription(
      content_name,
      new SecureTunnelContentDescription(
          offer_tunnel->description,
          offer_tunnel->client_pem_certificate,
          GetIdentity().certificate().ToPEMString()));
}

// SecureTunnelSession

SecureTunnelSession::SecureTunnelSession(
    SecureTunnelSessionClient* client, Session* session,
    talk_base::Thread* stream_thread, TunnelSessionRole role)
    : TunnelSession(client, session, stream_thread),
      role_(role) {
}

talk_base::StreamInterface* SecureTunnelSession::MakeSecureStream(
    talk_base::StreamInterface* stream) {
  talk_base::SSLStreamAdapter* ssl_stream =
      talk_base::SSLStreamAdapter::Create(stream);
  talk_base::SSLIdentity* identity =
      static_cast<SecureTunnelSessionClient*>(client_)->
      GetIdentity().GetReference();
  ssl_stream->SetIdentity(identity);
  if (role_ == RESPONDER)
    ssl_stream->SetServerRole();
  ssl_stream->StartSSLWithPeer();

  // SSL negotiation will start on the stream as soon as it
  // opens. However our SSLStreamAdapter still hasn't been told what
  // certificate to allow for our peer. If we are the initiator, we do
  // not have the peer's certificate yet: we will obtain it from the
  // session accept message which we will receive later (see
  // OnAccept()). We won't Connect() the PseudoTcpChannel until we get
  // that, so the stream will stay closed until then.  Keep a handle
  // on the streem so we can configure the peer certificate later.
  ssl_stream_reference_.reset(new talk_base::StreamReference(ssl_stream));
  return ssl_stream_reference_->NewReference();
}

talk_base::StreamInterface* SecureTunnelSession::GetStream() {
  ASSERT(channel_ != NULL);
  ASSERT(ssl_stream_reference_.get() == NULL);
  return MakeSecureStream(channel_->GetStream());
}

void SecureTunnelSession::OnAccept() {
  // We have either sent or received a session accept: it's time to
  // connect the tunnel. First we must set the peer certificate.
  ASSERT(channel_ != NULL);
  ASSERT(session_ != NULL);
  std::string content_name;
  const SecureTunnelContentDescription* remote_tunnel = NULL;
  if (!FindSecureTunnelContent(session_->remote_description(),
                               &content_name, &remote_tunnel)) {
    session_->Reject(STR_TERMINATE_INCOMPATIBLE_PARAMETERS);
    return;
  }

  const std::string& cert_pem =
      role_ == INITIATOR ? remote_tunnel->server_pem_certificate :
                           remote_tunnel->client_pem_certificate;
  talk_base::scoped_ptr<talk_base::SSLCertificate> peer_cert(
      ParseCertificate(cert_pem));
  if (peer_cert == NULL) {
    ASSERT(role_ == INITIATOR);  // when RESPONDER we validated it earlier
    LOG(LS_ERROR)
        << "Rejecting secure tunnel accept with invalid cetificate";
    session_->Reject(STR_TERMINATE_INCOMPATIBLE_PARAMETERS);
    return;
  }
  ASSERT(ssl_stream_reference_.get() != NULL);
  talk_base::SSLStreamAdapter* ssl_stream =
      static_cast<talk_base::SSLStreamAdapter*>(
          ssl_stream_reference_->GetStream());

  std::string algorithm;
  if (!peer_cert->GetSignatureDigestAlgorithm(&algorithm)) {
    LOG(LS_ERROR) << "Failed to get the algorithm for the peer cert signature";
    return;
  }
  unsigned char digest[talk_base::MessageDigest::kMaxSize];
  size_t digest_len;
  peer_cert->ComputeDigest(algorithm, digest, ARRAY_SIZE(digest), &digest_len);
  ssl_stream->SetPeerCertificateDigest(algorithm, digest, digest_len);

  // We no longer need our handle to the ssl stream.
  ssl_stream_reference_.reset();
  LOG(LS_INFO) << "Connecting tunnel";
  // This will try to connect the PseudoTcpChannel. If and when that
  // succeeds, then ssl negotiation will take place, and when that
  // succeeds, the tunnel stream will finally open.
  VERIFY(channel_->Connect(
      content_name, "tcp", ICE_CANDIDATE_COMPONENT_DEFAULT));
}

}  // namespace cricket
