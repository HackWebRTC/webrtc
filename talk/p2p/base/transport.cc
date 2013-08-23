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

#include "talk/p2p/base/transport.h"

#include "talk/base/bind.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/p2p/base/candidate.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/sessionmanager.h"
#include "talk/p2p/base/parsing.h"
#include "talk/p2p/base/transportchannelimpl.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/constants.h"

namespace cricket {

enum {
  MSG_CREATECHANNEL = 1,
  MSG_DESTROYCHANNEL = 2,
  MSG_DESTROYALLCHANNELS = 3,
  MSG_CONNECTCHANNELS = 4,
  MSG_RESETCHANNELS = 5,
  MSG_ONSIGNALINGREADY = 6,
  MSG_ONREMOTECANDIDATE = 7,
  MSG_READSTATE = 8,
  MSG_WRITESTATE = 9,
  MSG_REQUESTSIGNALING = 10,
  MSG_CANDIDATEREADY = 11,
  MSG_ROUTECHANGE = 12,
  MSG_CONNECTING = 13,
  MSG_CANDIDATEALLOCATIONCOMPLETE = 14,
  MSG_ROLECONFLICT = 15,
  MSG_SETICEROLE = 16,
  MSG_SETLOCALDESCRIPTION = 17,
  MSG_SETREMOTEDESCRIPTION = 18,
  MSG_GETSTATS = 19,
  MSG_SETIDENTITY = 20,
};

struct ChannelParams : public talk_base::MessageData {
  ChannelParams() : channel(NULL), candidate(NULL) {}
  explicit ChannelParams(int component)
      : component(component), channel(NULL), candidate(NULL) {}
  explicit ChannelParams(Candidate* candidate)
      : channel(NULL), candidate(candidate) {
  }

  ~ChannelParams() {
    delete candidate;
  }

  std::string name;
  int component;
  TransportChannelImpl* channel;
  Candidate* candidate;
};

struct TransportDescriptionParams : public talk_base::MessageData {
  TransportDescriptionParams(const TransportDescription& desc,
                             ContentAction action)
      : desc(desc), action(action), result(false) {}
  const TransportDescription& desc;
  ContentAction action;
  bool result;
};

struct IceRoleParam : public talk_base::MessageData {
  explicit IceRoleParam(IceRole role) : role(role) {}

  IceRole role;
};

struct StatsParam : public talk_base::MessageData {
  explicit StatsParam(TransportStats* stats)
      : stats(stats), result(false) {}

  TransportStats* stats;
  bool result;
};

struct IdentityParam : public talk_base::MessageData {
  explicit IdentityParam(talk_base::SSLIdentity* identity)
      : identity(identity) {}

