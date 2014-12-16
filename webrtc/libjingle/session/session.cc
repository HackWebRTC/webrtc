/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/session/session.h"

#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/jid.h"
#include "webrtc/p2p/base/p2ptransport.h"

namespace cricket {

bool BadMessage(const buzz::QName type,
                const std::string& text,
                MessageError* err) {
  err->SetType(type);
  err->SetText(text);
  return false;
}

Session::Session(SessionManager* session_manager,
                 const std::string& local_name,
                 const std::string& initiator_name,
                 const std::string& sid,
                 const std::string& content_type,
                 SessionClient* client)
    : BaseSession(session_manager->signaling_thread(),
                  session_manager->worker_thread(),
                  session_manager->port_allocator(),
                  sid, content_type, initiator_name == local_name) {
  ASSERT(client != NULL);
  session_manager_ = session_manager;
  local_name_ = local_name;
  initiator_name_ = initiator_name;
  transport_parser_ = new P2PTransportParser();
  client_ = client;
  initiate_acked_ = false;
  current_protocol_ = PROTOCOL_HYBRID;
}

Session::~Session() {
  delete transport_parser_;
}

bool Session::Initiate(const std::string& to,
                       const SessionDescription* sdesc) {
  ASSERT(signaling_thread()->IsCurrent());
  SessionError error;

  // Only from STATE_INIT
  if (state() != STATE_INIT)
    return false;

  // Setup for signaling.
  set_remote_name(to);
  set_local_description(sdesc);
  if (!CreateTransportProxies(GetEmptyTransportInfos(sdesc->contents()),
                              &error)) {
    LOG(LS_ERROR) << "Could not create transports: " << error.text;
    return false;
  }

  if (!SendInitiateMessage(sdesc, &error)) {
    LOG(LS_ERROR) << "Could not send initiate message: " << error.text;
    return false;
  }

  // We need to connect transport proxy and impl here so that we can process
  // the TransportDescriptions.
  SpeculativelyConnectAllTransportChannels();

  PushdownTransportDescription(CS_LOCAL, CA_OFFER, NULL);
  SetState(Session::STATE_SENTINITIATE);
  return true;
}

bool Session::Accept(const SessionDescription* sdesc) {
  ASSERT(signaling_thread()->IsCurrent());

  // Only if just received initiate
  if (state() != STATE_RECEIVEDINITIATE)
    return false;

  // Setup for signaling.
  set_local_description(sdesc);

  SessionError error;
  if (!SendAcceptMessage(sdesc, &error)) {
    LOG(LS_ERROR) << "Could not send accept message: " << error.text;
    return false;
  }
  // TODO(juberti): Add BUNDLE support to transport-info messages.
  PushdownTransportDescription(CS_LOCAL, CA_ANSWER, NULL);
  MaybeEnableMuxingSupport();  // Enable transport channel mux if supported.
  SetState(Session::STATE_SENTACCEPT);
  return true;
}

bool Session::Reject(const std::string& reason) {
  ASSERT(signaling_thread()->IsCurrent());

  // Reject is sent in response to an initiate or modify, to reject the
  // request
  if (state() != STATE_RECEIVEDINITIATE && state() != STATE_RECEIVEDMODIFY)
    return false;

  SessionError error;
  if (!SendRejectMessage(reason, &error)) {
    LOG(LS_ERROR) << "Could not send reject message: " << error.text;
    return false;
  }

  SetState(STATE_SENTREJECT);
  return true;
}

bool Session::TerminateWithReason(const std::string& reason) {
  ASSERT(signaling_thread()->IsCurrent());

  // Either side can terminate, at any time.
  switch (state()) {
    case STATE_SENTTERMINATE:
    case STATE_RECEIVEDTERMINATE:
      return false;

    case STATE_SENTREJECT:
    case STATE_RECEIVEDREJECT:
      // We don't need to send terminate if we sent or received a reject...
      // it's implicit.
      break;

    default:
      SessionError error;
      if (!SendTerminateMessage(reason, &error)) {
        LOG(LS_ERROR) << "Could not send terminate message: " << error.text;
        return false;
      }
      break;
  }

  SetState(STATE_SENTTERMINATE);
  return true;
}

bool Session::SendInfoMessage(const XmlElements& elems,
                              const std::string& remote_name) {
  ASSERT(signaling_thread()->IsCurrent());
  SessionError error;
  if (!SendMessage(ACTION_SESSION_INFO, elems, remote_name, &error)) {
    LOG(LS_ERROR) << "Could not send info message " << error.text;
    return false;
  }
  return true;
}

bool Session::SendDescriptionInfoMessage(const ContentInfos& contents) {
  XmlElements elems;
  WriteError write_error;
  if (!WriteDescriptionInfo(current_protocol_,
                            contents,
                            GetContentParsers(),
                            &elems, &write_error)) {
    LOG(LS_ERROR) << "Could not write description info message: "
                  << write_error.text;
    return false;
  }
  SessionError error;
  if (!SendMessage(ACTION_DESCRIPTION_INFO, elems, &error)) {
    LOG(LS_ERROR) << "Could not send description info message: "
                  << error.text;
    return false;
  }
  return true;
}

TransportInfos Session::GetEmptyTransportInfos(
    const ContentInfos& contents) const {
  TransportInfos tinfos;
  for (ContentInfos::const_iterator content = contents.begin();
       content != contents.end(); ++content) {
    tinfos.push_back(TransportInfo(content->name,
                                   TransportDescription(transport_type(),
                                                        std::string(),
                                                        std::string())));
  }
  return tinfos;
}

bool Session::OnRemoteCandidates(
    const TransportInfos& tinfos, ParseError* error) {
  for (TransportInfos::const_iterator tinfo = tinfos.begin();
       tinfo != tinfos.end(); ++tinfo) {
    std::string str_error;
    if (!BaseSession::OnRemoteCandidates(
        tinfo->content_name, tinfo->description.candidates, &str_error)) {
      return BadParse(str_error, error);
    }
  }
  return true;
}

bool Session::CreateTransportProxies(const TransportInfos& tinfos,
                                     SessionError* error) {
  for (TransportInfos::const_iterator tinfo = tinfos.begin();
       tinfo != tinfos.end(); ++tinfo) {
    if (tinfo->description.transport_type != transport_type()) {
      error->SetText("No supported transport in offer.");
      return false;
    }

    GetOrCreateTransportProxy(tinfo->content_name);
  }
  return true;
}

TransportParserMap Session::GetTransportParsers() {
  TransportParserMap parsers;
  parsers[transport_type()] = transport_parser_;
  return parsers;
}

CandidateTranslatorMap Session::GetCandidateTranslators() {
  CandidateTranslatorMap translators;
  // NOTE: This technique makes it impossible to parse G-ICE
  // candidates in session-initiate messages because the channels
  // aren't yet created at that point.  Since we don't use candidates
  // in session-initiate messages, we should be OK.  Once we switch to
  // ICE, this translation shouldn't be necessary.
  for (TransportMap::const_iterator iter = transport_proxies().begin();
       iter != transport_proxies().end(); ++iter) {
    translators[iter->first] = iter->second;
  }
  return translators;
}

ContentParserMap Session::GetContentParsers() {
  ContentParserMap parsers;
  parsers[content_type()] = client_;
  // We need to be able parse both RTP-based and SCTP-based Jingle
  // with the same client.
  if (content_type() == NS_JINGLE_RTP) {
    parsers[NS_JINGLE_DRAFT_SCTP] = client_;
  }
  return parsers;
}

void Session::OnTransportRequestSignaling(Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
  TransportProxy* transproxy = GetTransportProxy(transport);
  ASSERT(transproxy != NULL);
  if (transproxy) {
    // Reset candidate allocation status for the transport proxy.
    transproxy->set_candidates_allocated(false);
  }
  SignalRequestSignaling(this);
}

void Session::OnTransportConnecting(Transport* transport) {
  // This is an indication that we should begin watching the writability
  // state of the transport.
  OnTransportWritable(transport);
}

void Session::OnTransportWritable(Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());

