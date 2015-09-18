/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/session.h"

#include "webrtc/base/bind.h"
#include "webrtc/base/common.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/p2p/base/transport.h"
#include "webrtc/p2p/base/transportinfo.h"
#include "webrtc/p2p/base/transportcontroller.h"
#include "webrtc/p2p/base/constants.h"

namespace cricket {

using rtc::Bind;

std::string BaseSession::StateToString(State state) {
  switch (state) {
    case STATE_INIT:
      return "STATE_INIT";
    case STATE_SENTINITIATE:
      return "STATE_SENTINITIATE";
    case STATE_RECEIVEDINITIATE:
      return "STATE_RECEIVEDINITIATE";
    case STATE_SENTPRACCEPT:
      return "STATE_SENTPRACCEPT";
    case STATE_SENTACCEPT:
      return "STATE_SENTACCEPT";
    case STATE_RECEIVEDPRACCEPT:
      return "STATE_RECEIVEDPRACCEPT";
    case STATE_RECEIVEDACCEPT:
      return "STATE_RECEIVEDACCEPT";
    case STATE_SENTMODIFY:
      return "STATE_SENTMODIFY";
    case STATE_RECEIVEDMODIFY:
      return "STATE_RECEIVEDMODIFY";
    case STATE_SENTREJECT:
      return "STATE_SENTREJECT";
    case STATE_RECEIVEDREJECT:
      return "STATE_RECEIVEDREJECT";
    case STATE_SENTREDIRECT:
      return "STATE_SENTREDIRECT";
    case STATE_SENTTERMINATE:
      return "STATE_SENTTERMINATE";
    case STATE_RECEIVEDTERMINATE:
      return "STATE_RECEIVEDTERMINATE";
    case STATE_INPROGRESS:
      return "STATE_INPROGRESS";
    case STATE_DEINIT:
      return "STATE_DEINIT";
    default:
      break;
  }
  return "STATE_" + rtc::ToString(state);
}

BaseSession::BaseSession(rtc::Thread* signaling_thread,
                         rtc::Thread* worker_thread,
                         PortAllocator* port_allocator,
                         const std::string& sid,
                         bool initiator)
    : state_(STATE_INIT),
      error_(ERROR_NONE),
      signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      port_allocator_(port_allocator),
      sid_(sid),
      transport_controller_(new TransportController(signaling_thread,
                                                    worker_thread,
                                                    port_allocator)) {
  ASSERT(signaling_thread->IsCurrent());
  set_initiator(initiator);
}

BaseSession::~BaseSession() {
  ASSERT(signaling_thread()->IsCurrent());

  ASSERT(state_ != STATE_DEINIT);
  LogState(state_, STATE_DEINIT);
  state_ = STATE_DEINIT;
  SignalState(this, state_);
}

const SessionDescription* BaseSession::local_description() const {
  // TODO(tommi): Assert on thread correctness.
  return local_description_.get();
}

const SessionDescription* BaseSession::remote_description() const {
  // TODO(tommi): Assert on thread correctness.
  return remote_description_.get();
}

SessionDescription* BaseSession::remote_description() {
  // TODO(tommi): Assert on thread correctness.
  return remote_description_.get();
}

void BaseSession::set_local_description(const SessionDescription* sdesc) {
  // TODO(tommi): Assert on thread correctness.
  if (sdesc != local_description_.get())
    local_description_.reset(sdesc);
}

void BaseSession::set_remote_description(SessionDescription* sdesc) {
  // TODO(tommi): Assert on thread correctness.
  if (sdesc != remote_description_)
    remote_description_.reset(sdesc);
}

void BaseSession::set_initiator(bool initiator) {
  initiator_ = initiator;

  IceRole ice_role = initiator ? ICEROLE_CONTROLLING : ICEROLE_CONTROLLED;
  transport_controller_->SetIceRole(ice_role);
}

const SessionDescription* BaseSession::initiator_description() const {
  // TODO(tommi): Assert on thread correctness.
  return initiator_ ? local_description_.get() : remote_description_.get();
}

bool BaseSession::PushdownTransportDescription(ContentSource source,
                                               ContentAction action,
                                               std::string* error_desc) {
  ASSERT(signaling_thread()->IsCurrent());

  if (source == CS_LOCAL) {
    return PushdownLocalTransportDescription(local_description(),
                                             action,
                                             error_desc);
  }
  return PushdownRemoteTransportDescription(remote_description(),
                                            action,
                                            error_desc);
}

bool BaseSession::PushdownLocalTransportDescription(
    const SessionDescription* sdesc,
    ContentAction action,
    std::string* err) {
  ASSERT(signaling_thread()->IsCurrent());

  if (!sdesc) {
    return false;
  }

  for (const TransportInfo& tinfo : sdesc->transport_infos()) {
    if (!transport_controller_->SetLocalTransportDescription(
            tinfo.content_name, tinfo.description, action, err)) {
      return false;
    }
  }

  return true;
}

bool BaseSession::PushdownRemoteTransportDescription(
    const SessionDescription* sdesc,
    ContentAction action,
    std::string* err) {
  ASSERT(signaling_thread()->IsCurrent());

  if (!sdesc) {
    return false;
  }

  for (const TransportInfo& tinfo : sdesc->transport_infos()) {
    if (!transport_controller_->SetRemoteTransportDescription(
            tinfo.content_name, tinfo.description, action, err)) {
      return false;
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
}

void BaseSession::SetError(Error error, const std::string& error_desc) {
  ASSERT(signaling_thread_->IsCurrent());
  if (error != error_) {
    error_ = error;
    error_desc_ = error_desc;
    SignalError(this, error);
  }
}

void BaseSession::SetIceConnectionReceivingTimeout(int timeout_ms) {
  transport_controller_->SetIceConnectionReceivingTimeout(timeout_ms);
}

void BaseSession::LogState(State old_state, State new_state) {
  LOG(LS_INFO) << "Session:" << id()
               << " Old state:" << StateToString(old_state)
               << " New state:" << StateToString(new_state);
}

// static
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

void BaseSession::OnMessage(rtc::Message *pmsg) {
  switch (pmsg->message_id) {
  case MSG_TIMEOUT:
    // Session timeout has occured.
    SetError(ERROR_TIME, "Session timeout has occured.");
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

}  // namespace cricket
