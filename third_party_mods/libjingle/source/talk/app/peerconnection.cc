// Copyright 2011 Google Inc. All Rights Reserved.
// Author: mallinath@google.com (Mallinath Bareddy)

#include <vector>

#include "talk/app/peerconnection.h"

#include "talk/base/basicpacketsocketfactory.h"
#include "talk/base/helpers.h"
#include "talk/base/stringencode.h"
#include "talk/base/logging.h"

#include "talk/p2p/client/basicportallocator.h"
#include "talk/session/phone/mediasessionclient.h"
#include "talk/app/webrtcsessionimpl.h"
#include "talk/app/webrtc_json.h"

namespace webrtc {

static const size_t kConfigTokens = 2;
static const int kDefaultStunPort = 3478;

#ifdef PLATFORM_CHROMIUM
PeerConnection::PeerConnection(const std::string& config,
                               P2PSocketDispatcher* p2p_socket_dispatcher)
#else
PeerConnection::PeerConnection(const std::string& config)
#endif  // PLATFORM_CHROMIUM
      : config_(config)
      ,media_thread_(new talk_base::Thread)
      ,network_manager_(new talk_base::NetworkManager)
      ,signaling_thread_(new talk_base::Thread)
      ,initialized_(false)
      ,service_type_(SERVICE_COUNT)
      ,event_callback_(NULL)
      ,session_(NULL)
      ,incoming_(false)
#ifdef PLATFORM_CHROMIUM
      ,p2p_socket_dispatcher_(p2p_socket_dispatcher)
#endif  // PLATFORM_CHROMIUM
{
}

PeerConnection::~PeerConnection() {
  if (session_ != NULL) {
    // Before deleting the session, make sure that the signaling thread isn't
    // running (or wait for it if it is).
    signaling_thread_.reset();

    ASSERT(!session_->HasAudioStream());
    ASSERT(!session_->HasVideoStream());
    // TODO: the RemoveAllStreams has to be asynchronous. At the same
    //time "delete session_" should be called after RemoveAllStreams completed.
    delete session_;
  }
}

bool PeerConnection::Init() {
  ASSERT(!initialized_);

  std::vector<std::string> tokens;
  talk_base::tokenize(config_, ' ', &tokens);

  if (tokens.size() != kConfigTokens) {
    LOG(LS_ERROR) << "Invalid config string";
    return false;
  }

  service_type_ = SERVICE_COUNT;

  // NOTE: Must be in the same order as the enum.
  static const char* kValidServiceTypes[SERVICE_COUNT] = {
    "STUN", "STUNS","TURN", "TURNS"
  };
  const std::string& type = tokens[0];
  for (size_t i = 0; i < SERVICE_COUNT; ++i) {
    if (type.compare(kValidServiceTypes[i]) == 0) {
      service_type_ = static_cast<ServiceType>(i);
      break;
    }
  }

  if (service_type_ == SERVICE_COUNT) {
    LOG(LS_ERROR) << "Invalid service type: " << type;
    return false;
  }

  service_address_ = tokens[1];

  int port;
  tokens.clear();
  talk_base::tokenize(service_address_, ':', &tokens);
  if (tokens.size() != kConfigTokens) {
    port = kDefaultStunPort;
  } else {
    port = atoi(tokens[1].c_str());
    if (port <= 0 || port > 0xffff) {
      LOG(LS_ERROR) << "Invalid port: " << tokens[1];
      return false;
    }
  }

  talk_base::SocketAddress stun_addr(tokens[0], port);

  socket_factory_.reset(new talk_base::BasicPacketSocketFactory(
      media_thread_.get()));

  port_allocator_.reset(new cricket::BasicPortAllocator(network_manager_.get(),
		  stun_addr, talk_base::SocketAddress(), talk_base::SocketAddress(),
      talk_base::SocketAddress()));

  ASSERT(port_allocator_.get() != NULL);
  port_allocator_->set_flags(cricket::PORTALLOCATOR_DISABLE_STUN |
                             cricket::PORTALLOCATOR_DISABLE_TCP |
                             cricket::PORTALLOCATOR_DISABLE_RELAY);

  // create channel manager
  channel_manager_.reset(new WebRtcChannelManager(media_thread_.get()));

  //start the media thread
  media_thread_->SetPriority(talk_base::PRIORITY_HIGH);
  media_thread_->SetName("PeerConn", this);
  if (!media_thread_->Start()) {
    LOG(LS_ERROR) << "Failed to start media thread";
  } else if (!channel_manager_->Init()) {
    LOG(LS_ERROR) << "Failed to initialize the channel manager";
  } if (!signaling_thread_->SetName("Session Signaling Thread", this) ||
        !signaling_thread_->Start()) {
    LOG(LS_ERROR) << "Failed to start session signaling thread";
  } else {
    initialized_ = true;
  }

  return initialized_;
}

void PeerConnection::RegisterObserver(PeerConnectionObserver* observer) {
  // This assert is to catch cases where two observer pointers are registered.
  // We only support one and if another is to be used, the current one must be
  // cleared first.
  ASSERT(observer == NULL || event_callback_ == NULL);
  event_callback_ = observer;
}

bool PeerConnection::SignalingMessage(const std::string& signaling_message) {
  // Deserialize signaling message
  cricket::SessionDescription* incoming_sdp = NULL;
  std::vector<cricket::Candidate> candidates;
  if (!ParseJSONSignalingMessage(signaling_message, incoming_sdp, candidates))
    return false;

  bool ret = false;
  if (!session_) {
    // this will be incoming call
    std::string sid;
    talk_base::CreateRandomString(8, &sid);
    std::string direction("r");
    session_ = CreateMediaSession(sid, direction);
    ASSERT(session_ != NULL);
    incoming_ = true;
    ret = session_->OnInitiateMessage(incoming_sdp, candidates);
  } else {
    ret = session_->OnRemoteDescription(incoming_sdp, candidates);
  }
  return ret;
}

WebRTCSessionImpl* PeerConnection::CreateMediaSession(const std::string& id,
                                                      const std::string& dir) {
  WebRTCSessionImpl* session = new WebRTCSessionImpl(id, dir,
      port_allocator_.get(), channel_manager_.get(), this,
      signaling_thread_.get());
  if (session) {
    session->SignalOnRemoveStream.connect(this,
                                          &PeerConnection::SendRemoveSignal);
  }
  return session;
}

void PeerConnection::SendRemoveSignal(WebRTCSessionImpl* session) {
  if (event_callback_) {
    std::string message;
    if (GetJSONSignalingMessage(session->remote_description(),
                                session->local_candidates(), &message)) {
      event_callback_->OnSignalingMessage(message);
    }
  }
}

bool PeerConnection::AddStream(const std::string& stream_id, bool video) {
  if (!session_) {
    // if session doesn't exist then this should be an outgoing call
    std::string sid;
    if (!talk_base::CreateRandomString(8, &sid) ||
        (session_ = CreateMediaSession(sid, "s")) == NULL) {
      ASSERT(false && "failed to initialize a session");
      return false;
    }
  }

  bool ret = false;

  if (session_->HasStream(stream_id)) {
    ASSERT(false && "A stream with this name already exists");
  } else {
    //TODO: we should ensure CreateVoiceChannel/CreateVideoChannel be called
    // after transportchannel is ready
    if (!video) {
      ret = !session_->HasAudioStream() &&
            session_->CreateP2PTransportChannel(stream_id, video) &&
            session_->CreateVoiceChannel(stream_id);
    } else {
      ret = !session_->HasVideoStream() &&
            session_->CreateP2PTransportChannel(stream_id, video) &&
            session_->CreateVideoChannel(stream_id);
    }
  }
  return ret;
}

bool PeerConnection::RemoveStream(const std::string& stream_id) {
  ASSERT(session_ != NULL);
  return session_->RemoveStream(stream_id);
}

void PeerConnection::OnLocalDescription(
    cricket::SessionDescription* desc,
    const std::vector<cricket::Candidate>& candidates) {
  if (!desc) {
    LOG(LS_ERROR) << "no local SDP ";
    return;
  }

  std::string message;
  if (GetJSONSignalingMessage(desc, candidates, &message)) {
    if (event_callback_) {
      event_callback_->OnSignalingMessage(message);
    }
  }
}

bool PeerConnection::SetAudioDevice(const std::string& wave_in_device,
                                    const std::string& wave_out_device, int opts) {
  return channel_manager_->SetAudioOptions(wave_in_device, wave_out_device, opts);
}

bool PeerConnection::SetVideoRenderer(const std::string& stream_id,
    ExternalRenderer* external_renderer) {
  ASSERT(session_ != NULL);
  return session_->SetVideoRenderer(stream_id, external_renderer);
}

bool PeerConnection::SetVideoRenderer(int channel_id,
                                      void* window,
                                      unsigned int zOrder,
                                      float left,
                                      float top,
                                      float right,
                                      float bottom) {
  ASSERT(session_ != NULL);
  return session_->SetVideoRenderer(channel_id, window, zOrder, left, top,
                                    right, bottom);
}

bool PeerConnection::SetVideoCapture(const std::string& cam_device) {
  return channel_manager_->SetVideoOptions(cam_device);
}

bool PeerConnection::Connect() {
  return session_->Initiate();
}

void PeerConnection::OnAddStream(const std::string& stream_id,
                                 int channel_id,
                                 bool video) {
  if (event_callback_) {
    event_callback_->OnAddStream(stream_id, channel_id, video);
  }
}

void PeerConnection::OnRemoveStream(const std::string& stream_id,
                                    int channel_id,
                                    bool video) {
  if (event_callback_) {
    event_callback_->OnRemoveStream(stream_id, channel_id, video);
  }
}

void PeerConnection::OnRtcMediaChannelCreated(const std::string& stream_id,
                                              int channel_id,
                                              bool video) {
  if (event_callback_) {
    event_callback_->OnAddStream(stream_id, channel_id, video);
  }
}

void PeerConnection::Close() {
  if (session_)
    session_->RemoveAllStreams();
}

} // namespace webrtc
