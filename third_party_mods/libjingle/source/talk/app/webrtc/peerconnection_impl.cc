/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#include "talk/app/webrtc/peerconnection_impl.h"

#include "talk/app/webrtc/webrtc_json.h"
#include "talk/app/webrtc/webrtcsession.h"
#include "talk/base/basicpacketsocketfactory.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/base/stringencode.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/client/basicportallocator.h"

namespace webrtc {


PeerConnectionImpl::PeerConnectionImpl(
    cricket::PortAllocator* port_allocator,
    cricket::ChannelManager* channel_manager,
    talk_base::Thread* signaling_thread)
  : port_allocator_(port_allocator),
    channel_manager_(channel_manager),
    signaling_thread_(signaling_thread),
    event_callback_(NULL),
    session_(NULL) {
}

PeerConnectionImpl::~PeerConnectionImpl() {
}

bool PeerConnectionImpl::Init() {
  std::string sid;
  talk_base::CreateRandomString(8, &sid);
  const bool incoming = false;  // default outgoing direction
  session_.reset(CreateMediaSession(sid, incoming));
  if (session_.get() == NULL) {
    ASSERT(false && "failed to initialize a session");
    return false;
  }
  return true;
}

void PeerConnectionImpl::RegisterObserver(PeerConnectionObserver* observer) {
  // This assert is to catch cases where two observer pointers are registered.
  // We only support one and if another is to be used, the current one must be
  // cleared first.
  ASSERT(observer == NULL || event_callback_ == NULL);
  event_callback_ = observer;
}

bool PeerConnectionImpl::SignalingMessage(
    const std::string& signaling_message) {
  // Deserialize signaling message
  cricket::SessionDescription* incoming_sdp = NULL;
  std::vector<cricket::Candidate> candidates;
  if (!ParseJSONSignalingMessage(signaling_message,
                                 incoming_sdp, &candidates)) {
    return false;
  }

  bool ret = false;
  if (GetReadyState() == NEW) {
    // set direction to incoming, as message received first
    session_->set_incoming(true);
    ret = session_->OnInitiateMessage(incoming_sdp, candidates);
  } else {
    ret = session_->OnRemoteDescription(incoming_sdp, candidates);
  }
  return ret;
}

WebRtcSession* PeerConnectionImpl::CreateMediaSession(
    const std::string& id, bool incoming) {
  ASSERT(port_allocator_ != NULL);
  WebRtcSession* session = new WebRtcSession(id, incoming,
      port_allocator_, channel_manager_, signaling_thread_);

  if (session->Initiate()) {
    session->SignalAddStream.connect(
        this,
        &PeerConnectionImpl::OnAddStream);
    session->SignalRemoveStream.connect(
        this,
        &PeerConnectionImpl::OnRemoveStream);
    session->SignalLocalDescription.connect(
        this,
        &PeerConnectionImpl::OnLocalDescription);
    session->SignalFailedCall.connect(
        this,
        &PeerConnectionImpl::OnFailedCall);
  } else {
    delete session;
    session = NULL;
  }
  return session;
}

bool PeerConnectionImpl::AddStream(const std::string& stream_id, bool video) {
  bool ret = false;
  if (session_->HasStream(stream_id)) {
    ASSERT(false && "A stream with this name already exists");
  } else {
    if (!video) {
      ret = !session_->HasAudioStream() &&
            session_->CreateVoiceChannel(stream_id);
    } else {
      ret = !session_->HasVideoStream() &&
            session_->CreateVideoChannel(stream_id);
    }
  }
  return ret;
}

bool PeerConnectionImpl::RemoveStream(const std::string& stream_id) {
  return session_->RemoveStream(stream_id);
}

void PeerConnectionImpl::OnLocalDescription(
    const cricket::SessionDescription* desc,
    const std::vector<cricket::Candidate>& candidates) {
  if (!desc) {
    LOG(WARNING) << "no local SDP ";
    return;
  }

  std::string message;
  if (GetJSONSignalingMessage(desc, candidates, &message)) {
    if (event_callback_) {
      event_callback_->OnSignalingMessage(message);
    }
  }
}

void PeerConnectionImpl::OnFailedCall() {
  // TODO(mallinath): implement.
}

bool PeerConnectionImpl::SetAudioDevice(const std::string& wave_in_device,
                                        const std::string& wave_out_device,
                                        int opts) {
  return channel_manager_->SetAudioOptions(wave_in_device,
                                           wave_out_device,
                                           opts);
}

bool PeerConnectionImpl::SetLocalVideoRenderer(
    cricket::VideoRenderer* renderer) {
  return channel_manager_->SetLocalRenderer(renderer);
}

bool PeerConnectionImpl::SetVideoRenderer(const std::string& stream_id,
                                          cricket::VideoRenderer* renderer) {
  return session_->SetVideoRenderer(stream_id, renderer);
}

bool PeerConnectionImpl::SetVideoCapture(const std::string& cam_device) {
  return channel_manager_->SetVideoOptions(cam_device);
}

bool PeerConnectionImpl::Connect() {
  return session_->Connect();
}

// TODO(mallinath) - Close is not used anymore, should be removed.
bool PeerConnectionImpl::Close() {
  session_->RemoveAllStreams();
  return true;
}

void PeerConnectionImpl::OnAddStream(const std::string& stream_id,
                                     bool video) {
  if (event_callback_) {
    event_callback_->OnAddStream(stream_id, video);
  }
}

void PeerConnectionImpl::OnRemoveStream(const std::string& stream_id,
                                        bool video) {
  if (event_callback_) {
    event_callback_->OnRemoveStream(stream_id, video);
  }
}

PeerConnectionImpl::ReadyState PeerConnectionImpl::GetReadyState() {
  ReadyState ready_state;
  cricket::BaseSession::State state = session_->state();
  if (state == cricket::BaseSession::STATE_INIT) {
    ready_state = NEW;
  } else if (state == cricket::BaseSession::STATE_INPROGRESS) {
    ready_state = ACTIVE;
  } else if (state == cricket::BaseSession::STATE_DEINIT) {
    ready_state = CLOSED;
  } else {
    ready_state = NEGOTIATING;
  }
  return ready_state;
}

}  // namespace webrtc
