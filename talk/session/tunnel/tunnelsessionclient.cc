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

#include "talk/base/basicdefs.h"
#include "talk/base/basictypes.h"
#include "talk/base/common.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/base/stringutils.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/transportchannel.h"
#include "talk/xmllite/xmlelement.h"
#include "pseudotcpchannel.h"
#include "tunnelsessionclient.h"

namespace cricket {

const char NS_TUNNEL[] = "http://www.google.com/talk/tunnel";
const buzz::StaticQName QN_TUNNEL_DESCRIPTION = { NS_TUNNEL, "description" };
const buzz::StaticQName QN_TUNNEL_TYPE = { NS_TUNNEL, "type" };
const char CN_TUNNEL[] = "tunnel";

enum {
  MSG_CLOCK = 1,
  MSG_DESTROY,
  MSG_TERMINATE,
  MSG_EVENT,
  MSG_CREATE_TUNNEL,
};

struct EventData : public talk_base::MessageData {
  int event, error;
  EventData(int ev, int err = 0) : event(ev), error(err) { }
};

struct CreateTunnelData : public talk_base::MessageData {
  buzz::Jid jid;
  std::string description;
  talk_base::Thread* thread;
  talk_base::StreamInterface* stream;
};

extern const talk_base::ConstantLabel SESSION_STATES[];

const talk_base::ConstantLabel SESSION_STATES[] = {
  KLABEL(Session::STATE_INIT),
  KLABEL(Session::STATE_SENTINITIATE),
  KLABEL(Session::STATE_RECEIVEDINITIATE),
  KLABEL(Session::STATE_SENTACCEPT),
  KLABEL(Session::STATE_RECEIVEDACCEPT),
  KLABEL(Session::STATE_SENTMODIFY),
  KLABEL(Session::STATE_RECEIVEDMODIFY),
  KLABEL(Session::STATE_SENTREJECT),
  KLABEL(Session::STATE_RECEIVEDREJECT),
  KLABEL(Session::STATE_SENTREDIRECT),
  KLABEL(Session::STATE_SENTTERMINATE),
  KLABEL(Session::STATE_RECEIVEDTERMINATE),
  KLABEL(Session::STATE_INPROGRESS),
  KLABEL(Session::STATE_DEINIT),
  LASTLABEL
};

///////////////////////////////////////////////////////////////////////////////
// TunnelContentDescription
///////////////////////////////////////////////////////////////////////////////

struct TunnelContentDescription : public ContentDescription {
  std::string description;