  talk_base::SSLIdentity* identity;
};

Transport::Transport(talk_base::Thread* signaling_thread,
                     talk_base::Thread* worker_thread,
                     const std::string& content_name,
                     const std::string& type,
                     PortAllocator* allocator)
  : signaling_thread_(signaling_thread),
    worker_thread_(worker_thread),
    content_name_(content_name),
    type_(type),
    allocator_(allocator),
    destroyed_(false),
    readable_(TRANSPORT_STATE_NONE),
    writable_(TRANSPORT_STATE_NONE),
    was_writable_(false),
    connect_requested_(false),
    ice_role_(ICEROLE_UNKNOWN),
    tiebreaker_(0),
    protocol_(ICEPROTO_HYBRID),
    remote_ice_mode_(ICEMODE_FULL) {
}

Transport::~Transport() {
  ASSERT(signaling_thread_->IsCurrent());
  ASSERT(destroyed_);
}

void Transport::SetIceRole(IceRole role) {
  IceRoleParam param(role);
  worker_thread()->Send(this, MSG_SETICEROLE, &param);
}

void Transport::SetIdentity(talk_base::SSLIdentity* identity) {
  IdentityParam params(identity);
  worker_thread()->Send(this, MSG_SETIDENTITY, &params);
}

bool Transport::SetLocalTransportDescription(
    const TransportDescription& description, ContentAction action) {
  TransportDescriptionParams params(description, action);
  worker_thread()->Send(this, MSG_SETLOCALDESCRIPTION, &params);
  return params.result;
}

bool Transport::SetRemoteTransportDescription(
    const TransportDescription& description, ContentAction action) {
  TransportDescriptionParams params(description, action);
  worker_thread()->Send(this, MSG_SETREMOTEDESCRIPTION, &params);
  return params.result;
}

TransportChannelImpl* Transport::CreateChannel(int component) {
  ChannelParams params(component);
  worker_thread()->Send(this, MSG_CREATECHANNEL, &params);
  return params.channel;
}

TransportChannelImpl* Transport::CreateChannel_w(int component) {
  ASSERT(worker_thread()->IsCurrent());
  TransportChannelImpl *impl;
  talk_base::CritScope cs(&crit_);

  // Create the entry if it does not exist.
  bool impl_exists = false;
  if (channels_.find(component) == channels_.end()) {
    impl = CreateTransportChannel(component);
    channels_[component] = ChannelMapEntry(impl);
  } else {
    impl = channels_[component].get();
    impl_exists = true;
  }

  // Increase the ref count.
  channels_[component].AddRef();
  destroyed_ = false;

  if (impl_exists) {
    // If this is an existing channel, we should just return it without
    // connecting to all the signal again.
    return impl;
  }

  // Push down our transport state to the new channel.
  impl->SetIceRole(ice_role_);
  impl->SetIceTiebreaker(tiebreaker_);
  if (local_description_) {
    ApplyLocalTransportDescription_w(impl);
    if (remote_description_) {
      ApplyRemoteTransportDescription_w(impl);
      ApplyNegotiatedTransportDescription_w(impl);
    }
  }

  impl->SignalReadableState.connect(this, &Transport::OnChannelReadableState);
  impl->SignalWritableState.connect(this, &Transport::OnChannelWritableState);
  impl->SignalRequestSignaling.connect(
      this, &Transport::OnChannelRequestSignaling);
  impl->SignalCandidateReady.connect(this, &Transport::OnChannelCandidateReady);
  impl->SignalRouteChange.connect(this, &Transport::OnChannelRouteChange);
  impl->SignalCandidatesAllocationDone.connect(
      this, &Transport::OnChannelCandidatesAllocationDone);
  impl->SignalRoleConflict.connect(this, &Transport::OnRoleConflict);

  if (connect_requested_) {
    impl->Connect();
    if (channels_.size() == 1) {
      // If this is the first channel, then indicate that we have started
      // connecting.
      signaling_thread()->Post(this, MSG_CONNECTING, NULL);
    }
  }
  return impl;
}

TransportChannelImpl* Transport::GetChannel(int component) {
  talk_base::CritScope cs(&crit_);
  ChannelMap::iterator iter = channels_.find(component);
  return (iter != channels_.end()) ? iter->second.get() : NULL;
}

bool Transport::HasChannels() {
  talk_base::CritScope cs(&crit_);
  return !channels_.empty();
}

void Transport::DestroyChannel(int component) {
  ChannelParams params(component);
  worker_thread()->Send(this, MSG_DESTROYCHANNEL, &params);
}

void Transport::DestroyChannel_w(int component) {
  ASSERT(worker_thread()->IsCurrent());

  TransportChannelImpl* impl = NULL;
  {
    talk_base::CritScope cs(&crit_);
    ChannelMap::iterator iter = channels_.find(component);
    if (iter == channels_.end())
      return;

    iter->second.DecRef();
    if (!iter->second.ref()) {
      impl = iter->second.get();
      channels_.erase(iter);
    }
  }

  if (connect_requested_ && channels_.empty()) {
    // We're no longer attempting to connect.
    signaling_thread()->Post(this, MSG_CONNECTING, NULL);
  }

  if (impl) {
    // Check in case the deleted channel was the only non-writable channel.
    OnChannelWritableState(impl);
    DestroyTransportChannel(impl);
  }
}

void Transport::ConnectChannels() {
  ASSERT(signaling_thread()->IsCurrent());
  worker_thread()->Send(this, MSG_CONNECTCHANNELS, NULL);
}

void Transport::ConnectChannels_w() {
  ASSERT(worker_thread()->IsCurrent());
  if (connect_requested_ || channels_.empty())
    return;
  connect_requested_ = true;
  signaling_thread()->Post(
      this, MSG_CANDIDATEREADY, NULL);

  if (!local_description_) {
    // TOOD(mallinath) : TransportDescription(TD) shouldn't be generated here.
    // As Transport must know TD is offer or answer and cricket::Transport
    // doesn't have the capability to decide it. This should be set by the
    // Session.
    // Session must generate local TD before remote candidates pushed when
    // initiate request initiated by the remote.
    LOG(LS_INFO) << "Transport::ConnectChannels_w: No local description has "
                 << "been set. Will generate one.";
    TransportDescription desc(NS_GINGLE_P2P, std::vector<std::string>(),
                              talk_base::CreateRandomString(ICE_UFRAG_LENGTH),
                              talk_base::CreateRandomString(ICE_PWD_LENGTH),
                              ICEMODE_FULL, CONNECTIONROLE_NONE, NULL,
                              Candidates());
    SetLocalTransportDescription_w(desc, CA_OFFER);
  }

  CallChannels_w(&TransportChannelImpl::Connect);
  if (!channels_.empty()) {
    signaling_thread()->Post(this, MSG_CONNECTING, NULL);
  }
}

void Transport::OnConnecting_s() {
  ASSERT(signaling_thread()->IsCurrent());
  SignalConnecting(this);
}

void Transport::DestroyAllChannels() {
  ASSERT(signaling_thread()->IsCurrent());
  worker_thread()->Send(this, MSG_DESTROYALLCHANNELS, NULL);
  worker_thread()->Clear(this);
  signaling_thread()->Clear(this);
  destroyed_ = true;
}

void Transport::DestroyAllChannels_w() {
  ASSERT(worker_thread()->IsCurrent());
  std::vector<TransportChannelImpl*> impls;
  {
    talk_base::CritScope cs(&crit_);
    for (ChannelMap::iterator iter = channels_.begin();
         iter != channels_.end();
         ++iter) {
      iter->second.DecRef();
      if (!iter->second.ref())
        impls.push_back(iter->second.get());
      }
    }
  channels_.clear();


  for (size_t i = 0; i < impls.size(); ++i)
    DestroyTransportChannel(impls[i]);
}

void Transport::ResetChannels() {
  ASSERT(signaling_thread()->IsCurrent());
  worker_thread()->Send(this, MSG_RESETCHANNELS, NULL);
}

void Transport::ResetChannels_w() {
  ASSERT(worker_thread()->IsCurrent());

  // We are no longer attempting to connect
  connect_requested_ = false;

  // Clear out the old messages, they aren't relevant
  talk_base::CritScope cs(&crit_);
  ready_candidates_.clear();

  // Reset all of the channels
  CallChannels_w(&TransportChannelImpl::Reset);
}

void Transport::OnSignalingReady() {
  ASSERT(signaling_thread()->IsCurrent());
  if (destroyed_) return;

  worker_thread()->Post(this, MSG_ONSIGNALINGREADY, NULL);

  // Notify the subclass.
  OnTransportSignalingReady();
}

void Transport::CallChannels_w(TransportChannelFunc func) {
  ASSERT(worker_thread()->IsCurrent());
  talk_base::CritScope cs(&crit_);
  for (ChannelMap::iterator iter = channels_.begin();
       iter != channels_.end();
       ++iter) {
    ((iter->second.get())->*func)();
  }
}

bool Transport::VerifyCandidate(const Candidate& cand, std::string* error) {
  // No address zero.
  if (cand.address().IsNil() || cand.address().IsAny()) {
    *error = "candidate has address of zero";
    return false;
  }

  // Disallow all ports below 1024, except for 80 and 443 on public addresses.
  int port = cand.address().port();
  if (port < 1024) {
    if ((port != 80) && (port != 443)) {
      *error = "candidate has port below 1024, but not 80 or 443";
      return false;
    }

    if (cand.address().IsPrivateIP()) {
      *error = "candidate has port of 80 or 443 with private IP address";
      return false;
    }
  }

  return true;
}


bool Transport::GetStats(TransportStats* stats) {
  ASSERT(signaling_thread()->IsCurrent());
  StatsParam params(stats);
  worker_thread()->Send(this, MSG_GETSTATS, &params);
  return params.result;
}

bool Transport::GetStats_w(TransportStats* stats) {
  ASSERT(worker_thread()->IsCurrent());
  stats->content_name = content_name();
  stats->channel_stats.clear();
  for (ChannelMap::iterator iter = channels_.begin();
       iter != channels_.end();
       ++iter) {
    TransportChannelStats substats;
    substats.component = iter->second->component();
    if (!iter->second->GetStats(&substats.connection_infos)) {
      return false;
    }
    stats->channel_stats.push_back(substats);
  }
  return true;
}

bool Transport::GetSslRole(talk_base::SSLRole* ssl_role) const {
  return worker_thread_->Invoke<bool>(
      Bind(&Transport::GetSslRole_w, this, ssl_role));
}

void Transport::OnRemoteCandidates(const std::vector<Candidate>& candidates) {
  for (std::vector<Candidate>::const_iterator iter = candidates.begin();
       iter != candidates.end();
       ++iter) {
    OnRemoteCandidate(*iter);
  }
}

void Transport::OnRemoteCandidate(const Candidate& candidate) {
  ASSERT(signaling_thread()->IsCurrent());
  if (destroyed_) return;

  if (!HasChannel(candidate.component())) {
    LOG(LS_WARNING) << "Ignoring candidate for unknown component "
                    << candidate.component();
    return;
  }

  ChannelParams* params = new ChannelParams(new Candidate(candidate));
  worker_thread()->Post(this, MSG_ONREMOTECANDIDATE, params);
}

void Transport::OnRemoteCandidate_w(const Candidate& candidate) {
  ASSERT(worker_thread()->IsCurrent());
  ChannelMap::iterator iter = channels_.find(candidate.component());
  // It's ok for a channel to go away while this message is in transit.
  if (iter != channels_.end()) {
    iter->second->OnCandidate(candidate);
  }
}

void Transport::OnChannelReadableState(TransportChannel* channel) {
  ASSERT(worker_thread()->IsCurrent());
  signaling_thread()->Post(this, MSG_READSTATE, NULL);
}

void Transport::OnChannelReadableState_s() {
  ASSERT(signaling_thread()->IsCurrent());
  TransportState readable = GetTransportState_s(true);
  if (readable_ != readable) {
    readable_ = readable;
    SignalReadableState(this);
  }
}

void Transport::OnChannelWritableState(TransportChannel* channel) {
  ASSERT(worker_thread()->IsCurrent());
  signaling_thread()->Post(this, MSG_WRITESTATE, NULL);
}

void Transport::OnChannelWritableState_s() {
  ASSERT(signaling_thread()->IsCurrent());
  TransportState writable = GetTransportState_s(false);
  if (writable_ != writable) {
    was_writable_ = (writable_ == TRANSPORT_STATE_ALL);
    writable_ = writable;
    SignalWritableState(this);
  }
}

TransportState Transport::GetTransportState_s(bool read) {
  ASSERT(signaling_thread()->IsCurrent());
  talk_base::CritScope cs(&crit_);
  bool any = false;
  bool all = !channels_.empty();
  for (ChannelMap::iterator iter = channels_.begin();
       iter != channels_.end();
       ++iter) {
    bool b = (read ? iter->second->readable() :
      iter->second->writable());
    any = any || b;
    all = all && b;
  }
  if (all) {
    return TRANSPORT_STATE_ALL;
  } else if (any) {
    return TRANSPORT_STATE_SOME;
  } else {
    return TRANSPORT_STATE_NONE;
  }
}

void Transport::OnChannelRequestSignaling(TransportChannelImpl* channel) {
  ASSERT(worker_thread()->IsCurrent());
  ChannelParams* params = new ChannelParams(channel->component());
  signaling_thread()->Post(this, MSG_REQUESTSIGNALING, params);
}

void Transport::OnChannelRequestSignaling_s(int component) {
  ASSERT(signaling_thread()->IsCurrent());
  LOG(LS_INFO) << "Transport: " << content_name_ << ", allocating candidates";
  // Resetting ICE state for the channel.
  {
    talk_base::CritScope cs(&crit_);
    ChannelMap::iterator iter = channels_.find(component);
    if (iter != channels_.end())
      iter->second.set_candidates_allocated(false);
  }
  SignalRequestSignaling(this);
}

void Transport::OnChannelCandidateReady(TransportChannelImpl* channel,
                                        const Candidate& candidate) {
  ASSERT(worker_thread()->IsCurrent());
  talk_base::CritScope cs(&crit_);
  ready_candidates_.push_back(candidate);

  // We hold any messages until the client lets us connect.
  if (connect_requested_) {
    signaling_thread()->Post(
        this, MSG_CANDIDATEREADY, NULL);
  }
}

void Transport::OnChannelCandidateReady_s() {
  ASSERT(signaling_thread()->IsCurrent());
  ASSERT(connect_requested_);

  std::vector<Candidate> candidates;
  {
    talk_base::CritScope cs(&crit_);
    candidates.swap(ready_candidates_);
  }

  // we do the deleting of Candidate* here to keep the new above and
  // delete below close to each other
  if (!candidates.empty()) {
    SignalCandidatesReady(this, candidates);
  }
}

void Transport::OnChannelRouteChange(TransportChannel* channel,
                                     const Candidate& remote_candidate) {
  ASSERT(worker_thread()->IsCurrent());
  ChannelParams* params = new ChannelParams(new Candidate(remote_candidate));
  params->channel = static_cast<cricket::TransportChannelImpl*>(channel);
  signaling_thread()->Post(this, MSG_ROUTECHANGE, params);
}

void Transport::OnChannelRouteChange_s(const TransportChannel* channel,
                                       const Candidate& remote_candidate) {
  ASSERT(signaling_thread()->IsCurrent());
  SignalRouteChange(this, remote_candidate.component(), remote_candidate);
}

void Transport::OnChannelCandidatesAllocationDone(
    TransportChannelImpl* channel) {
  ASSERT(worker_thread()->IsCurrent());
  talk_base::CritScope cs(&crit_);
  ChannelMap::iterator iter = channels_.find(channel->component());
  ASSERT(iter != channels_.end());
  LOG(LS_INFO) << "Transport: " << content_name_ << ", component "
               << channel->component() << " allocation complete";
  iter->second.set_candidates_allocated(true);

  // If all channels belonging to this Transport got signal, then
  // forward this signal to upper layer.
  // Can this signal arrive before all transport channels are created?
  for (iter = channels_.begin(); iter != channels_.end(); ++iter) {
    if (!iter->second.candidates_allocated())
      return;
  }
  signaling_thread_->Post(this, MSG_CANDIDATEALLOCATIONCOMPLETE);
}

void Transport::OnChannelCandidatesAllocationDone_s() {
  ASSERT(signaling_thread()->IsCurrent());
  LOG(LS_INFO) << "Transport: " << content_name_ << " allocation complete";
  SignalCandidatesAllocationDone(this);
}

void Transport::OnRoleConflict(TransportChannelImpl* channel) {
  signaling_thread_->Post(this, MSG_ROLECONFLICT);
}

void Transport::SetIceRole_w(IceRole role) {
  talk_base::CritScope cs(&crit_);
  ice_role_ = role;
  for (ChannelMap::iterator iter = channels_.begin();
       iter != channels_.end(); ++iter) {
    iter->second->SetIceRole(ice_role_);
  }
}

void Transport::SetRemoteIceMode_w(IceMode mode) {
  talk_base::CritScope cs(&crit_);
  remote_ice_mode_ = mode;
  // Shouldn't channels be created after this method executed?
  for (ChannelMap::iterator iter = channels_.begin();
       iter != channels_.end(); ++iter) {
    iter->second->SetRemoteIceMode(remote_ice_mode_);
  }
}

bool Transport::SetLocalTransportDescription_w(
    const TransportDescription& desc, ContentAction action) {
  bool ret = true;
  talk_base::CritScope cs(&crit_);
  local_description_.reset(new TransportDescription(desc));

  for (ChannelMap::iterator iter = channels_.begin();
       iter != channels_.end(); ++iter) {
    ret &= ApplyLocalTransportDescription_w(iter->second.get());
  }
  if (!ret)
    return false;

  // If PRANSWER/ANSWER is set, we should decide transport protocol type.
  if (action == CA_PRANSWER || action == CA_ANSWER) {
    ret &= NegotiateTransportDescription_w(action);
  }
  return ret;
}

bool Transport::SetRemoteTransportDescription_w(
    const TransportDescription& desc, ContentAction action) {
  bool ret = true;
  talk_base::CritScope cs(&crit_);
  remote_description_.reset(new TransportDescription(desc));

  for (ChannelMap::iterator iter = channels_.begin();
       iter != channels_.end(); ++iter) {
    ret &= ApplyRemoteTransportDescription_w(iter->second.get());
  }

  // If PRANSWER/ANSWER is set, we should decide transport protocol type.
  if (action == CA_PRANSWER || action == CA_ANSWER) {
    ret = NegotiateTransportDescription_w(CA_OFFER);
  }
  return ret;
}

bool Transport::ApplyLocalTransportDescription_w(TransportChannelImpl* ch) {
  ch->SetIceCredentials(local_description_->ice_ufrag,
                        local_description_->ice_pwd);
  return true;
}

bool Transport::ApplyRemoteTransportDescription_w(TransportChannelImpl* ch) {
  ch->SetRemoteIceCredentials(remote_description_->ice_ufrag,
                              remote_description_->ice_pwd);
  return true;
}

bool Transport::ApplyNegotiatedTransportDescription_w(
    TransportChannelImpl* channel) {
  channel->SetIceProtocolType(protocol_);
  channel->SetRemoteIceMode(remote_ice_mode_);
  return true;
}

bool Transport::NegotiateTransportDescription_w(ContentAction local_role) {
  // TODO(ekr@rtfm.com): This is ICE-specific stuff. Refactor into
  // P2PTransport.
  const TransportDescription* offer;
  const TransportDescription* answer;

  if (local_role == CA_OFFER) {
    offer = local_description_.get();
    answer = remote_description_.get();
  } else {
    offer = remote_description_.get();
    answer = local_description_.get();
  }

  TransportProtocol offer_proto = TransportProtocolFromDescription(offer);
  TransportProtocol answer_proto = TransportProtocolFromDescription(answer);

  // If offered protocol is gice/ice, then we expect to receive matching
  // protocol in answer, anything else is treated as an error.
  // HYBRID is not an option when offered specific protocol.
  // If offered protocol is HYBRID and answered protocol is HYBRID then
  // gice is preferred protocol.
  // TODO(mallinath) - Answer from local or remote should't have both ice
  // and gice support. It should always pick which protocol it wants to use.
  // Once WebRTC stops supporting gice (for backward compatibility), HYBRID in
  // answer must be treated as error.
  if ((offer_proto == ICEPROTO_GOOGLE || offer_proto == ICEPROTO_RFC5245) &&
      (offer_proto != answer_proto)) {
    return false;
  }
  protocol_ = answer_proto == ICEPROTO_HYBRID ? ICEPROTO_GOOGLE : answer_proto;

  // If transport is in ICEROLE_CONTROLLED and remote end point supports only
  // ice_lite, this local end point should take CONTROLLING role.
  if (ice_role_ == ICEROLE_CONTROLLED &&
      remote_description_->ice_mode == ICEMODE_LITE) {
    SetIceRole_w(ICEROLE_CONTROLLING);
  }

  // Update remote ice_mode to all existing channels.
  remote_ice_mode_ = remote_description_->ice_mode;

  // Now that we have negotiated everything, push it downward.
  // Note that we cache the result so that if we have race conditions
  // between future SetRemote/SetLocal invocations and new channel
  // creation, we have the negotiation state saved until a new
  // negotiation happens.
  for (ChannelMap::iterator iter = channels_.begin();
       iter != channels_.end();
       ++iter) {
    if (!ApplyNegotiatedTransportDescription_w(iter->second.get()))
      return false;
  }
  return true;
}

void Transport::OnMessage(talk_base::Message* msg) {
  switch (msg->message_id) {
    case MSG_CREATECHANNEL: {
        ChannelParams* params = static_cast<ChannelParams*>(msg->pdata);
        params->channel = CreateChannel_w(params->component);
      }
      break;
    case MSG_DESTROYCHANNEL: {
        ChannelParams* params = static_cast<ChannelParams*>(msg->pdata);
        DestroyChannel_w(params->component);
      }
      break;
    case MSG_CONNECTCHANNELS:
      ConnectChannels_w();
      break;
    case MSG_RESETCHANNELS:
      ResetChannels_w();
      break;
    case MSG_DESTROYALLCHANNELS:
      DestroyAllChannels_w();
      break;
    case MSG_ONSIGNALINGREADY:
      CallChannels_w(&TransportChannelImpl::OnSignalingReady);
      break;
    case MSG_ONREMOTECANDIDATE: {
        ChannelParams* params = static_cast<ChannelParams*>(msg->pdata);
        OnRemoteCandidate_w(*params->candidate);
        delete params;
      }
      break;
    case MSG_CONNECTING:
      OnConnecting_s();
      break;
    case MSG_READSTATE:
      OnChannelReadableState_s();
      break;
    case MSG_WRITESTATE:
      OnChannelWritableState_s();
      break;
    case MSG_REQUESTSIGNALING: {
        ChannelParams* params = static_cast<ChannelParams*>(msg->pdata);
        OnChannelRequestSignaling_s(params->component);
        delete params;
      }
      break;
    case MSG_CANDIDATEREADY:
      OnChannelCandidateReady_s();
      break;
    case MSG_ROUTECHANGE: {
        ChannelParams* params = static_cast<ChannelParams*>(msg->pdata);
        OnChannelRouteChange_s(params->channel, *params->candidate);
        delete params;
      }
      break;
    case MSG_CANDIDATEALLOCATIONCOMPLETE:
      OnChannelCandidatesAllocationDone_s();
      break;
    case MSG_ROLECONFLICT:
      SignalRoleConflict();
      break;
    case MSG_SETICEROLE: {
        IceRoleParam* param =
            static_cast<IceRoleParam*>(msg->pdata);
        SetIceRole_w(param->role);
      }
      break;
    case MSG_SETLOCALDESCRIPTION: {
        TransportDescriptionParams* params =
            static_cast<TransportDescriptionParams*>(msg->pdata);
        params->result = SetLocalTransportDescription_w(params->desc,
                                                        params->action);
      }
      break;
    case MSG_SETREMOTEDESCRIPTION: {
        TransportDescriptionParams* params =
            static_cast<TransportDescriptionParams*>(msg->pdata);
        params->result = SetRemoteTransportDescription_w(params->desc,
                                                         params->action);
      }
      break;
    case MSG_GETSTATS: {
        StatsParam* params = static_cast<StatsParam*>(msg->pdata);
        params->result = GetStats_w(params->stats);
      }
      break;
    case MSG_SETIDENTITY: {
        IdentityParam* params = static_cast<IdentityParam*>(msg->pdata);
        SetIdentity_w(params->identity);
      }
      break;
  }
}

bool TransportParser::ParseAddress(const buzz::XmlElement* elem,
                                   const buzz::QName& address_name,
                                   const buzz::QName& port_name,
                                   talk_base::SocketAddress* address,
                                   ParseError* error) {
  if (!elem->HasAttr(address_name))
    return BadParse("address does not have " + address_name.LocalPart(), error);
  if (!elem->HasAttr(port_name))
    return BadParse("address does not have " + port_name.LocalPart(), error);

  address->SetIP(elem->Attr(address_name));
  std::istringstream ist(elem->Attr(port_name));
  int port = 0;
  ist >> port;
  address->SetPort(port);

  return true;
}

// We're GICE if the namespace is NS_GOOGLE_P2P, or if NS_JINGLE_ICE_UDP is
// used and the GICE ice-option is set.
TransportProtocol TransportProtocolFromDescription(
    const TransportDescription* desc) {
  ASSERT(desc != NULL);
  if (desc->transport_type == NS_JINGLE_ICE_UDP) {
    return (desc->HasOption(ICE_OPTION_GICE)) ?
        ICEPROTO_HYBRID : ICEPROTO_RFC5245;
  }
  return ICEPROTO_GOOGLE;
}

}  // namespace cricket
