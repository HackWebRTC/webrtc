/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#include "talk/p2p/base/session.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/helpers.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sslstreamadapter.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/jid.h"
#include "talk/p2p/base/dtlstransport.h"
#include "talk/p2p/base/p2ptransport.h"
#include "talk/p2p/base/sessionclient.h"
#include "talk/p2p/base/transport.h"
#include "talk/p2p/base/transportchannelproxy.h"
#include "talk/p2p/base/transportinfo.h"

#include "talk/p2p/base/constants.h"

namespace cricket {

bool BadMessage(const buzz::QName type,
                const std::string& text,
                MessageError* err) {
  err->SetType(type);
  err->SetText(text);
  return false;
}

TransportProxy::~TransportProxy() {
  for (ChannelMap::iterator iter = channels_.begin();
       iter != channels_.end(); ++iter) {
    iter->second->SignalDestroyed(iter->second);
    delete iter->second;
  }
}

std::string TransportProxy::type() const {
  return transport_->get()->type();
}

TransportChannel* TransportProxy::GetChannel(int component) {
  return GetChannelProxy(component);
}

TransportChannel* TransportProxy::CreateChannel(
    const std::string& name, int component) {
  ASSERT(GetChannel(component) == NULL);
  ASSERT(!transport_->get()->HasChannel(component));

  // We always create a proxy in case we need to change out the transport later.
  TransportChannelProxy* channel =
      new TransportChannelProxy(content_name(), name, component);
  channels_[component] = channel;

  // If we're already negotiated, create an impl and hook it up to the proxy
  // channel. If we're connecting, create an impl but don't hook it up yet.
  if (negotiated_) {
    SetChannelProxyImpl(component, channel);
  } else if (connecting_) {
    GetOrCreateChannelProxyImpl(component);
  }
  return channel;
}

bool TransportProxy::HasChannel(int component) {
  return transport_->get()->HasChannel(component);
}

void TransportProxy::DestroyChannel(int component) {
  TransportChannel* channel = GetChannel(component);
  if (channel) {
    // If the state of TransportProxy is not NEGOTIATED
    // then TransportChannelProxy and its impl are not
    // connected. Both must be connected before
    // deletion.
    if (!negotiated_) {
      SetChannelProxyImpl(component, GetChannelProxy(component));
    }

    channels_.erase(component);
    channel->SignalDestroyed(channel);
    delete channel;
  }
}

void TransportProxy::ConnectChannels() {
  if (!connecting_) {
    if (!negotiated_) {
      for (ChannelMap::iterator iter = channels_.begin();
           iter != channels_.end(); ++iter) {
        GetOrCreateChannelProxyImpl(iter->first);
      }
    }
    connecting_ = true;
  }
  // TODO(juberti): Right now Transport::ConnectChannels doesn't work if we
  // don't have any channels yet, so we need to allow this method to be called
  // multiple times. Once we fix Transport, we can move this call inside the
  // if (!connecting_) block.
  transport_->get()->ConnectChannels();
}

void TransportProxy::CompleteNegotiation() {
  if (!negotiated_) {
    for (ChannelMap::iterator iter = channels_.begin();
         iter != channels_.end(); ++iter) {
      SetChannelProxyImpl(iter->first, iter->second);
    }
    negotiated_ = true;
  }
}

void TransportProxy::AddSentCandidates(const Candidates& candidates) {
  for (Candidates::const_iterator cand = candidates.begin();
       cand != candidates.end(); ++cand) {
    sent_candidates_.push_back(*cand);
  }
}

void TransportProxy::AddUnsentCandidates(const Candidates& candidates) {
  for (Candidates::const_iterator cand = candidates.begin();
       cand != candidates.end(); ++cand) {
    unsent_candidates_.push_back(*cand);
  }
}

bool TransportProxy::GetChannelNameFromComponent(
    int component, std::string* channel_name) const {
  const TransportChannelProxy* channel = GetChannelProxy(component);
  if (channel == NULL) {
    return false;
  }

  *channel_name = channel->name();
  return true;
}

bool TransportProxy::GetComponentFromChannelName(
    const std::string& channel_name, int* component) const {
  const TransportChannelProxy* channel = GetChannelProxyByName(channel_name);
  if (channel == NULL) {
    return false;
  }

  *component = channel->component();
  return true;
}

TransportChannelProxy* TransportProxy::GetChannelProxy(int component) const {
  ChannelMap::const_iterator iter = channels_.find(component);
  return (iter != channels_.end()) ? iter->second : NULL;
}

TransportChannelProxy* TransportProxy::GetChannelProxyByName(
    const std::string& name) const {
  for (ChannelMap::const_iterator iter = channels_.begin();
       iter != channels_.end();
       ++iter) {
    if (iter->second->name() == name) {
      return iter->second;
    }
  }
  return NULL;
}

TransportChannelImpl* TransportProxy::GetOrCreateChannelProxyImpl(
    int component) {
  TransportChannelImpl* impl = transport_->get()->GetChannel(component);
  if (impl == NULL) {
    impl = transport_->get()->CreateChannel(component);
    impl->SetSessionId(sid_);
  }
  return impl;
}

void TransportProxy::SetChannelProxyImpl(
    int component, TransportChannelProxy* transproxy) {
  TransportChannelImpl* impl = GetOrCreateChannelProxyImpl(component);
  ASSERT(impl != NULL);
  transproxy->SetImplementation(impl);
}

// This function muxes |this| onto |target| by repointing |this| at
// |target|'s transport and setting our TransportChannelProxies
// to point to |target|'s underlying implementations.
bool TransportProxy::SetupMux(TransportProxy* target) {
  // Bail out if there's nothing to do.
  if (transport_ == target->transport_) {
    return true;
  }

  // Run through all channels and remove any non-rtp transport channels before
  // setting target transport channels.
  for (ChannelMap::const_iterator iter = channels_.begin();
       iter != channels_.end(); ++iter) {
    if (!target->transport_->get()->HasChannel(iter->first)) {
      // Remove if channel doesn't exist in |transport_|.
      iter->second->SetImplementation(NULL);
    } else {
      // Replace the impl for all the TransportProxyChannels with the channels
      // from |target|'s transport. Fail if there's not an exact match.
      iter->second->SetImplementation(
          target->transport_->get()->CreateChannel(iter->first));
    }
  }

  // Now replace our transport. Must happen afterwards because
  // it deletes all impls as a side effect.
  transport_ = target->transport_;
  transport_->get()->SignalCandidatesReady.connect(
      this, &TransportProxy::OnTransportCandidatesReady);
  set_candidates_allocated(target->candidates_allocated());
  return true;
}

void TransportProxy::SetRole(TransportRole role) {
  transport_->get()->SetRole(role);
}

bool TransportProxy::SetLocalTransportDescription(
    const TransportDescription& description, ContentAction action) {
  // If this is an answer, finalize the negotiation.
  if (action == CA_ANSWER) {
    CompleteNegotiation();
  }
  return transport_->get()->SetLocalTransportDescription(description, action);
}

bool TransportProxy::SetRemoteTransportDescription(
    const TransportDescription& description, ContentAction action) {
  // If this is an answer, finalize the negotiation.
  if (action == CA_ANSWER) {
    CompleteNegotiation();
  }
  return transport_->get()->SetRemoteTransportDescription(description, action);
}

void TransportProxy::OnSignalingReady() {
  // If we're starting a new allocation sequence, reset our state.
  set_candidates_allocated(false);
  transport_->get()->OnSignalingReady();
}

bool TransportProxy::OnRemoteCandidates(const Candidates& candidates,
                                        std::string* error) {
  // Ensure the transport is negotiated before handling candidates.
  // TODO(juberti): Remove this once everybody calls SetLocalTD.
  CompleteNegotiation();

  // Verify each candidate before passing down to transport layer.
  for (Candidates::const_iterator cand = candidates.begin();
       cand != candidates.end(); ++cand) {
    if (!transport_->get()->VerifyCandidate(*cand, error))
      return false;
    if (!HasChannel(cand->component())) {
      *error = "Candidate has unknown component: " + cand->ToString() +
               " for content: " + content_name_;
      return false;
    }
  }
  transport_->get()->OnRemoteCandidates(candidates);
  return true;
}

std::string BaseSession::StateToString(State state) {
  switch (state) {
    case Session::STATE_INIT:
      return "STATE_INIT";
    case Session::STATE_SENTINITIATE:
      return "STATE_SENTINITIATE";
    case Session::STATE_RECEIVEDINITIATE:
      return "STATE_RECEIVEDINITIATE";
    case Session::STATE_SENTPRACCEPT:
      return "STATE_SENTPRACCEPT";
    case Session::STATE_SENTACCEPT:
      return "STATE_SENTACCEPT";
    case Session::STATE_RECEIVEDPRACCEPT:
      return "STATE_RECEIVEDPRACCEPT";
    case Session::STATE_RECEIVEDACCEPT:
      return "STATE_RECEIVEDACCEPT";
    case Session::STATE_SENTMODIFY:
      return "STATE_SENTMODIFY";
    case Session::STATE_RECEIVEDMODIFY:
      return "STATE_RECEIVEDMODIFY";
    case Session::STATE_SENTREJECT:
      return "STATE_SENTREJECT";
    case Session::STATE_RECEIVEDREJECT:
      return "STATE_RECEIVEDREJECT";
    case Session::STATE_SENTREDIRECT:
      return "STATE_SENTREDIRECT";
    case Session::STATE_SENTTERMINATE:
      return "STATE_SENTTERMINATE";
    case Session::STATE_RECEIVEDTERMINATE:
      return "STATE_RECEIVEDTERMINATE";
    case Session::STATE_INPROGRESS:
      return "STATE_INPROGRESS";
    case Session::STATE_DEINIT:
      return "STATE_DEINIT";
    default:
      break;
  }
  return "STATE_" + talk_base::ToString(state);
}

BaseSession::BaseSession(talk_base::Thread* signaling_thread,
                         talk_base::Thread* worker_thread,
                         PortAllocator* port_allocator,
                         const std::string& sid,
                         const std::string& content_type,
                         bool initiator)
    : state_(STATE_INIT),
      error_(ERROR_NONE),
      signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      port_allocator_(port_allocator),
      sid_(sid),
      content_type_(content_type),
      transport_type_(NS_GINGLE_P2P),
      initiator_(initiator),
      identity_(NULL),
      local_description_(NULL),
      remote_description_(NULL),
      ice_tiebreaker_(talk_base::CreateRandomId64()),
      role_switch_(false) {
  ASSERT(signaling_thread->IsCurrent());
}

BaseSession::~BaseSession() {
  ASSERT(signaling_thread()->IsCurrent());

  ASSERT(state_ != STATE_DEINIT);
  LogState(state_, STATE_DEINIT);
  state_ = STATE_DEINIT;
  SignalState(this, state_);

  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    delete iter->second;
  }

