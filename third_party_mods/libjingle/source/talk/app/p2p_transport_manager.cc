// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "talk/app/p2p_transport_manager.h"

#include "talk/base/socketaddress.h"
#include "talk/p2p/base/p2ptransportchannel.h"
#include "talk/p2p/client/httpportallocator.h"
#include "talk/p2p/client/basicportallocator.h"

namespace webrtc {

P2PTransportManager::P2PTransportManager(cricket::PortAllocator* allocator)
    : event_handler_(NULL)
    ,state_(STATE_NONE)
    ,allocator_(allocator) {
}

P2PTransportManager::~P2PTransportManager() {
}

bool P2PTransportManager::Init(const std::string& name,
                               Protocol protocol,
                               const std::string& config,
                               EventHandler* event_handler) {
  name_ = name;
  event_handler_ = event_handler;

  channel_.reset(new cricket::P2PTransportChannel(
      name, "", NULL, allocator_));
  channel_->SignalRequestSignaling.connect(
      this, &P2PTransportManager::OnRequestSignaling);
  channel_->SignalWritableState.connect(
      this, &P2PTransportManager::OnReadableState);
  channel_->SignalWritableState.connect(
      this, &P2PTransportManager::OnWriteableState);
  channel_->SignalCandidateReady.connect(
      this, &P2PTransportManager::OnCandidateReady);

  channel_->Connect();
  return true;
}

bool P2PTransportManager::AddRemoteCandidate(
    const cricket::Candidate& candidate) {
  channel_->OnCandidate(candidate);
  return true;
}

cricket::P2PTransportChannel* P2PTransportManager::GetP2PChannel() {
  return channel_.get();
}

void P2PTransportManager::OnRequestSignaling() {
  channel_->OnSignalingReady();
}

void P2PTransportManager::OnCandidateReady(
    cricket::TransportChannelImpl* channel,
    const cricket::Candidate& candidate) {
  event_handler_->OnCandidateReady(candidate);
}

void P2PTransportManager::OnReadableState(cricket::TransportChannel* channel) {
  state_ = static_cast<State>(state_ | STATE_READABLE);
  event_handler_->OnStateChange(state_);
}

void P2PTransportManager::OnWriteableState(cricket::TransportChannel* channel) {
  state_ = static_cast<State>(state_ | STATE_WRITABLE);
  event_handler_->OnStateChange(state_);
}

}