  // If the transport is not writable, start a timer to make sure that it
  // becomes writable within a reasonable amount of time.  If it does not, we
  // terminate since we can't actually send data.  If the transport is writable,
  // cancel the timer.  Note that writability transitions may occur repeatedly
  // during the lifetime of the session.
  signaling_thread()->Clear(this, MSG_TIMEOUT);
  if (transport->HasChannels() && !transport->writable()) {
    signaling_thread()->PostDelayed(
        session_manager_->session_timeout() * 1000, this, MSG_TIMEOUT);
  }
}

void Session::OnTransportProxyCandidatesReady(TransportProxy* transproxy,
                                              const Candidates& candidates) {
  ASSERT(signaling_thread()->IsCurrent());
  if (transproxy != NULL) {
    if (initiator() && !initiate_acked_) {
      // TODO: This is to work around server re-ordering
      // messages.  We send the candidates once the session-initiate
      // is acked.  Once we have fixed the server to guarantee message
      // order, we can remove this case.
      transproxy->AddUnsentCandidates(candidates);
    } else {
      if (!transproxy->negotiated()) {
        transproxy->AddSentCandidates(candidates);
      }
      SessionError error;
      if (!SendTransportInfoMessage(transproxy, candidates, &error)) {
        LOG(LS_ERROR) << "Could not send transport info message: "
                      << error.text;
        return;
      }
    }
  }
}

