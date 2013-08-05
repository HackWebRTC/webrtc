/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#include "talk/app/webrtc/peerconnection.h"

#include <vector>

#include "talk/app/webrtc/dtmfsender.h"
#include "talk/app/webrtc/jsepicecandidate.h"
#include "talk/app/webrtc/jsepsessiondescription.h"
#include "talk/app/webrtc/mediastreamhandler.h"
#include "talk/app/webrtc/streamcollection.h"
#include "talk/base/logging.h"
#include "talk/base/stringencode.h"
#include "talk/session/media/channelmanager.h"

namespace {

using webrtc::PeerConnectionInterface;

// The min number of tokens in the ice uri.
static const size_t kMinIceUriTokens = 2;
// The min number of tokens must present in Turn host uri.
// e.g. user@turn.example.org
static const size_t kTurnHostTokensNum = 2;
// Number of tokens must be preset when TURN uri has transport param.
static const size_t kTurnTransportTokensNum = 2;
// The default stun port.
static const int kDefaultPort = 3478;
static const char kTransport[] = "transport";
static const char kDefaultTransportType[] = "udp";

// NOTE: Must be in the same order as the ServiceType enum.
static const char* kValidIceServiceTypes[] = {
    "stun", "stuns", "turn", "turns", "invalid" };

enum ServiceType {
  STUN,     // Indicates a STUN server.
  STUNS,    // Indicates a STUN server used with a TLS session.
  TURN,     // Indicates a TURN server
  TURNS,    // Indicates a TURN server used with a TLS session.
  INVALID,  // Unknown.
};

enum {
  MSG_CREATE_SESSIONDESCRIPTION_SUCCESS = 0,
  MSG_CREATE_SESSIONDESCRIPTION_FAILED,
  MSG_SET_SESSIONDESCRIPTION_SUCCESS,
  MSG_SET_SESSIONDESCRIPTION_FAILED,
  MSG_GETSTATS,
  MSG_ICECONNECTIONCHANGE,
  MSG_ICEGATHERINGCHANGE,
  MSG_ICECANDIDATE,
  MSG_ICECOMPLETE,
};

struct CandidateMsg : public talk_base::MessageData {
  explicit CandidateMsg(const webrtc::JsepIceCandidate* candidate)
      : candidate(candidate) {
  }
  talk_base::scoped_ptr<const webrtc::JsepIceCandidate> candidate;
};

struct CreateSessionDescriptionMsg : public talk_base::MessageData {
  explicit CreateSessionDescriptionMsg(
      webrtc::CreateSessionDescriptionObserver* observer)
      : observer(observer) {
  }

  talk_base::scoped_refptr<webrtc::CreateSessionDescriptionObserver> observer;
  std::string error;
  talk_base::scoped_ptr<webrtc::SessionDescriptionInterface> description;
};

struct SetSessionDescriptionMsg : public talk_base::MessageData {
  explicit SetSessionDescriptionMsg(
      webrtc::SetSessionDescriptionObserver* observer)
      : observer(observer) {
  }