  delete remote_description_;
  delete local_description_;
}

bool BaseSession::PushdownTransportDescription(ContentSource source,
                                               ContentAction action) {
  if (source == CS_LOCAL) {
    return PushdownLocalTransportDescription(local_description_, action);
  }
  return PushdownRemoteTransportDescription(remote_description_, action);
}

bool BaseSession::PushdownLocalTransportDescription(
    const SessionDescription* sdesc,
    ContentAction action) {
  // Update the Transports with the right information, and trigger them to
  // start connecting.
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    // If no transport info was in this session description, ret == false
    // and we just skip this one.
    TransportDescription tdesc;
    bool ret = GetTransportDescription(
        sdesc, iter->second->content_name(), &tdesc);
    if (ret) {
      if (!iter->second->SetLocalTransportDescription(tdesc, action)) {
        return false;
      }

      iter->second->ConnectChannels();
    }
  }

  return true;
}

bool BaseSession::PushdownRemoteTransportDescription(
    const SessionDescription* sdesc,
    ContentAction action) {
  // Update the Transports with the right information.
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    TransportDescription tdesc;

    // If no transport info was in this session description, ret == false
    // and we just skip this one.
    bool ret = GetTransportDescription(
        sdesc, iter->second->content_name(), &tdesc);
    if (ret) {
      if (!iter->second->SetRemoteTransportDescription(tdesc, action)) {
        return false;
      }
    }
  }

  return true;
}