void Session::OnIncomingMessage(const SessionMessage& msg) {
  ASSERT(signaling_thread()->IsCurrent());
  ASSERT(state() == STATE_INIT || msg.from == remote_name());

  if (current_protocol_== PROTOCOL_HYBRID) {
    if (msg.protocol == PROTOCOL_GINGLE) {
      current_protocol_ = PROTOCOL_GINGLE;
    } else {
      current_protocol_ = PROTOCOL_JINGLE;
    }
  }

  bool valid = false;
  MessageError error;
  switch (msg.type) {
    case ACTION_SESSION_INITIATE:
      valid = OnInitiateMessage(msg, &error);
      break;
    case ACTION_SESSION_INFO:
      valid = OnInfoMessage(msg);
      break;
    case ACTION_SESSION_ACCEPT:
      valid = OnAcceptMessage(msg, &error);
      break;
    case ACTION_SESSION_REJECT:
      valid = OnRejectMessage(msg, &error);
      break;
    case ACTION_SESSION_TERMINATE:
      valid = OnTerminateMessage(msg, &error);
      break;
    case ACTION_TRANSPORT_INFO:
      valid = OnTransportInfoMessage(msg, &error);
      break;
    case ACTION_TRANSPORT_ACCEPT:
      valid = OnTransportAcceptMessage(msg, &error);
      break;
    case ACTION_DESCRIPTION_INFO:
      valid = OnDescriptionInfoMessage(msg, &error);
      break;
    default:
      valid = BadMessage(buzz::QN_STANZA_BAD_REQUEST,
                         "unknown session message type",
                         &error);
  }

  if (valid) {
    SendAcknowledgementMessage(msg.stanza);
  } else {
    SignalErrorMessage(this, msg.stanza, error.type,
                       "modify", error.text, NULL);
  }
}

void Session::OnIncomingResponse(const buzz::XmlElement* orig_stanza,
                                 const buzz::XmlElement* response_stanza,
                                 const SessionMessage& msg) {
  ASSERT(signaling_thread()->IsCurrent());

  if (msg.type == ACTION_SESSION_INITIATE) {
    OnInitiateAcked();
  }
}

void Session::OnInitiateAcked() {
  // TODO: This is to work around server re-ordering
  // messages.  We send the candidates once the session-initiate
  // is acked.  Once we have fixed the server to guarantee message
  // order, we can remove this case.
  if (!initiate_acked_) {
    initiate_acked_ = true;
    SessionError error;
    SendAllUnsentTransportInfoMessages(&error);
  }
}