  talk_base::scoped_refptr<webrtc::SetSessionDescriptionObserver> observer;
  std::string error;
};

struct GetStatsMsg : public talk_base::MessageData {
  explicit GetStatsMsg(webrtc::StatsObserver* observer)
      : observer(observer) {
  }
  webrtc::StatsReports reports;
  talk_base::scoped_refptr<webrtc::StatsObserver> observer;
};

typedef webrtc::PortAllocatorFactoryInterface::StunConfiguration
    StunConfiguration;
typedef webrtc::PortAllocatorFactoryInterface::TurnConfiguration
    TurnConfiguration;

bool ParseIceServers(const PeerConnectionInterface::IceServers& configuration,
                     std::vector<StunConfiguration>* stun_config,
                     std::vector<TurnConfiguration>* turn_config) {
  // draft-nandakumar-rtcweb-stun-uri-01
  // stunURI       = scheme ":" stun-host [ ":" stun-port ]
  // scheme        = "stun" / "stuns"
  // stun-host     = IP-literal / IPv4address / reg-name
  // stun-port     = *DIGIT

  // draft-petithuguenin-behave-turn-uris-01
  // turnURI       = scheme ":" turn-host [ ":" turn-port ]
  //                 [ "?transport=" transport ]
  // scheme        = "turn" / "turns"
  // transport     = "udp" / "tcp" / transport-ext
  // transport-ext = 1*unreserved
  // turn-host     = IP-literal / IPv4address / reg-name
  // turn-port     = *DIGIT

  // TODO(ronghuawu): Handle IPV6 address
  for (size_t i = 0; i < configuration.size(); ++i) {
    webrtc::PeerConnectionInterface::IceServer server = configuration[i];
    if (server.uri.empty()) {
      LOG(WARNING) << "Empty uri.";
      continue;
    }
    std::vector<std::string> tokens;
    std::string turn_transport_type = kDefaultTransportType;
    talk_base::tokenize(server.uri, '?', &tokens);
    std::string uri_without_transport = tokens[0];
    // Let's look into transport= param, if it exists.
    if (tokens.size() == kTurnTransportTokensNum) {  // ?transport= is present.
      std::string uri_transport_param = tokens[1];
      talk_base::tokenize(uri_transport_param, '=', &tokens);
      if (tokens[0] == kTransport) {
        turn_transport_type = tokens[1];
      }
    }

    tokens.clear();
    talk_base::tokenize(uri_without_transport, ':', &tokens);
    if (tokens.size() < kMinIceUriTokens) {
      LOG(WARNING) << "Invalid uri: " << server.uri;
      continue;
    }
    ServiceType service_type = INVALID;
    const std::string& type = tokens[0];
    for (size_t i = 0; i < ARRAY_SIZE(kValidIceServiceTypes); ++i) {
      if (type.compare(kValidIceServiceTypes[i]) == 0) {
        service_type = static_cast<ServiceType>(i);
        break;
      }
    }
    if (service_type == INVALID) {
      LOG(WARNING) << "Invalid service type: " << type;
      continue;
    }
    std::string address = tokens[1];
    int port = kDefaultPort;
    if (tokens.size() > kMinIceUriTokens) {
      if (!talk_base::FromString(tokens[2], &port)) {
        LOG(LS_WARNING)  << "Failed to parse port string: " << tokens[2];
        continue;
      }

      if (port <= 0 || port > 0xffff) {
        LOG(WARNING) << "Invalid port: " << port;
        continue;
      }
    }

    switch (service_type) {
      case STUN:
      case STUNS:
        stun_config->push_back(StunConfiguration(address, port));
        break;
      case TURN: {
        if (server.username.empty()) {
          // Turn url example from the spec |url:"turn:user@turn.example.org"|.
          std::vector<std::string> turn_tokens;
          talk_base::tokenize(address, '@', &turn_tokens);
          if (turn_tokens.size() == kTurnHostTokensNum) {
            server.username = talk_base::s_url_decode(turn_tokens[0]);
            address = turn_tokens[1];
          }
        }
        turn_config->push_back(TurnConfiguration(address, port,
                                                 server.username,
                                                 server.password,
                                                 turn_transport_type));
        // STUN functionality is part of TURN.
        stun_config->push_back(StunConfiguration(address, port));
        break;
      }
      case TURNS:
      case INVALID:
      default:
        LOG(WARNING) << "Configuration not supported: " << server.uri;
        return false;
    }
  }
  return true;
}

// Check if we can send |new_stream| on a PeerConnection.
// Currently only one audio but multiple video track is supported per
// PeerConnection.
bool CanAddLocalMediaStream(webrtc::StreamCollectionInterface* current_streams,
                            webrtc::MediaStreamInterface* new_stream) {
  if (!new_stream || !current_streams)
    return false;
  if (current_streams->find(new_stream->label()) != NULL) {
    LOG(LS_ERROR) << "MediaStream with label " << new_stream->label()
                  << " is already added.";
    return false;
  }

  return true;
}

}  // namespace

