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
#include "talk/p2p/client/basicportallocator.h"

namespace webrtc {

// The number of the tokens in the config string.
static const size_t kConfigTokens = 2;

// The default stun port.
static const int kDefaultStunPort = 3478;

PeerConnectionImpl::PeerConnectionImpl(const std::string& config,
      cricket::PortAllocator* port_allocator,
      cricket::MediaEngine* media_engine,
      talk_base::Thread* worker_thread,
      talk_base::Thread* signaling_thread,
      cricket::DeviceManager* device_manager)
  : config_(config),
    port_allocator_(port_allocator),
    media_engine_(media_engine),
    worker_thread_(worker_thread),
    device_manager_(device_manager),
    signaling_thread_(signaling_thread),
    initialized_(false),
    service_type_(SERVICE_COUNT),
    event_callback_(NULL),
    session_(NULL),
    incoming_(false) {
}

PeerConnectionImpl::PeerConnectionImpl(const std::string& config,
                                       cricket::PortAllocator* port_allocator,
                                       talk_base::Thread* worker_thread)
  : config_(config),
    port_allocator_(port_allocator),
    media_engine_(NULL),
    worker_thread_(worker_thread),
    device_manager_(NULL),
    signaling_thread_(NULL),
    initialized_(false),
    service_type_(SERVICE_COUNT),
    event_callback_(NULL),
    session_(NULL),
    incoming_(false) {
}

PeerConnectionImpl::~PeerConnectionImpl() {
  session_.reset();
  channel_manager_.reset();
}

bool PeerConnectionImpl::Init() {
  ASSERT(!initialized_);
  std::vector<talk_base::SocketAddress> stun_hosts;
  talk_base::SocketAddress stun_addr;
  if (!ParseConfigString(config_, &stun_addr))
    return false;
  stun_hosts.push_back(stun_addr);

  // TODO(mallinath) - Changes are required to modify the stand alone
  // constructor to get signaling thread as input. It should not be created
  // here.
  if (!signaling_thread_) {
    signaling_thread_ = new talk_base::Thread();
    if (!signaling_thread_->SetName("signaling thread", this) ||
        !signaling_thread_->Start()) {
      LOG(WARNING) << "Failed to start libjingle signaling thread";
      return false;
    }
  }

  // create cricket::ChannelManager object
  ASSERT(worker_thread_ != NULL);
  if (media_engine_ && device_manager_) {
    channel_manager_.reset(new cricket::ChannelManager(
        media_engine_, device_manager_, worker_thread_));
  } else {
    channel_manager_.reset(new cricket::ChannelManager(worker_thread_));
  }

  initialized_ = channel_manager_->Init();

  if (event_callback_) {
    // TODO(ronghuawu): OnInitialized is no longer needed.
    if (initialized_)
      event_callback_->OnInitialized();
  }
  return initialized_;
}

bool PeerConnectionImpl::ParseConfigString(
    const std::string& config, talk_base::SocketAddress* stun_addr) {
  std::vector<std::string> tokens;
  talk_base::tokenize(config_, ' ', &tokens);

  if (tokens.size() != kConfigTokens) {
    LOG(WARNING) << "Invalid config string";
    return false;
  }

  service_type_ = SERVICE_COUNT;
  // NOTE: Must be in the same order as the enum.
  static const char* kValidServiceTypes[SERVICE_COUNT] = {
    "STUN", "STUNS", "TURN", "TURNS"
  };
  const std::string& type = tokens[0];
  for (size_t i = 0; i < SERVICE_COUNT; ++i) {
    if (type.compare(kValidServiceTypes[i]) == 0) {
      service_type_ = static_cast<ServiceType>(i);
      break;
    }
  }

  if (service_type_ == SERVICE_COUNT) {
    LOG(WARNING) << "Invalid service type: " << type;
    return false;
  }
  std::string service_address = tokens[1];

  int port;
  tokens.clear();
  talk_base::tokenize(service_address, ':', &tokens);
  if (tokens.size() != kConfigTokens) {
    port = kDefaultStunPort;
  } else {
    port = atoi(tokens[1].c_str());
    if (port <= 0 || port > 0xffff) {
      LOG(WARNING) << "Invalid port: " << tokens[1];
      return false;
    }
  }
  stun_addr->SetIP(service_address);
  stun_addr->SetPort(port);
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
  if (!session_.get()) {
    // this will be incoming call
    std::string sid;
    talk_base::CreateRandomString(8, &sid);
    std::string direction("r");
    session_.reset(CreateMediaSession(sid, direction));
    if (session_.get() == NULL) {
      ASSERT(false && "failed to initialize a session");
      return false;
    }
    incoming_ = true;
    ret = session_->OnInitiateMessage(incoming_sdp, candidates);
  } else {
    ret = session_->OnRemoteDescription(incoming_sdp, candidates);
  }

  return ret;
}

WebRtcSession* PeerConnectionImpl::CreateMediaSession(
    const std::string& id, const std::string& dir) {
  ASSERT(port_allocator_ != NULL);
  WebRtcSession* session = new WebRtcSession(id, dir,
      port_allocator_, channel_manager_.get(),
      signaling_thread_);

  if (session->Initiate()) {
    session->SignalRemoveStreamMessage.connect(
        this,
        &PeerConnectionImpl::SendRemoveSignal);
    session->SignalAddStream.connect(
        this,
        &PeerConnectionImpl::OnAddStream);
    session->SignalRemoveStream.connect(
        this,
        &PeerConnectionImpl::OnRemoveStream);
    session->SignalRtcMediaChannelCreated.connect(
        this,
        &PeerConnectionImpl::OnRtcMediaChannelCreated);
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

void PeerConnectionImpl::SendRemoveSignal(WebRtcSession* session) {
  std::string message;
  if (GetJSONSignalingMessage(session->remote_description(),
                              session->local_candidates(), &message)) {
    if (event_callback_) {
      event_callback_->OnSignalingMessage(message);
      // TODO(ronghuawu): Notify the client when the PeerConnection object
      // doesn't have any streams. Something like the onreadystatechanged
      // + setting readyState to 'CLOSED'.
    }
  }
}

bool PeerConnectionImpl::AddStream(const std::string& stream_id, bool video) {
  if (!session_.get()) {
    // if session doesn't exist then this should be an outgoing call
    std::string sid;
    talk_base::CreateRandomString(8, &sid);
    session_.reset(CreateMediaSession(sid, "s"));
    if (session_.get() == NULL) {
      ASSERT(false && "failed to initialize a session");
      return false;
    }
  }

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
  if (!session_.get()) {
    return false;
  }
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
  if (!session_.get()) {
    return false;
  }
  return session_->SetVideoRenderer(stream_id, renderer);
}

bool PeerConnectionImpl::SetVideoCapture(const std::string& cam_device) {
  return channel_manager_->SetVideoOptions(cam_device);
}

bool PeerConnectionImpl::Connect() {
  if (!session_.get()) {
    return false;
  }
  return session_->Connect();
}

bool PeerConnectionImpl::Close() {
  if (!session_.get()) {
    return false;
  }

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

void PeerConnectionImpl::OnRtcMediaChannelCreated(const std::string& stream_id,
                                                  bool video) {
  if (event_callback_) {
    event_callback_->OnLocalStreamInitialized(stream_id, video);
  }
}

}  // namespace webrtc