void Session::OnFailedSend(const buzz::XmlElement* orig_stanza,
                           const buzz::XmlElement* error_stanza) {
  ASSERT(signaling_thread()->IsCurrent());

  SessionMessage msg;
  ParseError parse_error;
  if (!ParseSessionMessage(orig_stanza, &msg, &parse_error)) {
    LOG(LS_ERROR) << "Error parsing failed send: " << parse_error.text
                  << ":" << orig_stanza;
    return;
  }

  // If the error is a session redirect, call OnRedirectError, which will
  // continue the session with a new remote JID.
  SessionRedirect redirect;
  if (FindSessionRedirect(error_stanza, &redirect)) {
    SessionError error;
    if (!OnRedirectError(redirect, &error)) {
      // TODO: Should we send a message back?  The standard
      // says nothing about it.
      std::ostringstream desc;
      desc << "Failed to redirect: " << error.text;
      LOG(LS_ERROR) << desc.str();
      SetError(ERROR_RESPONSE, desc.str());
    }
    return;
  }

  std::string error_type = "cancel";

  const buzz::XmlElement* error = error_stanza->FirstNamed(buzz::QN_ERROR);
  if (error) {
    error_type = error->Attr(buzz::QN_TYPE);

    LOG(LS_ERROR) << "Session error:\n" << error->Str() << "\n"
                  << "in response to:\n" << orig_stanza->Str();
  } else {
    // don't crash if <error> is missing
    LOG(LS_ERROR) << "Session error without <error/> element, ignoring";
    return;
  }

  if (msg.type == ACTION_TRANSPORT_INFO) {
    // Transport messages frequently generate errors because they are sent right
    // when we detect a network failure.  For that reason, we ignore such
    // errors, because if we do not establish writability again, we will
    // terminate anyway.  The exceptions are transport-specific error tags,
    // which we pass on to the respective transport.
  } else if ((error_type != "continue") && (error_type != "wait")) {
    // We do not set an error if the other side said it is okay to continue
    // (possibly after waiting).  These errors can be ignored.
    SetError(ERROR_RESPONSE, "");
  }
}

bool Session::OnInitiateMessage(const SessionMessage& msg,
                                MessageError* error) {
  if (!CheckState(STATE_INIT, error))
    return false;

  SessionInitiate init;
  if (!ParseSessionInitiate(msg.protocol, msg.action_elem,
                            GetContentParsers(), GetTransportParsers(),
                            GetCandidateTranslators(),
                            &init, error))
    return false;

  SessionError session_error;
  if (!CreateTransportProxies(init.transports, &session_error)) {
    return BadMessage(buzz::QN_STANZA_NOT_ACCEPTABLE,
                      session_error.text, error);
  }

  set_remote_name(msg.from);
  set_initiator_name(msg.initiator);
  set_remote_description(new SessionDescription(init.ClearContents(),
                                                init.transports,
                                                init.groups));
  // Updating transport with TransportDescription.
  PushdownTransportDescription(CS_REMOTE, CA_OFFER, NULL);
  SetState(STATE_RECEIVEDINITIATE);

  // Users of Session may listen to state change and call Reject().
  if (state() != STATE_SENTREJECT) {
    if (!OnRemoteCandidates(init.transports, error))
      return false;

    // TODO(juberti): Auto-generate and push down the local transport answer.
    // This is necessary for trickling to work with RFC 5245 ICE.
  }
  return true;
}

bool Session::OnAcceptMessage(const SessionMessage& msg, MessageError* error) {
  if (!CheckState(STATE_SENTINITIATE, error))
    return false;

  SessionAccept accept;
  if (!ParseSessionAccept(msg.protocol, msg.action_elem,
                          GetContentParsers(), GetTransportParsers(),
                          GetCandidateTranslators(),
                          &accept, error)) {
    return false;
  }

  // If we get an accept, we can assume the initiate has been
  // received, even if we haven't gotten an IQ response.
  OnInitiateAcked();

  set_remote_description(new SessionDescription(accept.ClearContents(),
                                                accept.transports,
                                                accept.groups));
  // Updating transport with TransportDescription.
  PushdownTransportDescription(CS_REMOTE, CA_ANSWER, NULL);
  MaybeEnableMuxingSupport();  // Enable transport channel mux if supported.
  SetState(STATE_RECEIVEDACCEPT);

  if (!OnRemoteCandidates(accept.transports, error))
    return false;

  return true;
}