namespace webrtc {

PeerConnection::PeerConnection(PeerConnectionFactory* factory)
    : factory_(factory),
      observer_(NULL),
      signaling_state_(kStable),
      ice_state_(kIceNew),
      ice_connection_state_(kIceConnectionNew),
      ice_gathering_state_(kIceGatheringNew) {
}

PeerConnection::~PeerConnection() {
  if (mediastream_signaling_)
    mediastream_signaling_->TearDown();
  if (stream_handler_container_)
    stream_handler_container_->TearDown();
}

bool PeerConnection::Initialize(
    const PeerConnectionInterface::IceServers& configuration,
    const MediaConstraintsInterface* constraints,
    webrtc::PortAllocatorFactoryInterface* allocator_factory,
    PeerConnectionObserver* observer) {
  std::vector<PortAllocatorFactoryInterface::StunConfiguration> stun_config;
  std::vector<PortAllocatorFactoryInterface::TurnConfiguration> turn_config;
  if (!ParseIceServers(configuration, &stun_config, &turn_config)) {
    return false;
  }

  return DoInitialize(stun_config, turn_config, constraints,
                      allocator_factory, observer);
}

bool PeerConnection::DoInitialize(
    const StunConfigurations& stun_config,
    const TurnConfigurations& turn_config,
    const MediaConstraintsInterface* constraints,
    webrtc::PortAllocatorFactoryInterface* allocator_factory,
    PeerConnectionObserver* observer) {
  ASSERT(observer != NULL);
  if (!observer)
    return false;
  observer_ = observer;
  port_allocator_.reset(
      allocator_factory->CreatePortAllocator(stun_config, turn_config));
  // To handle both internal and externally created port allocator, we will
  // enable BUNDLE here. Also enabling TURN and disable legacy relay service.
  port_allocator_->set_flags(cricket::PORTALLOCATOR_ENABLE_BUNDLE |
                             cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
                             cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
  // No step delay is used while allocating ports.
  port_allocator_->set_step_delay(cricket::kMinimumStepDelay);

  mediastream_signaling_.reset(new MediaStreamSignaling(
      factory_->signaling_thread(), this));

  session_.reset(new WebRtcSession(factory_->channel_manager(),
                                   factory_->signaling_thread(),
                                   factory_->worker_thread(),
                                   port_allocator_.get(),
                                   mediastream_signaling_.get()));
  stream_handler_container_.reset(new MediaStreamHandlerContainer(
      session_.get(), session_.get()));
  stats_.set_session(session_.get());

  // Initialize the WebRtcSession. It creates transport channels etc.
  if (!session_->Initialize(constraints))
    return false;


  // Register PeerConnection as receiver of local ice candidates.
  // All the callbacks will be posted to the application from PeerConnection.
  session_->RegisterIceObserver(this);
  session_->SignalState.connect(this, &PeerConnection::OnSessionStateChange);
  return true;
}

talk_base::scoped_refptr<StreamCollectionInterface>
PeerConnection::local_streams() {
  return mediastream_signaling_->local_streams();
}

talk_base::scoped_refptr<StreamCollectionInterface>
PeerConnection::remote_streams() {
  return mediastream_signaling_->remote_streams();
}

bool PeerConnection::AddStream(MediaStreamInterface* local_stream,
                               const MediaConstraintsInterface* constraints) {
  if (IsClosed()) {
    return false;
  }
  if (!CanAddLocalMediaStream(mediastream_signaling_->local_streams(),
                              local_stream))
    return false;

  // TODO(perkj): Implement support for MediaConstraints in AddStream.
  if (!mediastream_signaling_->AddLocalStream(local_stream)) {
    return false;
  }
  stats_.AddStream(local_stream);
  observer_->OnRenegotiationNeeded();
  return true;
}

void PeerConnection::RemoveStream(MediaStreamInterface* local_stream) {
  if (IsClosed()) {
    return;
  }
  mediastream_signaling_->RemoveLocalStream(local_stream);
  observer_->OnRenegotiationNeeded();
}

talk_base::scoped_refptr<DtmfSenderInterface> PeerConnection::CreateDtmfSender(
    AudioTrackInterface* track) {
  if (!track) {
    LOG(LS_ERROR) << "CreateDtmfSender - track is NULL.";
    return NULL;
  }
  if (!mediastream_signaling_->local_streams()->FindAudioTrack(track->id())) {
    LOG(LS_ERROR) << "CreateDtmfSender is called with a non local audio track.";
    return NULL;
  }

  talk_base::scoped_refptr<DtmfSenderInterface> sender(
      DtmfSender::Create(track, signaling_thread(), session_.get()));
  if (!sender.get()) {
    LOG(LS_ERROR) << "CreateDtmfSender failed on DtmfSender::Create.";
    return NULL;
  }
  return DtmfSenderProxy::Create(signaling_thread(), sender.get());
}

bool PeerConnection::GetStats(StatsObserver* observer,
                              MediaStreamTrackInterface* track) {
  if (!VERIFY(observer != NULL)) {
    LOG(LS_ERROR) << "GetStats - observer is NULL.";
    return false;
  }

  stats_.UpdateStats();
  talk_base::scoped_ptr<GetStatsMsg> msg(new GetStatsMsg(observer));
  if (!stats_.GetStats(track, &(msg->reports))) {
    return false;
  }
  signaling_thread()->Post(this, MSG_GETSTATS, msg.release());
  return true;
}

PeerConnectionInterface::SignalingState PeerConnection::signaling_state() {
  return signaling_state_;
}

PeerConnectionInterface::IceState PeerConnection::ice_state() {
  return ice_state_;
}

PeerConnectionInterface::IceConnectionState
PeerConnection::ice_connection_state() {
  return ice_connection_state_;
}

PeerConnectionInterface::IceGatheringState
PeerConnection::ice_gathering_state() {
  return ice_gathering_state_;
}

talk_base::scoped_refptr<DataChannelInterface>
PeerConnection::CreateDataChannel(
    const std::string& label,
    const DataChannelInit* config) {
  talk_base::scoped_refptr<DataChannelInterface> channel(
      session_->CreateDataChannel(label, config));
  if (!channel.get())
    return NULL;

  observer_->OnRenegotiationNeeded();
  return DataChannelProxy::Create(signaling_thread(), channel.get());
}

void PeerConnection::CreateOffer(CreateSessionDescriptionObserver* observer,
                                 const MediaConstraintsInterface* constraints) {
  if (!VERIFY(observer != NULL)) {
    LOG(LS_ERROR) << "CreateOffer - observer is NULL.";
    return;
  }
  CreateSessionDescriptionMsg* msg = new CreateSessionDescriptionMsg(observer);
  msg->description.reset(
      session_->CreateOffer(constraints));

  if (!msg->description) {
    msg->error = "CreateOffer failed.";
    signaling_thread()->Post(this, MSG_CREATE_SESSIONDESCRIPTION_FAILED, msg);
    return;
  }

  signaling_thread()->Post(this, MSG_CREATE_SESSIONDESCRIPTION_SUCCESS, msg);
}

void PeerConnection::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const MediaConstraintsInterface* constraints) {
  if (!VERIFY(observer != NULL)) {
    LOG(LS_ERROR) << "CreateAnswer - observer is NULL.";
    return;
  }
  CreateSessionDescriptionMsg* msg = new CreateSessionDescriptionMsg(observer);
  msg->description.reset(session_->CreateAnswer(constraints));
  if (!msg->description) {
    msg->error = "CreateAnswer failed.";
    signaling_thread()->Post(this, MSG_CREATE_SESSIONDESCRIPTION_FAILED, msg);
    return;
  }

  signaling_thread()->Post(this, MSG_CREATE_SESSIONDESCRIPTION_SUCCESS, msg);
}

void PeerConnection::SetLocalDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  if (!VERIFY(observer != NULL)) {
    LOG(LS_ERROR) << "SetLocalDescription - observer is NULL.";
    return;
  }
  if (!desc) {
    PostSetSessionDescriptionFailure(observer, "SessionDescription is NULL.");
    return;
  }

  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  stats_.UpdateStats();
  std::string error;
  if (!session_->SetLocalDescription(desc, &error)) {
    PostSetSessionDescriptionFailure(observer, error);
    return;
  }
  SetSessionDescriptionMsg* msg =  new SetSessionDescriptionMsg(observer);
  signaling_thread()->Post(this, MSG_SET_SESSIONDESCRIPTION_SUCCESS, msg);
}