TransportChannel* BaseSession::CreateChannel(const std::string& content_name,
                                             const std::string& channel_name,
                                             int component) {
  // We create the proxy "on demand" here because we need to support
  // creating channels at any time, even before we send or receive
  // initiate messages, which is before we create the transports.
  TransportProxy* transproxy = GetOrCreateTransportProxy(content_name);
  return transproxy->CreateChannel(channel_name, component);
}

TransportChannel* BaseSession::GetChannel(const std::string& content_name,
                                          int component) {
  TransportProxy* transproxy = GetTransportProxy(content_name);
  if (transproxy == NULL)
    return NULL;
  else
    return transproxy->GetChannel(component);
}

void BaseSession::DestroyChannel(const std::string& content_name,
                                 int component) {
  TransportProxy* transproxy = GetTransportProxy(content_name);
  ASSERT(transproxy != NULL);
  transproxy->DestroyChannel(component);
}

TransportProxy* BaseSession::GetOrCreateTransportProxy(
    const std::string& content_name) {
  TransportProxy* transproxy = GetTransportProxy(content_name);
  if (transproxy)
    return transproxy;

  Transport* transport = CreateTransport(content_name);
  transport->SetRole(initiator_ ? ROLE_CONTROLLING : ROLE_CONTROLLED);
  transport->SetTiebreaker(ice_tiebreaker_);
  // TODO: Connect all the Transport signals to TransportProxy
  // then to the BaseSession.
  transport->SignalConnecting.connect(
      this, &BaseSession::OnTransportConnecting);
  transport->SignalWritableState.connect(
      this, &BaseSession::OnTransportWritable);
  transport->SignalRequestSignaling.connect(
      this, &BaseSession::OnTransportRequestSignaling);
  transport->SignalTransportError.connect(
      this, &BaseSession::OnTransportSendError);
  transport->SignalRouteChange.connect(
      this, &BaseSession::OnTransportRouteChange);
  transport->SignalCandidatesAllocationDone.connect(
      this, &BaseSession::OnTransportCandidatesAllocationDone);
  transport->SignalRoleConflict.connect(
      this, &BaseSession::OnRoleConflict);

  transproxy = new TransportProxy(sid_, content_name,
                                  new TransportWrapper(transport));
  transproxy->SignalCandidatesReady.connect(
      this, &BaseSession::OnTransportProxyCandidatesReady);
  transports_[content_name] = transproxy;

  return transproxy;
}