bool Session::OnRejectMessage(const SessionMessage& msg, MessageError* error) {
  if (!CheckState(STATE_SENTINITIATE, error))
    return false;

  SetState(STATE_RECEIVEDREJECT);
  return true;
}

bool Session::OnInfoMessage(const SessionMessage& msg) {
  SignalInfoMessage(this, msg.action_elem);
  return true;
}

bool Session::OnTerminateMessage(const SessionMessage& msg,
                                 MessageError* error) {
  SessionTerminate term;
  if (!ParseSessionTerminate(msg.protocol, msg.action_elem, &term, error))
    return false;

  SignalReceivedTerminateReason(this, term.reason);
  if (term.debug_reason != buzz::STR_EMPTY) {
    LOG(LS_VERBOSE) << "Received error on call: " << term.debug_reason;
  }

  SetState(STATE_RECEIVEDTERMINATE);
  return true;
}

bool Session::OnTransportInfoMessage(const SessionMessage& msg,
                                     MessageError* error) {
  TransportInfos tinfos;
  if (!ParseTransportInfos(msg.protocol, msg.action_elem,
                           initiator_description()->contents(),
                           GetTransportParsers(), GetCandidateTranslators(),
                           &tinfos, error))
    return false;

  if (!OnRemoteCandidates(tinfos, error))
    return false;

  return true;
}

bool Session::OnTransportAcceptMessage(const SessionMessage& msg,
                                       MessageError* error) {
  // TODO: Currently here only for compatibility with
  // Gingle 1.1 clients (notably, Google Voice).
  return true;
}

bool Session::OnDescriptionInfoMessage(const SessionMessage& msg,
                              MessageError* error) {
  if (!CheckState(STATE_INPROGRESS, error))
    return false;

  DescriptionInfo description_info;
  if (!ParseDescriptionInfo(msg.protocol, msg.action_elem,
                            GetContentParsers(), GetTransportParsers(),
                            GetCandidateTranslators(),
                            &description_info, error)) {
    return false;
  }

  ContentInfos& updated_contents = description_info.contents;

  // TODO: Currently, reflector sends back
  // video stream updates even for an audio-only call, which causes
  // this to fail.  Put this back once reflector is fixed.
  //
  // ContentInfos::iterator it;
  // First, ensure all updates are valid before modifying remote_description_.
  // for (it = updated_contents.begin(); it != updated_contents.end(); ++it) {
  //   if (remote_description()->GetContentByName(it->name) == NULL) {
  //     return false;
  //   }
  // }

  // TODO: We used to replace contents from an update, but
  // that no longer works with partial updates.  We need to figure out
  // a way to merge patial updates into contents.  For now, users of
  // Session should listen to SignalRemoteDescriptionUpdate and handle
  // updates.  They should not expect remote_description to be the
  // latest value.
  //
  // for (it = updated_contents.begin(); it != updated_contents.end(); ++it) {
  //     remote_description()->RemoveContentByName(it->name);
  //     remote_description()->AddContent(it->name, it->type, it->description);
  //   }
  // }

  SignalRemoteDescriptionUpdate(this, updated_contents);
  return true;
}

bool BareJidsEqual(const std::string& name1,
                   const std::string& name2) {
  buzz::Jid jid1(name1);
  buzz::Jid jid2(name2);

  return jid1.IsValid() && jid2.IsValid() && jid1.BareEquals(jid2);
}

bool Session::OnRedirectError(const SessionRedirect& redirect,
                              SessionError* error) {
  MessageError message_error;
  if (!CheckState(STATE_SENTINITIATE, &message_error)) {
    return BadWrite(message_error.text, error);
  }

  if (!BareJidsEqual(remote_name(), redirect.target))
    return BadWrite("Redirection not allowed: must be the same bare jid.",
                    error);

  // When we receive a redirect, we point the session at the new JID
  // and resend the candidates.
  set_remote_name(redirect.target);
  return (SendInitiateMessage(local_description(), error) &&
          ResendAllTransportInfoMessages(error));
}

