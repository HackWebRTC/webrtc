/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>  // for std::pair

#include "webrtc/p2p/base/transport.h"

#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/constants.h"
#include "webrtc/p2p/base/port.h"
#include "webrtc/p2p/base/transportchannelimpl.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"

namespace cricket {

using rtc::Bind;

static bool VerifyIceParams(const TransportDescription& desc) {
  // For legacy protocols.
  if (desc.ice_ufrag.empty() && desc.ice_pwd.empty())
    return true;

  if (desc.ice_ufrag.length() < ICE_UFRAG_MIN_LENGTH ||
      desc.ice_ufrag.length() > ICE_UFRAG_MAX_LENGTH) {
    return false;
  }
  if (desc.ice_pwd.length() < ICE_PWD_MIN_LENGTH ||
      desc.ice_pwd.length() > ICE_PWD_MAX_LENGTH) {
    return false;
  }
  return true;
}

bool BadTransportDescription(const std::string& desc, std::string* err_desc) {
  if (err_desc) {
    *err_desc = desc;
  }
  LOG(LS_ERROR) << desc;
  return false;
}

bool IceCredentialsChanged(const std::string& old_ufrag,
                           const std::string& old_pwd,
                           const std::string& new_ufrag,
                           const std::string& new_pwd) {
  // TODO(jiayl): The standard (RFC 5245 Section 9.1.1.1) says that ICE should
  // restart when both the ufrag and password are changed, but we do restart
  // when either ufrag or passwrod is changed to keep compatible with GICE. We
  // should clean this up when GICE is no longer used.
  return (old_ufrag != new_ufrag) || (old_pwd != new_pwd);
}

static bool IceCredentialsChanged(const TransportDescription& old_desc,
                                  const TransportDescription& new_desc) {
  return IceCredentialsChanged(old_desc.ice_ufrag, old_desc.ice_pwd,
                               new_desc.ice_ufrag, new_desc.ice_pwd);
}

Transport::Transport(const std::string& name, PortAllocator* allocator)
    : name_(name), allocator_(allocator) {}

Transport::~Transport() {
  ASSERT(channels_destroyed_);
}

bool Transport::AllChannelsCompleted() const {
  // We aren't completed until at least one channel is complete, so if there
  // are no channels, we aren't complete yet.
  if (channels_.empty()) {
    LOG(LS_INFO) << name() << " transport is not complete"
                 << " because it has no TransportChannels";
    return false;
  }

  // A Transport's ICE process is completed if all of its channels are writable,
  // have finished allocating candidates, and have pruned all but one of their
  // connections.
  for (const auto& iter : channels_) {
    const TransportChannelImpl* channel = iter.second.get();
    bool complete =
        channel->writable() &&
        channel->GetState() == TransportChannelState::STATE_COMPLETED &&
        channel->GetIceRole() == ICEROLE_CONTROLLING &&
        channel->gathering_state() == kIceGatheringComplete;
    if (!complete) {
      LOG(LS_INFO) << name() << " transport is not complete"
                   << " because a channel is still incomplete.";
      return false;
    }
  }

  return true;
}

bool Transport::AnyChannelFailed() const {
  for (const auto& iter : channels_) {
    if (iter.second->GetState() == TransportChannelState::STATE_FAILED) {
      return true;
    }
  }
  return false;
}

void Transport::SetIceRole(IceRole role) {
  ice_role_ = role;
  for (auto& iter : channels_) {
    iter.second->SetIceRole(ice_role_);
  }
}

bool Transport::GetRemoteSSLCertificate(rtc::SSLCertificate** cert) {
  if (channels_.empty())
    return false;

  ChannelMap::iterator iter = channels_.begin();
  return iter->second->GetRemoteSSLCertificate(cert);
}

void Transport::SetChannelReceivingTimeout(int timeout_ms) {
  channel_receiving_timeout_ = timeout_ms;
  for (const auto& kv : channels_) {
    kv.second->SetReceivingTimeout(timeout_ms);
  }
}

bool Transport::SetLocalTransportDescription(
    const TransportDescription& description,
    ContentAction action,
    std::string* error_desc) {
  bool ret = true;

  if (!VerifyIceParams(description)) {
    return BadTransportDescription("Invalid ice-ufrag or ice-pwd length",
                                   error_desc);
  }

  if (local_description_ &&
      IceCredentialsChanged(*local_description_, description)) {
    IceRole new_ice_role =
        (action == CA_OFFER) ? ICEROLE_CONTROLLING : ICEROLE_CONTROLLED;

    // It must be called before ApplyLocalTransportDescription, which may
    // trigger an ICE restart and depends on the new ICE role.
    SetIceRole(new_ice_role);
  }

  local_description_.reset(new TransportDescription(description));

  for (auto& iter : channels_) {
    ret &= ApplyLocalTransportDescription(iter.second.get(), error_desc);
  }
  if (!ret) {
    return false;
  }

  // If PRANSWER/ANSWER is set, we should decide transport protocol type.
  if (action == CA_PRANSWER || action == CA_ANSWER) {
    ret &= NegotiateTransportDescription(action, error_desc);
  }
  if (ret) {
    local_description_set_ = true;
    ConnectChannels();
  }

  return ret;
}

bool Transport::SetRemoteTransportDescription(
    const TransportDescription& description,
    ContentAction action,
    std::string* error_desc) {
  bool ret = true;

  if (!VerifyIceParams(description)) {
    return BadTransportDescription("Invalid ice-ufrag or ice-pwd length",
                                   error_desc);
  }

  remote_description_.reset(new TransportDescription(description));
  for (auto& iter : channels_) {
    ret &= ApplyRemoteTransportDescription(iter.second.get(), error_desc);
  }

  // If PRANSWER/ANSWER is set, we should decide transport protocol type.
  if (action == CA_PRANSWER || action == CA_ANSWER) {
    ret = NegotiateTransportDescription(CA_OFFER, error_desc);
  }
  if (ret) {
    remote_description_set_ = true;
  }

  return ret;
}

TransportChannelImpl* Transport::CreateChannel(int component) {
  TransportChannelImpl* impl;

  // Create the entry if it does not exist.
  bool impl_exists = false;
  auto iterator = channels_.find(component);
  if (iterator == channels_.end()) {
    impl = CreateTransportChannel(component);
    iterator = channels_.insert(std::pair<int, ChannelMapEntry>(
        component, ChannelMapEntry(impl))).first;
  } else {
    impl = iterator->second.get();
    impl_exists = true;
  }

  // Increase the ref count.
  iterator->second.AddRef();
  channels_destroyed_ = false;

  if (impl_exists) {
    // If this is an existing channel, we should just return it without
    // connecting to all the signal again.
    return impl;
  }

  // Push down our transport state to the new channel.
  impl->SetIceRole(ice_role_);
  impl->SetIceTiebreaker(tiebreaker_);
  impl->SetReceivingTimeout(channel_receiving_timeout_);
  // TODO(ronghuawu): Change CreateChannel to be able to return error since
  // below Apply**Description calls can fail.
  if (local_description_)
    ApplyLocalTransportDescription(impl, NULL);
  if (remote_description_)
    ApplyRemoteTransportDescription(impl, NULL);
  if (local_description_ && remote_description_)
    ApplyNegotiatedTransportDescription(impl, NULL);

  impl->SignalWritableState.connect(this, &Transport::OnChannelWritableState);
  impl->SignalReceivingState.connect(this, &Transport::OnChannelReceivingState);
  impl->SignalGatheringState.connect(this, &Transport::OnChannelGatheringState);
  impl->SignalCandidateGathered.connect(this,
                                        &Transport::OnChannelCandidateGathered);
  impl->SignalRouteChange.connect(this, &Transport::OnChannelRouteChange);
  impl->SignalRoleConflict.connect(this, &Transport::OnRoleConflict);
  impl->SignalConnectionRemoved.connect(
      this, &Transport::OnChannelConnectionRemoved);

  if (connect_requested_) {
    impl->Connect();
    if (channels_.size() == 1) {
      // If this is the first channel, then indicate that we have started
      // connecting.
      SignalConnecting(this);
    }
  }
  return impl;
}

TransportChannelImpl* Transport::GetChannel(int component) {
  ChannelMap::iterator iter = channels_.find(component);
  return (iter != channels_.end()) ? iter->second.get() : NULL;
}

bool Transport::HasChannels() {
  return !channels_.empty();
}

void Transport::DestroyChannel(int component) {
  ChannelMap::iterator iter = channels_.find(component);
  if (iter == channels_.end())
    return;

  TransportChannelImpl* impl = NULL;

  iter->second.DecRef();
  if (!iter->second.ref()) {
    impl = iter->second.get();
    channels_.erase(iter);
  }

  if (connect_requested_ && channels_.empty()) {
    // We're no longer attempting to connect.
    SignalConnecting(this);
  }

  if (impl) {
    DestroyTransportChannel(impl);
    // Need to update aggregate state after destroying a channel,
    // for example if it was the only one that wasn't yet writable.
    UpdateWritableState();
    UpdateReceivingState();
    UpdateGatheringState();
    MaybeSignalCompleted();
  }
}

void Transport::ConnectChannels() {
  if (connect_requested_ || channels_.empty())
    return;

  connect_requested_ = true;

  if (!local_description_) {
    // TOOD(mallinath) : TransportDescription(TD) shouldn't be generated here.
    // As Transport must know TD is offer or answer and cricket::Transport
    // doesn't have the capability to decide it. This should be set by the
    // Session.
    // Session must generate local TD before remote candidates pushed when
    // initiate request initiated by the remote.
    LOG(LS_INFO) << "Transport::ConnectChannels: No local description has "
                 << "been set. Will generate one.";
    TransportDescription desc(
        std::vector<std::string>(), rtc::CreateRandomString(ICE_UFRAG_LENGTH),
        rtc::CreateRandomString(ICE_PWD_LENGTH), ICEMODE_FULL,
        CONNECTIONROLE_NONE, NULL, Candidates());
    SetLocalTransportDescription(desc, CA_OFFER, NULL);
  }

  CallChannels(&TransportChannelImpl::Connect);
  if (HasChannels()) {
    SignalConnecting(this);
  }
}

void Transport::MaybeStartGathering() {
  if (connect_requested_) {
    CallChannels(&TransportChannelImpl::MaybeStartGathering);
  }
}

void Transport::DestroyAllChannels() {
  std::vector<TransportChannelImpl*> impls;
  for (auto& iter : channels_) {
    iter.second.DecRef();
    if (!iter.second.ref())
      impls.push_back(iter.second.get());
  }

  channels_.clear();

  for (TransportChannelImpl* impl : impls) {
    DestroyTransportChannel(impl);
  }
  channels_destroyed_ = true;
}

void Transport::CallChannels(TransportChannelFunc func) {
  for (const auto& iter : channels_) {
    ((iter.second.get())->*func)();
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
  if (cand.protocol() == TCP_PROTOCOL_NAME &&
      (cand.tcptype() == TCPTYPE_ACTIVE_STR || port == 0)) {
    // Expected for active-only candidates per
    // http://tools.ietf.org/html/rfc6544#section-4.5 so no error.
    // Libjingle clients emit port 0, in "active" mode.
    return true;
  }
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
  stats->transport_name = name();
  stats->channel_stats.clear();
  for (auto iter : channels_) {
    ChannelMapEntry& entry = iter.second;
    TransportChannelStats substats;
    substats.component = entry->component();
    entry->GetSrtpCipher(&substats.srtp_cipher);
    entry->GetSslCipher(&substats.ssl_cipher);
    if (!entry->GetStats(&substats.connection_infos)) {
      return false;
    }
    stats->channel_stats.push_back(substats);
  }
  return true;
}

bool Transport::AddRemoteCandidates(const std::vector<Candidate>& candidates,
                                    std::string* error) {
  ASSERT(!channels_destroyed_);
  // Verify each candidate before passing down to transport layer.
  for (const Candidate& cand : candidates) {
    if (!VerifyCandidate(cand, error)) {
      return false;
    }
    if (!HasChannel(cand.component())) {
      *error = "Candidate has unknown component: " + cand.ToString() +
               " for content: " + name();
      return false;
    }
  }

  for (std::vector<Candidate>::const_iterator iter = candidates.begin();
       iter != candidates.end();
       ++iter) {
    TransportChannelImpl* channel = GetChannel(iter->component());
    if (channel != NULL) {
      channel->AddRemoteCandidate(*iter);
    }
  }
  return true;
}

void Transport::OnChannelWritableState(TransportChannel* channel) {
  LOG(LS_INFO) << name() << " TransportChannel " << channel->component()
               << " writability changed to " << channel->writable()
               << ". Check if transport is complete.";
  UpdateWritableState();
  MaybeSignalCompleted();
}

void Transport::OnChannelReceivingState(TransportChannel* channel) {
  UpdateReceivingState();
}

TransportState Transport::GetTransportState(TransportStateType state_type) {
  bool any = false;
  bool all = !channels_.empty();
  for (const auto iter : channels_) {
    bool b = false;
    switch (state_type) {
      case TRANSPORT_WRITABLE_STATE:
        b = iter.second->writable();
        break;
      case TRANSPORT_RECEIVING_STATE:
        b = iter.second->receiving();
        break;
      default:
        ASSERT(false);
    }
    any |= b;
    all &=  b;
  }

  if (all) {
    return TRANSPORT_STATE_ALL;
  } else if (any) {
    return TRANSPORT_STATE_SOME;
  }

  return TRANSPORT_STATE_NONE;
}

void Transport::OnChannelGatheringState(TransportChannelImpl* channel) {
  ASSERT(channels_.find(channel->component()) != channels_.end());
  UpdateGatheringState();
  if (gathering_state_ == kIceGatheringComplete) {
    // If UpdateGatheringState brought us to kIceGatheringComplete, check if
    // our connection state is also "Completed". Otherwise, there's no point in
    // checking (since it would only produce log messages).
    MaybeSignalCompleted();
  }
}

void Transport::OnChannelCandidateGathered(TransportChannelImpl* channel,
                                           const Candidate& candidate) {
  // We should never signal peer-reflexive candidates.
  if (candidate.type() == PRFLX_PORT_TYPE) {
    ASSERT(false);
    return;
  }

  ASSERT(connect_requested_);
  std::vector<Candidate> candidates;
  candidates.push_back(candidate);
  SignalCandidatesGathered(this, candidates);
}

void Transport::OnChannelRouteChange(TransportChannel* channel,
                                     const Candidate& remote_candidate) {
  SignalRouteChange(this, remote_candidate.component(), remote_candidate);
}

void Transport::OnRoleConflict(TransportChannelImpl* channel) {
  SignalRoleConflict();
}

void Transport::OnChannelConnectionRemoved(TransportChannelImpl* channel) {
  LOG(LS_INFO) << name() << " TransportChannel " << channel->component()
               << " connection removed. Check if transport is complete.";
  MaybeSignalCompleted();

  // Check if the state is now Failed.
  // Failed is only available in the Controlling ICE role.
  if (channel->GetIceRole() != ICEROLE_CONTROLLING) {
    return;
  }

  // Failed can only occur after candidate gathering has stopped.
  if (channel->gathering_state() != kIceGatheringComplete) {
    return;
  }

  if (channel->GetState() == TransportChannelState::STATE_FAILED) {
    // A Transport has failed if any of its channels have no remaining
    // connections.
    SignalFailed(this);
  }
}

void Transport::MaybeSignalCompleted() {
  if (AllChannelsCompleted()) {
    LOG(LS_INFO) << name() << " transport is complete"
                 << " because all the channels are complete.";
    SignalCompleted(this);
  }
  // TODO(deadbeef): Should we do anything if we previously were completed,
  // but now are not (if, for example, a new remote candidate is added)?
}

void Transport::UpdateGatheringState() {
  IceGatheringState new_state = kIceGatheringNew;
  bool any_gathering = false;
  bool all_complete = !channels_.empty();
  for (const auto& kv : channels_) {
    any_gathering =
        any_gathering || kv.second->gathering_state() != kIceGatheringNew;
    all_complete =
        all_complete && kv.second->gathering_state() == kIceGatheringComplete;
  }
  if (all_complete) {
    new_state = kIceGatheringComplete;
  } else if (any_gathering) {
    new_state = kIceGatheringGathering;
  }

  if (gathering_state_ != new_state) {
    gathering_state_ = new_state;
    if (gathering_state_ == kIceGatheringGathering) {
      LOG(LS_INFO) << "Transport: " << name_ << ", gathering candidates";
    } else if (gathering_state_ == kIceGatheringComplete) {
      LOG(LS_INFO) << "Transport " << name() << " gathering complete.";
    }
    SignalGatheringState(this);
  }
}

void Transport::UpdateReceivingState() {
  TransportState receiving = GetTransportState(TRANSPORT_RECEIVING_STATE);
  if (receiving_ != receiving) {
    receiving_ = receiving;
    SignalReceivingState(this);
  }
}

void Transport::UpdateWritableState() {
  TransportState writable = GetTransportState(TRANSPORT_WRITABLE_STATE);
  LOG(LS_INFO) << name() << " transport writable state changed? " << writable_
               << " => " << writable;
  if (writable_ != writable) {
    was_writable_ = (writable_ == TRANSPORT_STATE_ALL);
    writable_ = writable;
    SignalWritableState(this);
  }
}

bool Transport::ApplyLocalTransportDescription(TransportChannelImpl* ch,
                                               std::string* error_desc) {
  ch->SetIceCredentials(local_description_->ice_ufrag,
                        local_description_->ice_pwd);
  return true;
}

bool Transport::ApplyRemoteTransportDescription(TransportChannelImpl* ch,
                                                std::string* error_desc) {
  ch->SetRemoteIceCredentials(remote_description_->ice_ufrag,
                              remote_description_->ice_pwd);
  return true;
}

bool Transport::ApplyNegotiatedTransportDescription(
    TransportChannelImpl* channel,
    std::string* error_desc) {
  channel->SetRemoteIceMode(remote_ice_mode_);
  return true;
}

bool Transport::NegotiateTransportDescription(ContentAction local_role,
                                              std::string* error_desc) {
  // TODO(ekr@rtfm.com): This is ICE-specific stuff. Refactor into
  // P2PTransport.

  // If transport is in ICEROLE_CONTROLLED and remote end point supports only
  // ice_lite, this local end point should take CONTROLLING role.
  if (ice_role_ == ICEROLE_CONTROLLED &&
      remote_description_->ice_mode == ICEMODE_LITE) {
    SetIceRole(ICEROLE_CONTROLLING);
  }

  // Update remote ice_mode to all existing channels.
  remote_ice_mode_ = remote_description_->ice_mode;

  // Now that we have negotiated everything, push it downward.
  // Note that we cache the result so that if we have race conditions
  // between future SetRemote/SetLocal invocations and new channel
  // creation, we have the negotiation state saved until a new
  // negotiation happens.
  for (auto& iter : channels_) {
    if (!ApplyNegotiatedTransportDescription(iter.second.get(), error_desc))
      return false;
  }
  return true;
}

}  // namespace cricket