Transport* BaseSession::GetTransport(const std::string& content_name) {
  TransportProxy* transproxy = GetTransportProxy(content_name);
  if (transproxy == NULL)
    return NULL;
  return transproxy->impl();
}

TransportProxy* BaseSession::GetTransportProxy(
    const std::string& content_name) {
  TransportMap::iterator iter = transports_.find(content_name);
  return (iter != transports_.end()) ? iter->second : NULL;
}

TransportProxy* BaseSession::GetTransportProxy(const Transport* transport) {
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    TransportProxy* transproxy = iter->second;
    if (transproxy->impl() == transport) {
      return transproxy;
    }
  }
  return NULL;
}

TransportProxy* BaseSession::GetFirstTransportProxy() {
  if (transports_.empty())
    return NULL;
  return transports_.begin()->second;
}

void BaseSession::DestroyTransportProxy(
    const std::string& content_name) {
  TransportMap::iterator iter = transports_.find(content_name);
  if (iter != transports_.end()) {
    delete iter->second;
    transports_.erase(content_name);
  }
}

cricket::Transport* BaseSession::CreateTransport(
    const std::string& content_name) {
  ASSERT(transport_type_ == NS_GINGLE_P2P);
  return new cricket::DtlsTransport<P2PTransport>(
      signaling_thread(), worker_thread(), content_name,
      port_allocator(), identity_);
}

