/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#include "talk/app/webrtc_dev/webrtcsession.h"

#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/session/phone/channel.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/app/webrtc_dev/mediastream.h"
#include "talk/app/webrtc_dev/peerconnectionsignaling.h"

namespace webrtc {

enum {
  MSG_CANDIDATE_TIMEOUT = 101,
};

// We allow 30 seconds to establish a connection, otherwise it's an error.
static const int kCallSetupTimeout = 30 * 1000;

WebRtcSession::WebRtcSession(cricket::ChannelManager* channel_manager,
                             talk_base::Thread* worker_thread,
                             cricket::PortAllocator* port_allocator,
                             PeerConnectionSignaling* pc_signaling)
      // TODO(ronghuawu): get the signaling thread from PeerConnectionSignaling
    : cricket::BaseSession(worker_thread, worker_thread,
                           port_allocator,
                           talk_base::ToString(talk_base::CreateRandomId()),
                           cricket::NS_JINGLE_RTP, true) {
  // TODO(mallinath) - Remove initiator flag from BaseSession. As it's being
  // used only by cricket::Session.
  channel_manager_ = channel_manager;
  pc_signaling_ = pc_signaling;
}

WebRtcSession::~WebRtcSession() {
  Terminate();
}

bool WebRtcSession::Initialize() {
  ASSERT(pc_signaling_ != NULL);
  pc_signaling_->SignalUpdateSessionDescription.connect(
      this, &WebRtcSession::OnSignalUpdateSessionDescription);
  return CreateChannels();
}

void WebRtcSession::Terminate() {
  if (voice_channel_.get()) {
    channel_manager_->DestroyVoiceChannel(voice_channel_.release());
  }
  if (video_channel_.get()) {
    channel_manager_->DestroyVideoChannel(video_channel_.release());
  }
}

void WebRtcSession::OnSignalUpdateSessionDescription(
    const cricket::SessionDescription* local_desc,
    const cricket::SessionDescription* remote_desc,
    StreamCollection* streams) {
  if (state() == STATE_INPROGRESS) {
    // TODO(mallinath) - Handling of session updates is not ready yet.
    return;
  }
}

bool WebRtcSession::CreateChannels() {
  voice_channel_.reset(channel_manager_->CreateVoiceChannel(
      this, cricket::CN_AUDIO, true));
  if (!voice_channel_.get()) {
    LOG(LS_ERROR) << "Failed to create voice channel";
    return false;
  }

  video_channel_.reset(channel_manager_->CreateVideoChannel(
      this, cricket::CN_VIDEO, true, voice_channel_.get()));
  if (!video_channel_.get()) {
    LOG(LS_ERROR) << "Failed to create video channel";
    return false;
  }
  return true;
}

void WebRtcSession::OnTransportRequestSignaling(
    cricket::Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
  transport->OnSignalingReady();
}

void WebRtcSession::OnTransportConnecting(cricket::Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
  // start monitoring for the write state of the transport.
  OnTransportWritable(transport);
}

void WebRtcSession::OnTransportWritable(cricket::Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
  // If the transport is not in writable state, start a timer to monitor
  // the state. If the transport doesn't become writable state in 30 seconds
  // then we are assuming call can't be continued.
  signaling_thread()->Clear(this, MSG_CANDIDATE_TIMEOUT);
  if (transport->HasChannels() && !transport->writable()) {
    signaling_thread()->PostDelayed(
        kCallSetupTimeout, this, MSG_CANDIDATE_TIMEOUT);
  }
}

void WebRtcSession::OnTransportCandidatesReady(
    cricket::Transport* transport, const cricket::Candidates& candidates) {
  ASSERT(signaling_thread()->IsCurrent());
  pc_signaling_->Initialize(candidates);
}

void WebRtcSession::OnTransportChannelGone(cricket::Transport* transport) {
  ASSERT(signaling_thread()->IsCurrent());
}

void WebRtcSession::OnMessage(talk_base::Message* msg) {
  switch (msg->message_id) {
    case MSG_CANDIDATE_TIMEOUT:
      break;
    default:
      break;
  }
}

}  // namespace webrtc