void PeerConnection::SetRemoteDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  if (!VERIFY(observer != NULL)) {
    LOG(LS_ERROR) << "SetRemoteDescription - observer is NULL.";
    return;
  }

  if (!desc) {
    PostSetSessionDescriptionFailure(observer, "SessionDescription is NULL.");
    return;
  }
  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  stats_.UpdateStats();
  std::string error;
  if (!session_->SetRemoteDescription(desc, &error)) {
    PostSetSessionDescriptionFailure(observer, error);
    return;
  }
  SetSessionDescriptionMsg* msg  = new SetSessionDescriptionMsg(observer);
  signaling_thread()->Post(this, MSG_SET_SESSIONDESCRIPTION_SUCCESS, msg);
}

void PeerConnection::PostSetSessionDescriptionFailure(
    SetSessionDescriptionObserver* observer,
    const std::string& error) {
  SetSessionDescriptionMsg* msg  = new SetSessionDescriptionMsg(observer);
  msg->error = error;
  signaling_thread()->Post(this, MSG_SET_SESSIONDESCRIPTION_FAILED, msg);
}

bool PeerConnection::UpdateIce(const IceServers& configuration,
                               const MediaConstraintsInterface* constraints) {
  // TODO(ronghuawu): Implement UpdateIce.
  LOG(LS_ERROR) << "UpdateIce is not implemented.";
  return false;
}