bool BaseSession::GetStats(SessionStats* stats) {
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    std::string proxy_id = iter->second->content_name();
    // We are ignoring not-yet-instantiated transports.
    if (iter->second->impl()) {
      std::string transport_id = iter->second->impl()->content_name();
      stats->proxy_to_transport[proxy_id] = transport_id;
      if (stats->transport_stats.find(transport_id)
          == stats->transport_stats.end()) {
        TransportStats subinfos;
        if (!iter->second->impl()->GetStats(&subinfos)) {
          return false;
        }
        stats->transport_stats[transport_id] = subinfos;
      }
    }
  }
  return true;
}

void BaseSession::SetState(State state) {
  ASSERT(signaling_thread_->IsCurrent());
  if (state != state_) {
    LogState(state_, state);
    state_ = state;
    SignalState(this, state_);
    signaling_thread_->Post(this, MSG_STATE);
  }
  SignalNewDescription();
}

void BaseSession::SetError(Error error) {
  ASSERT(signaling_thread_->IsCurrent());
  if (error != error_) {
    error_ = error;
    SignalError(this, error);
  }
}

void BaseSession::OnSignalingReady() {
  ASSERT(signaling_thread()->IsCurrent());
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    iter->second->OnSignalingReady();
  }
}

// TODO(juberti): Since PushdownLocalTD now triggers the connection process to
// start, remove this method once everyone calls PushdownLocalTD.
void BaseSession::SpeculativelyConnectAllTransportChannels() {
  // Put all transports into the connecting state.
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    iter->second->ConnectChannels();
  }
}

bool BaseSession::OnRemoteCandidates(const std::string& content_name,
                                     const Candidates& candidates,
                                     std::string* error) {
  // Give candidates to the appropriate transport, and tell that transport
  // to start connecting, if it's not already doing so.
  TransportProxy* transproxy = GetTransportProxy(content_name);
  if (!transproxy) {
    *error = "Unknown content name " + content_name;
    return false;
  }
  if (!transproxy->OnRemoteCandidates(candidates, error)) {
    return false;
  }
  // TODO(juberti): Remove this call once we can be sure that we always have
  // a local transport description (which will trigger the connection).
  transproxy->ConnectChannels();
  return true;
}

bool BaseSession::MaybeEnableMuxingSupport() {
  // We need both a local and remote description to decide if we should mux.
  if ((state_ == STATE_SENTINITIATE ||
      state_ == STATE_RECEIVEDINITIATE) &&
      ((local_description_ == NULL) ||
      (remote_description_ == NULL))) {
    return false;
  }

  // In order to perform the multiplexing, we need all proxies to be in the
  // negotiated state, i.e. to have implementations underneath.
  // Ensure that this is the case, regardless of whether we are going to mux.
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    ASSERT(iter->second->negotiated());
    if (!iter->second->negotiated())
      return false;
  }

  // If both sides agree to BUNDLE, mux all the specified contents onto the
  // transport belonging to the first content name in the BUNDLE group.
  // If the contents are already muxed, this will be a no-op.
  // TODO(juberti): Should this check that local and remote have configured
  // BUNDLE the same way?
  bool candidates_allocated = IsCandidateAllocationDone();
  const ContentGroup* local_bundle_group =
      local_description()->GetGroupByName(GROUP_TYPE_BUNDLE);
  const ContentGroup* remote_bundle_group =
      remote_description()->GetGroupByName(GROUP_TYPE_BUNDLE);
  if (local_bundle_group && remote_bundle_group &&
      local_bundle_group->FirstContentName()) {
    const std::string* content_name = local_bundle_group->FirstContentName();
    const ContentInfo* content =
        local_description_->GetContentByName(*content_name);
    ASSERT(content != NULL);
    if (!SetSelectedProxy(content->name, local_bundle_group)) {
      LOG(LS_WARNING) << "Failed to set up BUNDLE";
      return false;
    }

    // If we weren't done gathering before, we might be done now, as a result
    // of enabling mux.
    LOG(LS_INFO) << "Enabling BUNDLE, bundling onto transport: "
                 << *content_name;
    if (!candidates_allocated) {
      MaybeCandidateAllocationDone();
    }
  } else {
    LOG(LS_INFO) << "No BUNDLE information, not bundling.";
  }
  return true;
}