bool Session::CheckState(State expected, MessageError* error) {
  if (state() != expected) {
    // The server can deliver messages out of order/repeated for various
    // reasons. For example, if the server does not recive our iq response,
    // it could assume that the iq it sent was lost, and will then send
    // it again. Ideally, we should implement reliable messaging with
    // duplicate elimination.
    return BadMessage(buzz::QN_STANZA_NOT_ALLOWED,
                      "message not allowed in current state",
                      error);
  }
  return true;
}

void Session::SetError(Error error, const std::string& error_desc) {
  BaseSession::SetError(error, error_desc);
  if (error != ERROR_NONE)
    signaling_thread()->Post(this, MSG_ERROR);
}

void Session::OnMessage(rtc::Message* pmsg) {
  // preserve this because BaseSession::OnMessage may modify it
  State orig_state = state();

  BaseSession::OnMessage(pmsg);

  switch (pmsg->message_id) {
  case MSG_ERROR:
    TerminateWithReason(STR_TERMINATE_ERROR);
    break;

  case MSG_STATE:
    switch (orig_state) {
    case STATE_SENTREJECT:
    case STATE_RECEIVEDREJECT:
      // Assume clean termination.
      Terminate();
      break;

    case STATE_SENTTERMINATE:
    case STATE_RECEIVEDTERMINATE:
      session_manager_->DestroySession(this);
      break;

    default:
      // Explicitly ignoring some states here.
      break;
    }
    break;
  }
}

bool Session::SendInitiateMessage(const SessionDescription* sdesc,
                                  SessionError* error) {
  SessionInitiate init;
  init.contents = sdesc->contents();
  init.transports = GetEmptyTransportInfos(init.contents);
  init.groups = sdesc->groups();
  return SendMessage(ACTION_SESSION_INITIATE, init, error);
}

bool Session::WriteSessionAction(
    SignalingProtocol protocol, const SessionInitiate& init,
    XmlElements* elems, WriteError* error) {
  return WriteSessionInitiate(protocol, init.contents, init.transports,
                              GetContentParsers(), GetTransportParsers(),
                              GetCandidateTranslators(), init.groups,
                              elems, error);
}

bool Session::SendAcceptMessage(const SessionDescription* sdesc,
                                SessionError* error) {
  XmlElements elems;
  if (!WriteSessionAccept(current_protocol_,
                          sdesc->contents(),
                          GetEmptyTransportInfos(sdesc->contents()),
                          GetContentParsers(), GetTransportParsers(),
                          GetCandidateTranslators(), sdesc->groups(),
                          &elems, error)) {
    return false;
  }
  return SendMessage(ACTION_SESSION_ACCEPT, elems, error);
}

bool Session::SendRejectMessage(const std::string& reason,
                                SessionError* error) {
  SessionTerminate term(reason);
  return SendMessage(ACTION_SESSION_REJECT, term, error);
}

bool Session::SendTerminateMessage(const std::string& reason,
                                   SessionError* error) {
  SessionTerminate term(reason);
  return SendMessage(ACTION_SESSION_TERMINATE, term, error);
}

bool Session::WriteSessionAction(SignalingProtocol protocol,
                                 const SessionTerminate& term,
                                 XmlElements* elems, WriteError* error) {
  WriteSessionTerminate(protocol, term, elems);
  return true;
}

bool Session::SendTransportInfoMessage(const TransportInfo& tinfo,
                                       SessionError* error) {
  return SendMessage(ACTION_TRANSPORT_INFO, tinfo, error);
}

bool Session::SendTransportInfoMessage(const TransportProxy* transproxy,
                                       const Candidates& candidates,
                                       SessionError* error) {
  return SendTransportInfoMessage(TransportInfo(transproxy->content_name(),
      TransportDescription(transproxy->type(), std::vector<std::string>(),
                           std::string(), std::string(), ICEMODE_FULL,
                           CONNECTIONROLE_NONE, NULL, candidates)), error);
}