bool PeerConnection::AddIceCandidate(
    const IceCandidateInterface* ice_candidate) {
  return session_->ProcessIceMessage(ice_candidate);
}

const SessionDescriptionInterface* PeerConnection::local_description() const {
  return session_->local_description();
}

const SessionDescriptionInterface* PeerConnection::remote_description() const {
  return session_->remote_description();
}

void PeerConnection::Close() {
  // Update stats here so that we have the most recent stats for tracks and
  // streams before the channels are closed.
  stats_.UpdateStats();

  session_->Terminate();
}

void PeerConnection::OnSessionStateChange(cricket::BaseSession* /*session*/,
                                          cricket::BaseSession::State state) {
  switch (state) {
    case cricket::BaseSession::STATE_INIT:
      ChangeSignalingState(PeerConnectionInterface::kStable);
    case cricket::BaseSession::STATE_SENTINITIATE:
      ChangeSignalingState(PeerConnectionInterface::kHaveLocalOffer);
      break;
    case cricket::BaseSession::STATE_SENTPRACCEPT:
      ChangeSignalingState(PeerConnectionInterface::kHaveLocalPrAnswer);
      break;
    case cricket::BaseSession::STATE_RECEIVEDINITIATE:
      ChangeSignalingState(PeerConnectionInterface::kHaveRemoteOffer);
      break;
    case cricket::BaseSession::STATE_RECEIVEDPRACCEPT:
      ChangeSignalingState(PeerConnectionInterface::kHaveRemotePrAnswer);
      break;
    case cricket::BaseSession::STATE_SENTACCEPT:
    case cricket::BaseSession::STATE_RECEIVEDACCEPT:
      ChangeSignalingState(PeerConnectionInterface::kStable);
      break;
    case cricket::BaseSession::STATE_RECEIVEDTERMINATE:
      ChangeSignalingState(PeerConnectionInterface::kClosed);
      break;
    default:
      break;
  }
}

void PeerConnection::OnMessage(talk_base::Message* msg) {
  switch (msg->message_id) {
    case MSG_CREATE_SESSIONDESCRIPTION_SUCCESS: {
      CreateSessionDescriptionMsg* param =
          static_cast<CreateSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnSuccess(param->description.release());
      delete param;
      break;
    }
    case MSG_CREATE_SESSIONDESCRIPTION_FAILED: {
      CreateSessionDescriptionMsg* param =
          static_cast<CreateSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnFailure(param->error);
      delete param;
      break;
    }
    case MSG_SET_SESSIONDESCRIPTION_SUCCESS: {
      SetSessionDescriptionMsg* param =
          static_cast<SetSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnSuccess();
      delete param;
      break;
    }
    case MSG_SET_SESSIONDESCRIPTION_FAILED: {
      SetSessionDescriptionMsg* param =
          static_cast<SetSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnFailure(param->error);
      delete param;
      break;
    }
    case MSG_GETSTATS: {
      GetStatsMsg* param = static_cast<GetStatsMsg*>(msg->pdata);
      param->observer->OnComplete(param->reports);
      delete param;
      break;
    }
    case MSG_ICECONNECTIONCHANGE: {
      observer_->OnIceConnectionChange(ice_connection_state_);
      break;
    }
    case MSG_ICEGATHERINGCHANGE: {
      observer_->OnIceGatheringChange(ice_gathering_state_);
      break;
    }
    case MSG_ICECANDIDATE: {
      CandidateMsg* data = static_cast<CandidateMsg*>(msg->pdata);
      observer_->OnIceCandidate(data->candidate.get());
      delete data;
      break;
    }
    case MSG_ICECOMPLETE: {
      observer_->OnIceComplete();
      break;
    }
    default:
      ASSERT(false && "Not implemented");
      break;
  }
}

void PeerConnection::OnAddRemoteStream(MediaStreamInterface* stream) {
  stats_.AddStream(stream);
  observer_->OnAddStream(stream);
}