bool BaseSession::SetSelectedProxy(const std::string& content_name,
                                   const ContentGroup* muxed_group) {
  TransportProxy* selected_proxy = GetTransportProxy(content_name);
  if (!selected_proxy) {
    return false;
  }

  ASSERT(selected_proxy->negotiated());
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    // If content is part of the mux group, then repoint its proxy at the
    // transport object that we have chosen to mux onto. If the proxy
    // is already pointing at the right object, it will be a no-op.
    if (muxed_group->HasContentName(iter->first) &&
        !iter->second->SetupMux(selected_proxy)) {
      return false;
    }
  }
  return true;
}

void BaseSession::OnTransportCandidatesAllocationDone(Transport* transport) {
  // TODO(juberti): This is a clunky way of processing the done signal. Instead,
  // TransportProxy should receive the done signal directly, set its allocated
  // flag internally, and then reissue the done signal to Session.
  // Overall we should make TransportProxy receive *all* the signals from
  // Transport, since this removes the need to manually iterate over all
  // the transports, as is needed to make sure signals are handled properly
  // when BUNDLEing.
#if 0
  ASSERT(!IsCandidateAllocationDone());
#endif
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    if (iter->second->impl() == transport) {
      iter->second->set_candidates_allocated(true);
    }
  }
  MaybeCandidateAllocationDone();
}

bool BaseSession::IsCandidateAllocationDone() const {
  for (TransportMap::const_iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    if (!iter->second->candidates_allocated())
      return false;
  }
  return true;
}

void BaseSession::MaybeCandidateAllocationDone() {
  if (IsCandidateAllocationDone()) {
    LOG(LS_INFO) << "Candidate gathering is complete.";
    OnCandidatesAllocationDone();
  }
}

void BaseSession::OnRoleConflict() {
  if (role_switch_) {
    LOG(LS_WARNING) << "Repeat of role conflict signal from Transport.";
    return;
  }

  role_switch_ = true;
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    // Role will be reverse of initial role setting.
    TransportRole role = initiator_ ? ROLE_CONTROLLED : ROLE_CONTROLLING;
    iter->second->SetRole(role);
  }
}

void BaseSession::LogState(State old_state, State new_state) {
  LOG(LS_INFO) << "Session:" << id()
               << " Old state:" << StateToString(old_state)
               << " New state:" << StateToString(new_state)
               << " Type:" << content_type()
               << " Transport:" << transport_type();
}