  TunnelContentDescription(const std::string& desc) : description(desc) { }
  virtual ContentDescription* Copy() const {
    return new TunnelContentDescription(*this);
  }
};

///////////////////////////////////////////////////////////////////////////////
// TunnelSessionClientBase
///////////////////////////////////////////////////////////////////////////////

TunnelSessionClientBase::TunnelSessionClientBase(const buzz::Jid& jid,
                                SessionManager* manager, const std::string &ns)
  : jid_(jid), session_manager_(manager), namespace_(ns), shutdown_(false) {
  session_manager_->AddClient(namespace_, this);
}

TunnelSessionClientBase::~TunnelSessionClientBase() {
  shutdown_ = true;
  for (std::vector<TunnelSession*>::iterator it = sessions_.begin();
       it != sessions_.end();
       ++it) {
     Session* session = (*it)->ReleaseSession(true);
     session_manager_->DestroySession(session);
  }
  session_manager_->RemoveClient(namespace_);
}

void TunnelSessionClientBase::OnSessionCreate(Session* session, bool received) {
  LOG(LS_INFO) << "TunnelSessionClientBase::OnSessionCreate: received=" 
               << received;
  ASSERT(session_manager_->signaling_thread()->IsCurrent());
  if (received)
    sessions_.push_back(
        MakeTunnelSession(session, talk_base::Thread::Current(), RESPONDER));
}

void TunnelSessionClientBase::OnSessionDestroy(Session* session) {
  LOG(LS_INFO) << "TunnelSessionClientBase::OnSessionDestroy";
  ASSERT(session_manager_->signaling_thread()->IsCurrent());
  if (shutdown_)
    return;
  for (std::vector<TunnelSession*>::iterator it = sessions_.begin();
       it != sessions_.end();
       ++it) {
    if ((*it)->HasSession(session)) {
      VERIFY((*it)->ReleaseSession(false) == session);
      sessions_.erase(it);
      return;
    }
  }
}

talk_base::StreamInterface* TunnelSessionClientBase::CreateTunnel(
    const buzz::Jid& to, const std::string& description) {
  // Valid from any thread
  CreateTunnelData data;
  data.jid = to;
  data.description = description;
  data.thread = talk_base::Thread::Current();
  data.stream = NULL;
  session_manager_->signaling_thread()->Send(this, MSG_CREATE_TUNNEL, &data);
  return data.stream;
}

talk_base::StreamInterface* TunnelSessionClientBase::AcceptTunnel(
    Session* session) {
  ASSERT(session_manager_->signaling_thread()->IsCurrent());
  TunnelSession* tunnel = NULL;
  for (std::vector<TunnelSession*>::iterator it = sessions_.begin();
       it != sessions_.end();
       ++it) {
    if ((*it)->HasSession(session)) {
      tunnel = *it;
      break;
    }
  }
  ASSERT(tunnel != NULL);

  SessionDescription* answer = CreateAnswer(session->remote_description());
  if (answer == NULL)
    return NULL;

  session->Accept(answer);
  return tunnel->GetStream();
}

void TunnelSessionClientBase::DeclineTunnel(Session* session) {
  ASSERT(session_manager_->signaling_thread()->IsCurrent());
  session->Reject(STR_TERMINATE_DECLINE);
}

void TunnelSessionClientBase::OnMessage(talk_base::Message* pmsg) {
  if (pmsg->message_id == MSG_CREATE_TUNNEL) {
    ASSERT(session_manager_->signaling_thread()->IsCurrent());
    CreateTunnelData* data = static_cast<CreateTunnelData*>(pmsg->pdata);
    SessionDescription* offer = CreateOffer(data->jid, data->description);
    if (offer == NULL) {
      return;
    }

    Session* session = session_manager_->CreateSession(jid_.Str(), namespace_);
    TunnelSession* tunnel = MakeTunnelSession(session, data->thread,
                                              INITIATOR);
    sessions_.push_back(tunnel);
    session->Initiate(data->jid.Str(), offer);
    data->stream = tunnel->GetStream();
  }
}

TunnelSession* TunnelSessionClientBase::MakeTunnelSession(
    Session* session, talk_base::Thread* stream_thread,
    TunnelSessionRole /*role*/) {
  return new TunnelSession(this, session, stream_thread);
}

///////////////////////////////////////////////////////////////////////////////
// TunnelSessionClient
///////////////////////////////////////////////////////////////////////////////

TunnelSessionClient::TunnelSessionClient(const buzz::Jid& jid,
                                         SessionManager* manager,
                                         const std::string &ns)
    : TunnelSessionClientBase(jid, manager, ns) {
}

TunnelSessionClient::TunnelSessionClient(const buzz::Jid& jid,
                                         SessionManager* manager)
    : TunnelSessionClientBase(jid, manager, NS_TUNNEL) {
}

TunnelSessionClient::~TunnelSessionClient() {
}


bool TunnelSessionClient::ParseContent(SignalingProtocol protocol,
                                       const buzz::XmlElement* elem,
                                       ContentDescription** content,
                                       ParseError* error) {
  if (const buzz::XmlElement* type_elem = elem->FirstNamed(QN_TUNNEL_TYPE)) {
    *content = new TunnelContentDescription(type_elem->BodyText());
    return true;
  }
  return false;
}

bool TunnelSessionClient::WriteContent(
    SignalingProtocol protocol,
    const ContentDescription* untyped_content,
    buzz::XmlElement** elem, WriteError* error) {
  const TunnelContentDescription* content =
      static_cast<const TunnelContentDescription*>(untyped_content);

  buzz::XmlElement* root = new buzz::XmlElement(QN_TUNNEL_DESCRIPTION, true);
  buzz::XmlElement* type_elem = new buzz::XmlElement(QN_TUNNEL_TYPE);
  type_elem->SetBodyText(content->description);
  root->AddElement(type_elem);
  *elem = root;
  return true;
}

SessionDescription* NewTunnelSessionDescription(
    const std::string& content_name, ContentDescription* content) {
  SessionDescription* sdesc = new SessionDescription();
  sdesc->AddContent(content_name, NS_TUNNEL, content);
  return sdesc;
}

bool FindTunnelContent(const cricket::SessionDescription* sdesc,
                       std::string* name,
                       const TunnelContentDescription** content) {
  const ContentInfo* cinfo = sdesc->FirstContentByType(NS_TUNNEL);
  if (cinfo == NULL)
    return false;

  *name = cinfo->name;
  *content = static_cast<const TunnelContentDescription*>(
      cinfo->description);
  return true;
}

void TunnelSessionClient::OnIncomingTunnel(const buzz::Jid &jid,
                                           Session *session) {
  std::string content_name;
  const TunnelContentDescription* content = NULL;
  if (!FindTunnelContent(session->remote_description(),
                         &content_name, &content)) {
    session->Reject(STR_TERMINATE_INCOMPATIBLE_PARAMETERS);
    return;
  }

  SignalIncomingTunnel(this, jid, content->description, session);
}

SessionDescription* TunnelSessionClient::CreateOffer(
    const buzz::Jid &jid, const std::string &description) {
  SessionDescription* offer = NewTunnelSessionDescription(
      CN_TUNNEL, new TunnelContentDescription(description));
  talk_base::scoped_ptr<TransportDescription> tdesc(
      session_manager_->transport_desc_factory()->CreateOffer(
          TransportOptions(), NULL));
  if (tdesc.get()) {
    offer->AddTransportInfo(TransportInfo(CN_TUNNEL, *tdesc));
  } else {
    delete offer;
    offer = NULL;
  }
  return offer;
}

SessionDescription* TunnelSessionClient::CreateAnswer(
    const SessionDescription* offer) {
  std::string content_name;
  const TunnelContentDescription* offer_tunnel = NULL;
  if (!FindTunnelContent(offer, &content_name, &offer_tunnel))
    return NULL;

  SessionDescription* answer = NewTunnelSessionDescription(
      content_name, new TunnelContentDescription(offer_tunnel->description));
  const TransportInfo* tinfo = offer->GetTransportInfoByName(content_name);
  if (tinfo) {
    const TransportDescription* offer_tdesc = &tinfo->description;
    ASSERT(offer_tdesc != NULL);
    talk_base::scoped_ptr<TransportDescription> tdesc(
      session_manager_->transport_desc_factory()->CreateAnswer(
          offer_tdesc, TransportOptions(),  NULL));
    if (tdesc.get()) {
      answer->AddTransportInfo(TransportInfo(content_name, *tdesc));
    } else {
      delete answer;
      answer = NULL;
    }
  }
  return answer;
}
///////////////////////////////////////////////////////////////////////////////
// TunnelSession
///////////////////////////////////////////////////////////////////////////////

//
// Signalling thread methods
//

TunnelSession::TunnelSession(TunnelSessionClientBase* client, Session* session,
                             talk_base::Thread* stream_thread)
    : client_(client), session_(session), channel_(NULL) {
  ASSERT(client_ != NULL);
  ASSERT(session_ != NULL);
  session_->SignalState.connect(this, &TunnelSession::OnSessionState);
  channel_ = new PseudoTcpChannel(stream_thread, session_);
  channel_->SignalChannelClosed.connect(this, &TunnelSession::OnChannelClosed);
}

TunnelSession::~TunnelSession() {
  ASSERT(client_ != NULL);
  ASSERT(session_ == NULL);
  ASSERT(channel_ == NULL);
}

talk_base::StreamInterface* TunnelSession::GetStream() {
  ASSERT(channel_ != NULL);
  return channel_->GetStream();
}

bool TunnelSession::HasSession(Session* session) {
  ASSERT(NULL != session_);
  return (session_ == session);
}

Session* TunnelSession::ReleaseSession(bool channel_exists) {
  ASSERT(NULL != session_);
  ASSERT(NULL != channel_);
  Session* session = session_;
  session_->SignalState.disconnect(this);
  session_ = NULL;
  if (channel_exists)
    channel_->SignalChannelClosed.disconnect(this);
  channel_ = NULL;
  delete this;
  return session;
}

void TunnelSession::OnSessionState(BaseSession* session,
                                   BaseSession::State state) {
  LOG(LS_INFO) << "TunnelSession::OnSessionState("
               << talk_base::nonnull(
                    talk_base::FindLabel(state, SESSION_STATES), "Unknown")
               << ")";
  ASSERT(session == session_);

  switch (state) {
  case Session::STATE_RECEIVEDINITIATE:
    OnInitiate();
    break;
  case Session::STATE_SENTACCEPT:
  case Session::STATE_RECEIVEDACCEPT:
    OnAccept();
    break;
  case Session::STATE_SENTTERMINATE:
  case Session::STATE_RECEIVEDTERMINATE:
    OnTerminate();
    break;
  case Session::STATE_DEINIT:
    // ReleaseSession should have been called before this.
    ASSERT(false);
    break;
  default:
    break;
  }
}

void TunnelSession::OnInitiate() {
  ASSERT(client_ != NULL);
  ASSERT(session_ != NULL);
  client_->OnIncomingTunnel(buzz::Jid(session_->remote_name()), session_);
}

void TunnelSession::OnAccept() {
  ASSERT(channel_ != NULL);
  const ContentInfo* content =
      session_->remote_description()->FirstContentByType(NS_TUNNEL);
  ASSERT(content != NULL);
  VERIFY(channel_->Connect(
      content->name, "tcp", ICE_CANDIDATE_COMPONENT_DEFAULT));
}

void TunnelSession::OnTerminate() {
  ASSERT(channel_ != NULL);
  channel_->OnSessionTerminate(session_);
}

void TunnelSession::OnChannelClosed(PseudoTcpChannel* channel) {
  ASSERT(channel_ == channel);
  ASSERT(session_ != NULL);
  session_->Terminate();
}

///////////////////////////////////////////////////////////////////////////////

} // namespace cricket