void PeerConnection::OnRemoveRemoteStream(MediaStreamInterface* stream) {
  stream_handler_container_->RemoveRemoteStream(stream);
  observer_->OnRemoveStream(stream);
}

void PeerConnection::OnAddDataChannel(DataChannelInterface* data_channel) {
  observer_->OnDataChannel(DataChannelProxy::Create(signaling_thread(),
                                                    data_channel));
}

void PeerConnection::OnAddRemoteAudioTrack(MediaStreamInterface* stream,
                                           AudioTrackInterface* audio_track,
                                           uint32 ssrc) {
  stream_handler_container_->AddRemoteAudioTrack(stream, audio_track, ssrc);
}

void PeerConnection::OnAddRemoteVideoTrack(MediaStreamInterface* stream,
                                           VideoTrackInterface* video_track,
                                           uint32 ssrc) {
  stream_handler_container_->AddRemoteVideoTrack(stream, video_track, ssrc);
}

void PeerConnection::OnRemoveRemoteAudioTrack(
    MediaStreamInterface* stream,
    AudioTrackInterface* audio_track) {
  stream_handler_container_->RemoveRemoteTrack(stream, audio_track);
}

void PeerConnection::OnRemoveRemoteVideoTrack(
    MediaStreamInterface* stream,
    VideoTrackInterface* video_track) {
  stream_handler_container_->RemoveRemoteTrack(stream, video_track);
}
void PeerConnection::OnAddLocalAudioTrack(MediaStreamInterface* stream,
                                          AudioTrackInterface* audio_track,
                                          uint32 ssrc) {
  stream_handler_container_->AddLocalAudioTrack(stream, audio_track, ssrc);
}
void PeerConnection::OnAddLocalVideoTrack(MediaStreamInterface* stream,
                                          VideoTrackInterface* video_track,
                                          uint32 ssrc) {
  stream_handler_container_->AddLocalVideoTrack(stream, video_track, ssrc);
}

void PeerConnection::OnRemoveLocalAudioTrack(MediaStreamInterface* stream,
                                             AudioTrackInterface* audio_track) {
  stream_handler_container_->RemoveLocalTrack(stream, audio_track);
}

void PeerConnection::OnRemoveLocalVideoTrack(MediaStreamInterface* stream,
                                             VideoTrackInterface* video_track) {
  stream_handler_container_->RemoveLocalTrack(stream, video_track);
}

void PeerConnection::OnRemoveLocalStream(MediaStreamInterface* stream) {
  stream_handler_container_->RemoveLocalStream(stream);
}

void PeerConnection::OnIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  ice_connection_state_ = new_state;
  signaling_thread()->Post(this, MSG_ICECONNECTIONCHANGE);
}

void PeerConnection::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  if (IsClosed()) {
    return;
  }
  ice_gathering_state_ = new_state;
  signaling_thread()->Post(this, MSG_ICEGATHERINGCHANGE);
}

void PeerConnection::OnIceCandidate(const IceCandidateInterface* candidate) {
  JsepIceCandidate* candidate_copy = NULL;
  if (candidate) {
    // TODO(ronghuawu): Make IceCandidateInterface reference counted instead
    // of making a copy.
    candidate_copy = new JsepIceCandidate(candidate->sdp_mid(),
                                          candidate->sdp_mline_index(),
                                          candidate->candidate());
  }
  // The Post takes the ownership of the |candidate_copy|.
  signaling_thread()->Post(this, MSG_ICECANDIDATE,
                           new CandidateMsg(candidate_copy));
}

void PeerConnection::OnIceComplete() {
  signaling_thread()->Post(this, MSG_ICECOMPLETE);
}

void PeerConnection::ChangeSignalingState(
    PeerConnectionInterface::SignalingState signaling_state) {
  signaling_state_ = signaling_state;
  if (signaling_state == kClosed) {
    ice_connection_state_ = kIceConnectionClosed;
    observer_->OnIceConnectionChange(ice_connection_state_);
    if (ice_gathering_state_ != kIceGatheringComplete) {
      ice_gathering_state_ = kIceGatheringComplete;
      observer_->OnIceGatheringChange(ice_gathering_state_);
    }
  }
  observer_->OnSignalingChange(signaling_state_);
  observer_->OnStateChange(PeerConnectionObserver::kSignalingState);
}

}  // namespace webrtc