bool BaseSession::GetTransportDescription(const SessionDescription* description,
                                          const std::string& content_name,
                                          TransportDescription* tdesc) {
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

void BaseSession::SignalNewDescription() {
  ContentAction action;
  ContentSource source;
  if (!GetContentAction(&action, &source)) {
    return;
  }
  if (source == CS_LOCAL) {
    SignalNewLocalDescription(this, action);
  } else {
    SignalNewRemoteDescription(this, action);
  }
}

bool BaseSession::GetContentAction(ContentAction* action,
                                   ContentSource* source) {
  switch (state_) {
    // new local description
    case STATE_SENTINITIATE:
      *action = CA_OFFER;
      *source = CS_LOCAL;
      break;
    case STATE_SENTPRACCEPT:
      *action = CA_PRANSWER;
      *source = CS_LOCAL;
      break;
    case STATE_SENTACCEPT:
      *action = CA_ANSWER;
      *source = CS_LOCAL;
      break;
    // new remote description
    case STATE_RECEIVEDINITIATE:
      *action = CA_OFFER;
      *source = CS_REMOTE;
      break;
    case STATE_RECEIVEDPRACCEPT:
      *action = CA_PRANSWER;
      *source = CS_REMOTE;
      break;
    case STATE_RECEIVEDACCEPT:
      *action = CA_ANSWER;
      *source = CS_REMOTE;
      break;
    default:
      return false;
  }
  return true;
}

void BaseSession::OnMessage(talk_base::Message *pmsg) {
  switch (pmsg->message_id) {
  case MSG_TIMEOUT:
    // Session timeout has occured.
    SetError(ERROR_TIME);
    break;

  case MSG_STATE:
    switch (state_) {
    case STATE_SENTACCEPT:
    case STATE_RECEIVEDACCEPT:
      SetState(STATE_INPROGRESS);
      break;

    default:
      // Explicitly ignoring some states here.
      break;
    }
    break;
  }
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

bool Session::Initiate(const std::string &to,
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

  PushdownTransportDescription(CS_LOCAL, CA_OFFER);
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
  PushdownTransportDescription(CS_LOCAL, CA_ANSWER);
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

bool Session::SendInfoMessage(const XmlElements& elems) {
  ASSERT(signaling_thread()->IsCurrent());
  SessionError error;
  if (!SendMessage(ACTION_SESSION_INFO, elems, &error)) {
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
    tinfos.push_back(
        TransportInfo(content->name,
            TransportDescription(transport_type(), Candidates())));
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

void Session::OnTransportSendError(Transport* transport,
                                   const buzz::XmlElement* stanza,
                                   const buzz::QName& name,
                                   const std::string& type,
                                   const std::string& text,
                                   const buzz::XmlElement* extra_info) {
  ASSERT(signaling_thread()->IsCurrent());
  SignalErrorMessage(this, stanza, name, type, text, extra_info);
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
      LOG(LS_ERROR) << "Failed to redirect: " << error.text;
      SetError(ERROR_RESPONSE);
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
    SetError(ERROR_RESPONSE);
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
  PushdownTransportDescription(CS_REMOTE, CA_OFFER);
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
  PushdownTransportDescription(CS_REMOTE, CA_ANSWER);
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

void Session::SetError(Error error) {
  BaseSession::SetError(error);
  if (error != ERROR_NONE)
    signaling_thread()->Post(this, MSG_ERROR);
}

void Session::OnMessage(talk_base::Message* pmsg) {
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
      TransportDescription(transproxy->type(), candidates)), error);
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
  talk_base::scoped_ptr<buzz::XmlElement> stanza(
      new buzz::XmlElement(buzz::QN_IQ));

  SessionMessage msg(current_protocol_, type, id(), initiator_name());
  msg.to = remote_name();
  WriteSessionMessage(msg, action_elems, stanza.get());

  SignalOutgoingMessage(this, stanza.get());
  return true;
}

template <typename Action>
bool Session::SendMessage(ActionType type, const Action& action,
                          SessionError* error) {
  talk_base::scoped_ptr<buzz::XmlElement> stanza(
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
  talk_base::scoped_ptr<buzz::XmlElement> ack(
      new buzz::XmlElement(buzz::QN_IQ));
  ack->SetAttr(buzz::QN_TO, remote_name());
  ack->SetAttr(buzz::QN_ID, stanza->Attr(buzz::QN_ID));
  ack->SetAttr(buzz::QN_TYPE, "result");

  SignalOutgoingMessage(this, ack.get());
}

}  // namespace cricket