bool Session::WriteSessionAction(SignalingProtocol protocol,
                                 const TransportInfo& tinfo,
                                 XmlElements* elems, WriteError* error) {
  TransportInfos tinfos;
  tinfos.push_back(tinfo);
  return WriteTransportInfos(protocol, tinfos,
                             GetTransportParsers(), GetCandidateTranslators(),
                             elems, error);
}

bool Session::ResendAllTransportInfoMessages(SessionError* error) {
  for (TransportMap::const_iterator iter = transport_proxies().begin();
       iter != transport_proxies().end(); ++iter) {
    TransportProxy* transproxy = iter->second;
    if (transproxy->sent_candidates().size() > 0) {
      if (!SendTransportInfoMessage(
              transproxy, transproxy->sent_candidates(), error)) {
        LOG(LS_ERROR) << "Could not resend transport info messages: "
                      << error->text;
        return false;
      }
      transproxy->ClearSentCandidates();
    }
  }
  return true;
}

bool Session::SendAllUnsentTransportInfoMessages(SessionError* error) {
  for (TransportMap::const_iterator iter = transport_proxies().begin();
       iter != transport_proxies().end(); ++iter) {
    TransportProxy* transproxy = iter->second;
    if (transproxy->unsent_candidates().size() > 0) {
      if (!SendTransportInfoMessage(
              transproxy, transproxy->unsent_candidates(), error)) {
        LOG(LS_ERROR) << "Could not send unsent transport info messages: "
                      << error->text;
        return false;
      }
      transproxy->ClearUnsentCandidates();
    }
  }
  return true;
}

bool Session::SendMessage(ActionType type, const XmlElements& action_elems,
                          SessionError* error) {
    return SendMessage(type, action_elems, remote_name(), error);
}

bool Session::SendMessage(ActionType type, const XmlElements& action_elems,
                          const std::string& remote_name, SessionError* error) {
  rtc::scoped_ptr<buzz::XmlElement> stanza(
      new buzz::XmlElement(buzz::QN_IQ));

  SessionMessage msg(current_protocol_, type, id(), initiator_name());
  msg.to = remote_name;
  WriteSessionMessage(msg, action_elems, stanza.get());

  SignalOutgoingMessage(this, stanza.get());
  return true;
}

template <typename Action>
bool Session::SendMessage(ActionType type, const Action& action,
                          SessionError* error) {
  rtc::scoped_ptr<buzz::XmlElement> stanza(
      new buzz::XmlElement(buzz::QN_IQ));
  if (!WriteActionMessage(type, action, stanza.get(), error))
    return false;

  SignalOutgoingMessage(this, stanza.get());
  return true;
}

template <typename Action>
bool Session::WriteActionMessage(ActionType type, const Action& action,
                                 buzz::XmlElement* stanza,
                                 WriteError* error) {
  if (current_protocol_ == PROTOCOL_HYBRID) {
    if (!WriteActionMessage(PROTOCOL_JINGLE, type, action, stanza, error))
      return false;
    if (!WriteActionMessage(PROTOCOL_GINGLE, type, action, stanza, error))
      return false;
  } else {
    if (!WriteActionMessage(current_protocol_, type, action, stanza, error))
      return false;
  }
  return true;
}

template <typename Action>
bool Session::WriteActionMessage(SignalingProtocol protocol,
                                 ActionType type, const Action& action,
                                 buzz::XmlElement* stanza, WriteError* error) {
  XmlElements action_elems;
  if (!WriteSessionAction(protocol, action, &action_elems, error))
    return false;

  SessionMessage msg(protocol, type, id(), initiator_name());
  msg.to = remote_name();

  WriteSessionMessage(msg, action_elems, stanza);
  return true;
}

void Session::SendAcknowledgementMessage(const buzz::XmlElement* stanza) {
  rtc::scoped_ptr<buzz::XmlElement> ack(
      new buzz::XmlElement(buzz::QN_IQ));
  ack->SetAttr(buzz::QN_TO, remote_name());
  ack->SetAttr(buzz::QN_ID, stanza->Attr(buzz::QN_ID));
  ack->SetAttr(buzz::QN_TYPE, "result");

  SignalOutgoingMessage(this, ack.get());
}

}  // namespace cricket
