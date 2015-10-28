/*
 * libjingle
 * Copyright 2012 Google Inc.
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
#include <cctype>  // for isdigit

#include "talk/app/webrtc/audiotrack.h"
#include "talk/app/webrtc/dtmfsender.h"
#include "talk/app/webrtc/jsepicecandidate.h"
#include "talk/app/webrtc/jsepsessiondescription.h"
#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/app/webrtc/mediastream.h"
#include "talk/app/webrtc/mediastreamproxy.h"
#include "talk/app/webrtc/mediastreamtrackproxy.h"
#include "talk/app/webrtc/remoteaudiosource.h"
#include "talk/app/webrtc/remotevideocapturer.h"
#include "talk/app/webrtc/rtpreceiver.h"
#include "talk/app/webrtc/rtpsender.h"
#include "talk/app/webrtc/streamcollection.h"
#include "talk/app/webrtc/videosource.h"
#include "talk/app/webrtc/videotrack.h"
#include "talk/media/sctp/sctpdataengine.h"
#include "webrtc/p2p/client/basicportallocator.h"
#include "talk/session/media/channelmanager.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/system_wrappers/include/field_trial.h"

namespace {

using webrtc::DataChannel;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaStreamInterface;
using webrtc::PeerConnectionInterface;
using webrtc::StreamCollection;
using webrtc::StunConfigurations;
using webrtc::TurnConfigurations;
typedef webrtc::PortAllocatorFactoryInterface::StunConfiguration
    StunConfiguration;
typedef webrtc::PortAllocatorFactoryInterface::TurnConfiguration
    TurnConfiguration;

static const char kDefaultStreamLabel[] = "default";
static const char kDefaultAudioTrackLabel[] = "defaulta0";
static const char kDefaultVideoTrackLabel[] = "defaultv0";

// The min number of tokens must present in Turn host uri.
// e.g. user@turn.example.org
static const size_t kTurnHostTokensNum = 2;
// Number of tokens must be preset when TURN uri has transport param.
static const size_t kTurnTransportTokensNum = 2;
// The default stun port.
static const int kDefaultStunPort = 3478;
static const int kDefaultStunTlsPort = 5349;
static const char kTransport[] = "transport";
static const char kUdpTransportType[] = "udp";
static const char kTcpTransportType[] = "tcp";

// NOTE: Must be in the same order as the ServiceType enum.
static const char* kValidIceServiceTypes[] = {"stun", "stuns", "turn", "turns"};

// NOTE: A loop below assumes that the first value of this enum is 0 and all
// other values are incremental.
enum ServiceType {
  STUN = 0,  // Indicates a STUN server.
  STUNS,     // Indicates a STUN server used with a TLS session.
  TURN,      // Indicates a TURN server
  TURNS,     // Indicates a TURN server used with a TLS session.
  INVALID,   // Unknown.
};
static_assert(INVALID == ARRAY_SIZE(kValidIceServiceTypes),
              "kValidIceServiceTypes must have as many strings as ServiceType "
              "has values.");

enum {
  MSG_SET_SESSIONDESCRIPTION_SUCCESS = 0,
  MSG_SET_SESSIONDESCRIPTION_FAILED,
  MSG_CREATE_SESSIONDESCRIPTION_FAILED,
  MSG_GETSTATS,
};

struct SetSessionDescriptionMsg : public rtc::MessageData {
  explicit SetSessionDescriptionMsg(
      webrtc::SetSessionDescriptionObserver* observer)
      : observer(observer) {
  }

  rtc::scoped_refptr<webrtc::SetSessionDescriptionObserver> observer;
  std::string error;
};

struct CreateSessionDescriptionMsg : public rtc::MessageData {
  explicit CreateSessionDescriptionMsg(
      webrtc::CreateSessionDescriptionObserver* observer)
      : observer(observer) {}

  rtc::scoped_refptr<webrtc::CreateSessionDescriptionObserver> observer;
  std::string error;
};

struct GetStatsMsg : public rtc::MessageData {
  GetStatsMsg(webrtc::StatsObserver* observer,
              webrtc::MediaStreamTrackInterface* track)
      : observer(observer), track(track) {
  }
  rtc::scoped_refptr<webrtc::StatsObserver> observer;
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track;
};

// |in_str| should be of format
// stunURI       = scheme ":" stun-host [ ":" stun-port ]
// scheme        = "stun" / "stuns"
// stun-host     = IP-literal / IPv4address / reg-name
// stun-port     = *DIGIT
//
// draft-petithuguenin-behave-turn-uris-01
// turnURI       = scheme ":" turn-host [ ":" turn-port ]
// turn-host     = username@IP-literal / IPv4address / reg-name
bool GetServiceTypeAndHostnameFromUri(const std::string& in_str,
                                      ServiceType* service_type,
                                      std::string* hostname) {
  const std::string::size_type colonpos = in_str.find(':');
  if (colonpos == std::string::npos) {
    LOG(LS_WARNING) << "Missing ':' in ICE URI: " << in_str;
    return false;
  }
  if ((colonpos + 1) == in_str.length()) {
    LOG(LS_WARNING) << "Empty hostname in ICE URI: " << in_str;
    return false;
  }
  *service_type = INVALID;
  for (size_t i = 0; i < ARRAY_SIZE(kValidIceServiceTypes); ++i) {
    if (in_str.compare(0, colonpos, kValidIceServiceTypes[i]) == 0) {
      *service_type = static_cast<ServiceType>(i);
      break;
    }
  }
  if (*service_type == INVALID) {
    return false;
  }
  *hostname = in_str.substr(colonpos + 1, std::string::npos);
  return true;
}

bool ParsePort(const std::string& in_str, int* port) {
  // Make sure port only contains digits. FromString doesn't check this.
  for (const char& c : in_str) {
    if (!std::isdigit(c)) {
      return false;
    }
  }
  return rtc::FromString(in_str, port);
}

// This method parses IPv6 and IPv4 literal strings, along with hostnames in
// standard hostname:port format.
// Consider following formats as correct.
// |hostname:port|, |[IPV6 address]:port|, |IPv4 address|:port,
// |hostname|, |[IPv6 address]|, |IPv4 address|.
bool ParseHostnameAndPortFromString(const std::string& in_str,
                                    std::string* host,
                                    int* port) {
  RTC_DCHECK(host->empty());
  if (in_str.at(0) == '[') {
    std::string::size_type closebracket = in_str.rfind(']');
    if (closebracket != std::string::npos) {
      std::string::size_type colonpos = in_str.find(':', closebracket);
      if (std::string::npos != colonpos) {
        if (!ParsePort(in_str.substr(closebracket + 2, std::string::npos),
                       port)) {
          return false;
        }
      }
      *host = in_str.substr(1, closebracket - 1);
    } else {
      return false;
    }
  } else {
    std::string::size_type colonpos = in_str.find(':');
    if (std::string::npos != colonpos) {
      if (!ParsePort(in_str.substr(colonpos + 1, std::string::npos), port)) {
        return false;
      }
      *host = in_str.substr(0, colonpos);
    } else {
      *host = in_str;
    }
  }
  return !host->empty();
}

// Adds a StunConfiguration or TurnConfiguration to the appropriate list,
// by parsing |url| and using the username/password in |server|.
bool ParseIceServerUrl(const PeerConnectionInterface::IceServer& server,
                       const std::string& url,
                       StunConfigurations* stun_config,
                       TurnConfigurations* turn_config) {
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
  RTC_DCHECK(stun_config != nullptr);
  RTC_DCHECK(turn_config != nullptr);
  std::vector<std::string> tokens;
  std::string turn_transport_type = kUdpTransportType;
  RTC_DCHECK(!url.empty());
  rtc::tokenize(url, '?', &tokens);
  std::string uri_without_transport = tokens[0];
  // Let's look into transport= param, if it exists.
  if (tokens.size() == kTurnTransportTokensNum) {  // ?transport= is present.
    std::string uri_transport_param = tokens[1];
    rtc::tokenize(uri_transport_param, '=', &tokens);
    if (tokens[0] == kTransport) {
      // As per above grammar transport param will be consist of lower case
      // letters.
      if (tokens[1] != kUdpTransportType && tokens[1] != kTcpTransportType) {
        LOG(LS_WARNING) << "Transport param should always be udp or tcp.";
        return false;
      }
      turn_transport_type = tokens[1];
    }
  }

  std::string hoststring;
  ServiceType service_type;
  if (!GetServiceTypeAndHostnameFromUri(uri_without_transport,
                                       &service_type,
                                       &hoststring)) {
    LOG(LS_WARNING) << "Invalid transport parameter in ICE URI: " << url;
    return false;
  }

  // GetServiceTypeAndHostnameFromUri should never give an empty hoststring
  RTC_DCHECK(!hoststring.empty());

  // Let's break hostname.
  tokens.clear();
  rtc::tokenize_with_empty_tokens(hoststring, '@', &tokens);

  std::string username(server.username);
  if (tokens.size() > kTurnHostTokensNum) {
    LOG(LS_WARNING) << "Invalid user@hostname format: " << hoststring;
    return false;
  }
  if (tokens.size() == kTurnHostTokensNum) {
    if (tokens[0].empty() || tokens[1].empty()) {
      LOG(LS_WARNING) << "Invalid user@hostname format: " << hoststring;
      return false;
    }
    username.assign(rtc::s_url_decode(tokens[0]));
    hoststring = tokens[1];
  } else {
    hoststring = tokens[0];
  }

  int port = kDefaultStunPort;
  if (service_type == TURNS) {
    port = kDefaultStunTlsPort;
    turn_transport_type = kTcpTransportType;
  }

  std::string address;
  if (!ParseHostnameAndPortFromString(hoststring, &address, &port)) {
    LOG(WARNING) << "Invalid hostname format: " << uri_without_transport;
    return false;
  }

  if (port <= 0 || port > 0xffff) {
    LOG(WARNING) << "Invalid port: " << port;
    return false;
  }

  switch (service_type) {
    case STUN:
    case STUNS:
      stun_config->push_back(StunConfiguration(address, port));
      break;
    case TURN:
    case TURNS: {
      bool secure = (service_type == TURNS);
      turn_config->push_back(TurnConfiguration(address, port,
                                               username,
                                               server.password,
                                               turn_transport_type,
                                               secure));
      break;
    }
    case INVALID:
    default:
      LOG(WARNING) << "Configuration not supported: " << url;
      return false;
  }
  return true;
}

// Check if we can send |new_stream| on a PeerConnection.
bool CanAddLocalMediaStream(webrtc::StreamCollectionInterface* current_streams,
                            webrtc::MediaStreamInterface* new_stream) {
  if (!new_stream || !current_streams) {
    return false;
  }
  if (current_streams->find(new_stream->label()) != nullptr) {
    LOG(LS_ERROR) << "MediaStream with label " << new_stream->label()
                  << " is already added.";
    return false;
  }
  return true;
}

bool MediaContentDirectionHasSend(cricket::MediaContentDirection dir) {
  return dir == cricket::MD_SENDONLY || dir == cricket::MD_SENDRECV;
}

// If the direction is "recvonly" or "inactive", treat the description
// as containing no streams.
// See: https://code.google.com/p/webrtc/issues/detail?id=5054
std::vector<cricket::StreamParams> GetActiveStreams(
    const cricket::MediaContentDescription* desc) {
  return MediaContentDirectionHasSend(desc->direction())
             ? desc->streams()
             : std::vector<cricket::StreamParams>();
}

bool IsValidOfferToReceiveMedia(int value) {
  typedef PeerConnectionInterface::RTCOfferAnswerOptions Options;
  return (value >= Options::kUndefined) &&
         (value <= Options::kMaxOfferToReceiveMedia);
}

// Add the stream and RTP data channel info to |session_options|.
void SetStreams(cricket::MediaSessionOptions* session_options,
                rtc::scoped_refptr<StreamCollection> streams,
                const std::map<std::string, rtc::scoped_refptr<DataChannel>>&
                    rtp_data_channels) {
  session_options->streams.clear();
  if (streams != nullptr) {
    for (size_t i = 0; i < streams->count(); ++i) {
      MediaStreamInterface* stream = streams->at(i);
      // For each audio track in the stream, add it to the MediaSessionOptions.
      for (const auto& track : stream->GetAudioTracks()) {
        session_options->AddSendStream(cricket::MEDIA_TYPE_AUDIO, track->id(),
                                       stream->label());
      }
      // For each video track in the stream, add it to the MediaSessionOptions.
      for (const auto& track : stream->GetVideoTracks()) {
        session_options->AddSendStream(cricket::MEDIA_TYPE_VIDEO, track->id(),
                                       stream->label());
      }
    }
  }

  // Check for data channels.
  for (const auto& kv : rtp_data_channels) {
    const DataChannel* channel = kv.second;
    if (channel->state() == DataChannel::kConnecting ||
        channel->state() == DataChannel::kOpen) {
      // |streamid| and |sync_label| are both set to the DataChannel label
      // here so they can be signaled the same way as MediaStreams and Tracks.
      // For MediaStreams, the sync_label is the MediaStream label and the
      // track label is the same as |streamid|.
      const std::string& streamid = channel->label();
      const std::string& sync_label = channel->label();
      session_options->AddSendStream(cricket::MEDIA_TYPE_DATA, streamid,
                                     sync_label);
    }
  }
}

}  // namespace

namespace webrtc {

// Factory class for creating remote MediaStreams and MediaStreamTracks.
class RemoteMediaStreamFactory {
 public:
  explicit RemoteMediaStreamFactory(rtc::Thread* signaling_thread,
                                    cricket::ChannelManager* channel_manager)
      : signaling_thread_(signaling_thread),
        channel_manager_(channel_manager) {}

  rtc::scoped_refptr<MediaStreamInterface> CreateMediaStream(
      const std::string& stream_label) {
    return MediaStreamProxy::Create(signaling_thread_,
                                    MediaStream::Create(stream_label));
  }

  AudioTrackInterface* AddAudioTrack(webrtc::MediaStreamInterface* stream,
                                     const std::string& track_id) {
    return AddTrack<AudioTrackInterface, AudioTrack, AudioTrackProxy>(
        stream, track_id, RemoteAudioSource::Create().get());
  }

  VideoTrackInterface* AddVideoTrack(webrtc::MediaStreamInterface* stream,
                                     const std::string& track_id) {
    return AddTrack<VideoTrackInterface, VideoTrack, VideoTrackProxy>(
        stream, track_id,
        VideoSource::Create(channel_manager_, new RemoteVideoCapturer(),
                            nullptr)
            .get());
  }

 private:
  template <typename TI, typename T, typename TP, typename S>
  TI* AddTrack(MediaStreamInterface* stream,
               const std::string& track_id,
               S* source) {
    rtc::scoped_refptr<TI> track(
        TP::Create(signaling_thread_, T::Create(track_id, source)));
    track->set_state(webrtc::MediaStreamTrackInterface::kLive);
    if (stream->AddTrack(track)) {
      return track;
    }
    return nullptr;
  }

  rtc::Thread* signaling_thread_;
  cricket::ChannelManager* channel_manager_;
};

bool ConvertRtcOptionsForOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options) {
  typedef PeerConnectionInterface::RTCOfferAnswerOptions RTCOfferAnswerOptions;
  if (!IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_audio) ||
      !IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_video)) {
    return false;
  }

  if (rtc_options.offer_to_receive_audio != RTCOfferAnswerOptions::kUndefined) {
    session_options->recv_audio = (rtc_options.offer_to_receive_audio > 0);
  }
  if (rtc_options.offer_to_receive_video != RTCOfferAnswerOptions::kUndefined) {
    session_options->recv_video = (rtc_options.offer_to_receive_video > 0);
  }

  session_options->vad_enabled = rtc_options.voice_activity_detection;
  session_options->transport_options.ice_restart = rtc_options.ice_restart;
  session_options->bundle_enabled = rtc_options.use_rtp_mux;

  return true;
}

bool ParseConstraintsForAnswer(const MediaConstraintsInterface* constraints,
                               cricket::MediaSessionOptions* session_options) {
  bool value = false;
  size_t mandatory_constraints_satisfied = 0;

  // kOfferToReceiveAudio defaults to true according to spec.
  if (!FindConstraint(constraints,
                      MediaConstraintsInterface::kOfferToReceiveAudio, &value,
                      &mandatory_constraints_satisfied) ||
      value) {
    session_options->recv_audio = true;
  }

  // kOfferToReceiveVideo defaults to false according to spec. But
  // if it is an answer and video is offered, we should still accept video
  // per default.
  value = false;
  if (!FindConstraint(constraints,
                      MediaConstraintsInterface::kOfferToReceiveVideo, &value,
                      &mandatory_constraints_satisfied) ||
      value) {
    session_options->recv_video = true;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kVoiceActivityDetection, &value,
                     &mandatory_constraints_satisfied)) {
    session_options->vad_enabled = value;
  }

  if (FindConstraint(constraints, MediaConstraintsInterface::kUseRtpMux, &value,
                     &mandatory_constraints_satisfied)) {
    session_options->bundle_enabled = value;
  } else {
    // kUseRtpMux defaults to true according to spec.
    session_options->bundle_enabled = true;
  }

  if (FindConstraint(constraints, MediaConstraintsInterface::kIceRestart,
                     &value, &mandatory_constraints_satisfied)) {
    session_options->transport_options.ice_restart = value;
  } else {
    // kIceRestart defaults to false according to spec.
    session_options->transport_options.ice_restart = false;
  }

  if (!constraints) {
    return true;
  }
  return mandatory_constraints_satisfied == constraints->GetMandatory().size();
}

bool ParseIceServers(const PeerConnectionInterface::IceServers& servers,
                     StunConfigurations* stun_config,
                     TurnConfigurations* turn_config) {
  for (const webrtc::PeerConnectionInterface::IceServer& server : servers) {
    if (!server.urls.empty()) {
      for (const std::string& url : server.urls) {
        if (url.empty()) {
          LOG(LS_ERROR) << "Empty uri.";
          return false;
        }
        if (!ParseIceServerUrl(server, url, stun_config, turn_config)) {
          return false;
        }
      }
    } else if (!server.uri.empty()) {
      // Fallback to old .uri if new .urls isn't present.
      if (!ParseIceServerUrl(server, server.uri, stun_config, turn_config)) {
        return false;
      }
    } else {
      LOG(LS_ERROR) << "Empty uri.";
      return false;
    }
  }
  return true;
}

PeerConnection::PeerConnection(PeerConnectionFactory* factory)
    : factory_(factory),
      observer_(NULL),
      uma_observer_(NULL),
      signaling_state_(kStable),
      ice_state_(kIceNew),
      ice_connection_state_(kIceConnectionNew),
      ice_gathering_state_(kIceGatheringNew),
      local_streams_(StreamCollection::Create()),
      remote_streams_(StreamCollection::Create()) {}

PeerConnection::~PeerConnection() {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  // Need to detach RTP senders/receivers from WebRtcSession,
  // since it's about to be destroyed.
  for (const auto& sender : senders_) {
    sender->Stop();
  }
  for (const auto& receiver : receivers_) {
    receiver->Stop();
  }
}

bool PeerConnection::Initialize(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    const MediaConstraintsInterface* constraints,
    PortAllocatorFactoryInterface* allocator_factory,
    rtc::scoped_ptr<DtlsIdentityStoreInterface> dtls_identity_store,
    PeerConnectionObserver* observer) {
  RTC_DCHECK(observer != nullptr);
  if (!observer) {
    return false;
  }
  observer_ = observer;

  std::vector<PortAllocatorFactoryInterface::StunConfiguration> stun_config;
  std::vector<PortAllocatorFactoryInterface::TurnConfiguration> turn_config;
  if (!ParseIceServers(configuration.servers, &stun_config, &turn_config)) {
    return false;
  }
  port_allocator_.reset(
      allocator_factory->CreatePortAllocator(stun_config, turn_config));

  // To handle both internal and externally created port allocator, we will
  // enable BUNDLE here.
  int portallocator_flags = port_allocator_->flags();
  portallocator_flags |= cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                         cricket::PORTALLOCATOR_ENABLE_IPV6;
  bool value;
  // If IPv6 flag was specified, we'll not override it by experiment.
  if (FindConstraint(constraints, MediaConstraintsInterface::kEnableIPv6,
                     &value, nullptr)) {
    if (!value) {
      portallocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
    }
  } else if (webrtc::field_trial::FindFullName("WebRTC-IPv6Default") ==
             "Disabled") {
    portallocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
  }

  if (configuration.tcp_candidate_policy == kTcpCandidatePolicyDisabled) {
    portallocator_flags |= cricket::PORTALLOCATOR_DISABLE_TCP;
    LOG(LS_INFO) << "TCP candidates are disabled.";
  }

  port_allocator_->set_flags(portallocator_flags);
  // No step delay is used while allocating ports.
  port_allocator_->set_step_delay(cricket::kMinimumStepDelay);

  media_controller_.reset(factory_->CreateMediaController());

  remote_stream_factory_.reset(new RemoteMediaStreamFactory(
      factory_->signaling_thread(), media_controller_->channel_manager()));

  session_.reset(
      new WebRtcSession(media_controller_.get(), factory_->signaling_thread(),
                        factory_->worker_thread(), port_allocator_.get()));
  stats_.reset(new StatsCollector(this));

  // Initialize the WebRtcSession. It creates transport channels etc.
  if (!session_->Initialize(factory_->options(), constraints,
                            dtls_identity_store.Pass(), configuration)) {
    return false;
  }

  // Register PeerConnection as receiver of local ice candidates.
  // All the callbacks will be posted to the application from PeerConnection.
  session_->RegisterIceObserver(this);
  session_->SignalState.connect(this, &PeerConnection::OnSessionStateChange);
  session_->SignalVoiceChannelDestroyed.connect(
      this, &PeerConnection::OnVoiceChannelDestroyed);
  session_->SignalVideoChannelDestroyed.connect(
      this, &PeerConnection::OnVideoChannelDestroyed);
  session_->SignalDataChannelCreated.connect(
      this, &PeerConnection::OnDataChannelCreated);
  session_->SignalDataChannelDestroyed.connect(
      this, &PeerConnection::OnDataChannelDestroyed);
  session_->SignalDataChannelOpenMessage.connect(
      this, &PeerConnection::OnDataChannelOpenMessage);
  return true;
}

rtc::scoped_refptr<StreamCollectionInterface>
PeerConnection::local_streams() {
  return local_streams_;
}

rtc::scoped_refptr<StreamCollectionInterface>
PeerConnection::remote_streams() {
  return remote_streams_;
}

// TODO(deadbeef): Create RtpSenders immediately here, even if local
// description hasn't yet been set.
bool PeerConnection::AddStream(MediaStreamInterface* local_stream) {
  if (IsClosed()) {
    return false;
  }
  if (!CanAddLocalMediaStream(local_streams_, local_stream)) {
    return false;
  }

  local_streams_->AddStream(local_stream);

  // Find tracks that have already been configured in SDP. This can occur if a
  // local session description that contains the MSID of these tracks is set
  // before AddLocalStream is called. It can also occur if the local session
  // description is not changed and RemoveLocalStream  is called and later
  // AddLocalStream is called again with the same stream.
  for (const auto& track : local_stream->GetAudioTracks()) {
    const TrackInfo* track_info =
        FindTrackInfo(local_audio_tracks_, local_stream->label(), track->id());
    if (track_info) {
      CreateAudioSender(local_stream, track.get(), track_info->ssrc);
    }
  }
  for (const auto& track : local_stream->GetVideoTracks()) {
    const TrackInfo* track_info =
        FindTrackInfo(local_video_tracks_, local_stream->label(), track->id());
    if (track_info) {
      CreateVideoSender(local_stream, track.get(), track_info->ssrc);
    }
  }

  stats_->AddStream(local_stream);
  observer_->OnRenegotiationNeeded();
  return true;
}

// TODO(deadbeef): Don't destroy RtpSenders here; they should be kept around
// indefinitely.
void PeerConnection::RemoveStream(MediaStreamInterface* local_stream) {
  for (const auto& track : local_stream->GetAudioTracks()) {
    const TrackInfo* track_info =
        FindTrackInfo(local_audio_tracks_, local_stream->label(), track->id());
    if (track_info) {
      DestroyAudioSender(local_stream, track.get(), track_info->ssrc);
    }
  }
  for (const auto& track : local_stream->GetVideoTracks()) {
    const TrackInfo* track_info =
        FindTrackInfo(local_video_tracks_, local_stream->label(), track->id());
    if (track_info) {
      DestroyVideoSender(local_stream, track.get());
    }
  }

  local_streams_->RemoveStream(local_stream);

  if (IsClosed()) {
    return;
  }
  observer_->OnRenegotiationNeeded();
}

rtc::scoped_refptr<DtmfSenderInterface> PeerConnection::CreateDtmfSender(
    AudioTrackInterface* track) {
  if (!track) {
    LOG(LS_ERROR) << "CreateDtmfSender - track is NULL.";
    return NULL;
  }
  if (!local_streams_->FindAudioTrack(track->id())) {
    LOG(LS_ERROR) << "CreateDtmfSender is called with a non local audio track.";
    return NULL;
  }

  rtc::scoped_refptr<DtmfSenderInterface> sender(
      DtmfSender::Create(track, signaling_thread(), session_.get()));
  if (!sender.get()) {
    LOG(LS_ERROR) << "CreateDtmfSender failed on DtmfSender::Create.";
    return NULL;
  }
  return DtmfSenderProxy::Create(signaling_thread(), sender.get());
}

std::vector<rtc::scoped_refptr<RtpSenderInterface>> PeerConnection::GetSenders()
    const {
  std::vector<rtc::scoped_refptr<RtpSenderInterface>> senders;
  for (const auto& sender : senders_) {
    senders.push_back(RtpSenderProxy::Create(signaling_thread(), sender.get()));
  }
  return senders;
}

std::vector<rtc::scoped_refptr<RtpReceiverInterface>>
PeerConnection::GetReceivers() const {
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> receivers;
  for (const auto& receiver : receivers_) {
    receivers.push_back(
        RtpReceiverProxy::Create(signaling_thread(), receiver.get()));
  }
  return receivers;
}

bool PeerConnection::GetStats(StatsObserver* observer,
                              MediaStreamTrackInterface* track,
                              StatsOutputLevel level) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (!VERIFY(observer != NULL)) {
    LOG(LS_ERROR) << "GetStats - observer is NULL.";
    return false;
  }

  stats_->UpdateStats(level);
  signaling_thread()->Post(this, MSG_GETSTATS,
                           new GetStatsMsg(observer, track));
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

rtc::scoped_refptr<DataChannelInterface>
PeerConnection::CreateDataChannel(
    const std::string& label,
    const DataChannelInit* config) {
  bool first_datachannel = !HasDataChannels();

  rtc::scoped_ptr<InternalDataChannelInit> internal_config;
  if (config) {
    internal_config.reset(new InternalDataChannelInit(*config));
  }
  rtc::scoped_refptr<DataChannelInterface> channel(
      InternalCreateDataChannel(label, internal_config.get()));
  if (!channel.get()) {
    return nullptr;
  }

  // Trigger the onRenegotiationNeeded event for every new RTP DataChannel, or
  // the first SCTP DataChannel.
  if (session_->data_channel_type() == cricket::DCT_RTP || first_datachannel) {
    observer_->OnRenegotiationNeeded();
  }

  return DataChannelProxy::Create(signaling_thread(), channel.get());
}

void PeerConnection::CreateOffer(CreateSessionDescriptionObserver* observer,
                                 const MediaConstraintsInterface* constraints) {
  if (!VERIFY(observer != nullptr)) {
    LOG(LS_ERROR) << "CreateOffer - observer is NULL.";
    return;
  }
  RTCOfferAnswerOptions options;

  bool value;
  size_t mandatory_constraints = 0;

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kOfferToReceiveAudio,
                     &value,
                     &mandatory_constraints)) {
    options.offer_to_receive_audio =
        value ? RTCOfferAnswerOptions::kOfferToReceiveMediaTrue : 0;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kOfferToReceiveVideo,
                     &value,
                     &mandatory_constraints)) {
    options.offer_to_receive_video =
        value ? RTCOfferAnswerOptions::kOfferToReceiveMediaTrue : 0;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kVoiceActivityDetection,
                     &value,
                     &mandatory_constraints)) {
    options.voice_activity_detection = value;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kIceRestart,
                     &value,
                     &mandatory_constraints)) {
    options.ice_restart = value;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kUseRtpMux,
                     &value,
                     &mandatory_constraints)) {
    options.use_rtp_mux = value;
  }

  CreateOffer(observer, options);
}

void PeerConnection::CreateOffer(CreateSessionDescriptionObserver* observer,
                                 const RTCOfferAnswerOptions& options) {
  if (!VERIFY(observer != nullptr)) {
    LOG(LS_ERROR) << "CreateOffer - observer is NULL.";
    return;
  }

  cricket::MediaSessionOptions session_options;
  if (!GetOptionsForOffer(options, &session_options)) {
    std::string error = "CreateOffer called with invalid options.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(observer, error);
    return;
  }

  session_->CreateOffer(observer, options, session_options);
}

void PeerConnection::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const MediaConstraintsInterface* constraints) {
  if (!VERIFY(observer != nullptr)) {
    LOG(LS_ERROR) << "CreateAnswer - observer is NULL.";
    return;
  }

  cricket::MediaSessionOptions session_options;
  if (!GetOptionsForAnswer(constraints, &session_options)) {
    std::string error = "CreateAnswer called with invalid constraints.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(observer, error);
    return;
  }

  session_->CreateAnswer(observer, constraints, session_options);
}

void PeerConnection::SetLocalDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  if (!VERIFY(observer != nullptr)) {
    LOG(LS_ERROR) << "SetLocalDescription - observer is NULL.";
    return;
  }
  if (!desc) {
    PostSetSessionDescriptionFailure(observer, "SessionDescription is NULL.");
    return;
  }
  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  stats_->UpdateStats(kStatsOutputLevelStandard);
  std::string error;
  if (!session_->SetLocalDescription(desc, &error)) {
    PostSetSessionDescriptionFailure(observer, error);
    return;
  }

  // If setting the description decided our SSL role, allocate any necessary
  // SCTP sids.
  rtc::SSLRole role;
  if (session_->data_channel_type() == cricket::DCT_SCTP &&
      session_->GetSslRole(&role)) {
    AllocateSctpSids(role);
  }

  // Update state and SSRC of local MediaStreams and DataChannels based on the
  // local session description.
  const cricket::ContentInfo* audio_content =
      GetFirstAudioContent(desc->description());
  if (audio_content) {
    const cricket::AudioContentDescription* audio_desc =
        static_cast<const cricket::AudioContentDescription*>(
            audio_content->description);
    UpdateLocalTracks(audio_desc->streams(), audio_desc->type());
  }

  const cricket::ContentInfo* video_content =
      GetFirstVideoContent(desc->description());
  if (video_content) {
    const cricket::VideoContentDescription* video_desc =
        static_cast<const cricket::VideoContentDescription*>(
            video_content->description);
    UpdateLocalTracks(video_desc->streams(), video_desc->type());
  }

  const cricket::ContentInfo* data_content =
      GetFirstDataContent(desc->description());
  if (data_content) {
    const cricket::DataContentDescription* data_desc =
        static_cast<const cricket::DataContentDescription*>(
            data_content->description);
    if (rtc::starts_with(data_desc->protocol().data(),
                         cricket::kMediaProtocolRtpPrefix)) {
      UpdateLocalRtpDataChannels(data_desc->streams());
    }
  }

  SetSessionDescriptionMsg* msg = new SetSessionDescriptionMsg(observer);
  signaling_thread()->Post(this, MSG_SET_SESSIONDESCRIPTION_SUCCESS, msg);

  // MaybeStartGathering needs to be called after posting
  // MSG_SET_SESSIONDESCRIPTION_SUCCESS, so that we don't signal any candidates
  // before signaling that SetLocalDescription completed.
  session_->MaybeStartGathering();
}

void PeerConnection::SetRemoteDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  if (!VERIFY(observer != nullptr)) {
    LOG(LS_ERROR) << "SetRemoteDescription - observer is NULL.";
    return;
  }
  if (!desc) {
    PostSetSessionDescriptionFailure(observer, "SessionDescription is NULL.");
    return;
  }
  // Update stats here so that we have the most recent stats for tracks and
  // streams that might be removed by updating the session description.
  stats_->UpdateStats(kStatsOutputLevelStandard);
  std::string error;
  if (!session_->SetRemoteDescription(desc, &error)) {
    PostSetSessionDescriptionFailure(observer, error);
    return;
  }

  // If setting the description decided our SSL role, allocate any necessary
  // SCTP sids.
  rtc::SSLRole role;
  if (session_->data_channel_type() == cricket::DCT_SCTP &&
      session_->GetSslRole(&role)) {
    AllocateSctpSids(role);
  }

  const cricket::SessionDescription* remote_desc = desc->description();

  // We wait to signal new streams until we finish processing the description,
  // since only at that point will new streams have all their tracks.
  rtc::scoped_refptr<StreamCollection> new_streams(StreamCollection::Create());

  // Find all audio rtp streams and create corresponding remote AudioTracks
  // and MediaStreams.
  const cricket::ContentInfo* audio_content = GetFirstAudioContent(remote_desc);
  if (audio_content) {
    const cricket::AudioContentDescription* desc =
        static_cast<const cricket::AudioContentDescription*>(
            audio_content->description);
    UpdateRemoteStreamsList(GetActiveStreams(desc), desc->type(), new_streams);
    remote_info_.default_audio_track_needed =
        !remote_desc->msid_supported() && desc->streams().empty() &&
        MediaContentDirectionHasSend(desc->direction());
  }

  // Find all video rtp streams and create corresponding remote VideoTracks
  // and MediaStreams.
  const cricket::ContentInfo* video_content = GetFirstVideoContent(remote_desc);
  if (video_content) {
    const cricket::VideoContentDescription* desc =
        static_cast<const cricket::VideoContentDescription*>(
            video_content->description);
    UpdateRemoteStreamsList(GetActiveStreams(desc), desc->type(), new_streams);
    remote_info_.default_video_track_needed =
        !remote_desc->msid_supported() && desc->streams().empty() &&
        MediaContentDirectionHasSend(desc->direction());
  }

  // Update the DataChannels with the information from the remote peer.
  const cricket::ContentInfo* data_content = GetFirstDataContent(remote_desc);
  if (data_content) {
    const cricket::DataContentDescription* desc =
        static_cast<const cricket::DataContentDescription*>(
            data_content->description);
    if (rtc::starts_with(desc->protocol().data(),
                         cricket::kMediaProtocolRtpPrefix)) {
      UpdateRemoteRtpDataChannels(GetActiveStreams(desc));
    }
  }

  // Iterate new_streams and notify the observer about new MediaStreams.
  for (size_t i = 0; i < new_streams->count(); ++i) {
    MediaStreamInterface* new_stream = new_streams->at(i);
    stats_->AddStream(new_stream);
    observer_->OnAddStream(new_stream);
  }

  // Find removed MediaStreams.
  if (remote_info_.IsDefaultMediaStreamNeeded() &&
      remote_streams_->find(kDefaultStreamLabel) != nullptr) {
    // The default media stream already exists. No need to do anything.
  } else {
    UpdateEndedRemoteMediaStreams();
    remote_info_.msid_supported |= remote_streams_->count() > 0;
  }
  MaybeCreateDefaultStream();

  SetSessionDescriptionMsg* msg = new SetSessionDescriptionMsg(observer);
  signaling_thread()->Post(this, MSG_SET_SESSIONDESCRIPTION_SUCCESS, msg);
}

bool PeerConnection::SetConfiguration(const RTCConfiguration& config) {
  if (port_allocator_) {
    std::vector<PortAllocatorFactoryInterface::StunConfiguration> stuns;
    std::vector<PortAllocatorFactoryInterface::TurnConfiguration> turns;
    if (!ParseIceServers(config.servers, &stuns, &turns)) {
      return false;
    }

    std::vector<rtc::SocketAddress> stun_hosts;
    typedef std::vector<StunConfiguration>::const_iterator StunIt;
    for (StunIt stun_it = stuns.begin(); stun_it != stuns.end(); ++stun_it) {
      stun_hosts.push_back(stun_it->server);
    }

    rtc::SocketAddress stun_addr;
    if (!stun_hosts.empty()) {
      stun_addr = stun_hosts.front();
      LOG(LS_INFO) << "SetConfiguration: StunServer Address: "
                   << stun_addr.ToString();
    }

    for (size_t i = 0; i < turns.size(); ++i) {
      cricket::RelayCredentials credentials(turns[i].username,
                                            turns[i].password);
      cricket::RelayServerConfig relay_server(cricket::RELAY_TURN);
      cricket::ProtocolType protocol;
      if (cricket::StringToProto(turns[i].transport_type.c_str(), &protocol)) {
        relay_server.ports.push_back(cricket::ProtocolAddress(
            turns[i].server, protocol, turns[i].secure));
        relay_server.credentials = credentials;
        LOG(LS_INFO) << "SetConfiguration: TurnServer Address: "
                     << turns[i].server.ToString();
      } else {
        LOG(LS_WARNING) << "Ignoring TURN server " << turns[i].server << ". "
                        << "Reason= Incorrect " << turns[i].transport_type
                        << " transport parameter.";
      }
    }
  }
  session_->SetIceConfig(session_->ParseIceConfig(config));
  return session_->SetIceTransports(config.type);
}

bool PeerConnection::AddIceCandidate(
    const IceCandidateInterface* ice_candidate) {
  return session_->ProcessIceMessage(ice_candidate);
}

void PeerConnection::RegisterUMAObserver(UMAObserver* observer) {
  uma_observer_ = observer;

  if (session_) {
    session_->set_metrics_observer(uma_observer_);
  }

  // Send information about IPv4/IPv6 status.
  if (uma_observer_ && port_allocator_) {
    if (port_allocator_->flags() & cricket::PORTALLOCATOR_ENABLE_IPV6) {
      uma_observer_->IncrementEnumCounter(
          kEnumCounterAddressFamily, kPeerConnection_IPv6,
          kPeerConnectionAddressFamilyCounter_Max);
    } else {
      uma_observer_->IncrementEnumCounter(
          kEnumCounterAddressFamily, kPeerConnection_IPv4,
          kPeerConnectionAddressFamilyCounter_Max);
    }
  }
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
  stats_->UpdateStats(kStatsOutputLevelStandard);

  session_->Close();
}

void PeerConnection::OnSessionStateChange(WebRtcSession* /*session*/,
                                          WebRtcSession::State state) {
  switch (state) {
    case WebRtcSession::STATE_INIT:
      ChangeSignalingState(PeerConnectionInterface::kStable);
      break;
    case WebRtcSession::STATE_SENTOFFER:
      ChangeSignalingState(PeerConnectionInterface::kHaveLocalOffer);
      break;
    case WebRtcSession::STATE_SENTPRANSWER:
      ChangeSignalingState(PeerConnectionInterface::kHaveLocalPrAnswer);
      break;
    case WebRtcSession::STATE_RECEIVEDOFFER:
      ChangeSignalingState(PeerConnectionInterface::kHaveRemoteOffer);
      break;
    case WebRtcSession::STATE_RECEIVEDPRANSWER:
      ChangeSignalingState(PeerConnectionInterface::kHaveRemotePrAnswer);
      break;
    case WebRtcSession::STATE_INPROGRESS:
      ChangeSignalingState(PeerConnectionInterface::kStable);
      break;
    case WebRtcSession::STATE_CLOSED:
      ChangeSignalingState(PeerConnectionInterface::kClosed);
      break;
    default:
      break;
  }
}

void PeerConnection::OnMessage(rtc::Message* msg) {
  switch (msg->message_id) {
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
    case MSG_CREATE_SESSIONDESCRIPTION_FAILED: {
      CreateSessionDescriptionMsg* param =
          static_cast<CreateSessionDescriptionMsg*>(msg->pdata);
      param->observer->OnFailure(param->error);
      delete param;
      break;
    }
    case MSG_GETSTATS: {
      GetStatsMsg* param = static_cast<GetStatsMsg*>(msg->pdata);
      StatsReports reports;
      stats_->GetStats(param->track, &reports);
      param->observer->OnComplete(reports);
      delete param;
      break;
    }
    default:
      RTC_DCHECK(false && "Not implemented");
      break;
  }
}

void PeerConnection::CreateAudioReceiver(MediaStreamInterface* stream,
                                         AudioTrackInterface* audio_track,
                                         uint32_t ssrc) {
  receivers_.push_back(new AudioRtpReceiver(audio_track, ssrc, session_.get()));
}

void PeerConnection::CreateVideoReceiver(MediaStreamInterface* stream,
                                         VideoTrackInterface* video_track,
                                         uint32_t ssrc) {
  receivers_.push_back(new VideoRtpReceiver(video_track, ssrc, session_.get()));
}

// TODO(deadbeef): Keep RtpReceivers around even if track goes away in remote
// description.
void PeerConnection::DestroyAudioReceiver(MediaStreamInterface* stream,
                                          AudioTrackInterface* audio_track) {
  auto it = FindReceiverForTrack(audio_track);
  if (it == receivers_.end()) {
    LOG(LS_WARNING) << "RtpReceiver for track with id " << audio_track->id()
                    << " doesn't exist.";
  } else {
    (*it)->Stop();
    receivers_.erase(it);
  }
}

void PeerConnection::DestroyVideoReceiver(MediaStreamInterface* stream,
                                          VideoTrackInterface* video_track) {
  auto it = FindReceiverForTrack(video_track);
  if (it == receivers_.end()) {
    LOG(LS_WARNING) << "RtpReceiver for track with id " << video_track->id()
                    << " doesn't exist.";
  } else {
    (*it)->Stop();
    receivers_.erase(it);
  }
}

void PeerConnection::CreateAudioSender(MediaStreamInterface* stream,
                                       AudioTrackInterface* audio_track,
                                       uint32_t ssrc) {
  senders_.push_back(new AudioRtpSender(audio_track, ssrc, session_.get()));
  stats_->AddLocalAudioTrack(audio_track, ssrc);
}

void PeerConnection::CreateVideoSender(MediaStreamInterface* stream,
                                       VideoTrackInterface* video_track,
                                       uint32_t ssrc) {
  senders_.push_back(new VideoRtpSender(video_track, ssrc, session_.get()));
}

// TODO(deadbeef): Keep RtpSenders around even if track goes away in local
// description.
void PeerConnection::DestroyAudioSender(MediaStreamInterface* stream,
                                        AudioTrackInterface* audio_track,
                                        uint32_t ssrc) {
  auto it = FindSenderForTrack(audio_track);
  if (it == senders_.end()) {
    LOG(LS_WARNING) << "RtpSender for track with id " << audio_track->id()
                    << " doesn't exist.";
    return;
  } else {
    (*it)->Stop();
    senders_.erase(it);
  }
  stats_->RemoveLocalAudioTrack(audio_track, ssrc);
}

void PeerConnection::DestroyVideoSender(MediaStreamInterface* stream,
                                        VideoTrackInterface* video_track) {
  auto it = FindSenderForTrack(video_track);
  if (it == senders_.end()) {
    LOG(LS_WARNING) << "RtpSender for track with id " << video_track->id()
                    << " doesn't exist.";
    return;
  } else {
    (*it)->Stop();
    senders_.erase(it);
  }
}

void PeerConnection::OnIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  // After transitioning to "closed", ignore any additional states from
  // WebRtcSession (such as "disconnected").
  if (IsClosed()) {
    return;
  }
  ice_connection_state_ = new_state;
  observer_->OnIceConnectionChange(ice_connection_state_);
}

void PeerConnection::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
  ice_gathering_state_ = new_state;
  observer_->OnIceGatheringChange(ice_gathering_state_);
}

void PeerConnection::OnIceCandidate(const IceCandidateInterface* candidate) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  observer_->OnIceCandidate(candidate);
}

void PeerConnection::OnIceComplete() {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  observer_->OnIceComplete();
}

void PeerConnection::OnIceConnectionReceivingChange(bool receiving) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  observer_->OnIceConnectionReceivingChange(receiving);
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

void PeerConnection::PostSetSessionDescriptionFailure(
    SetSessionDescriptionObserver* observer,
    const std::string& error) {
  SetSessionDescriptionMsg* msg = new SetSessionDescriptionMsg(observer);
  msg->error = error;
  signaling_thread()->Post(this, MSG_SET_SESSIONDESCRIPTION_FAILED, msg);
}

void PeerConnection::PostCreateSessionDescriptionFailure(
    CreateSessionDescriptionObserver* observer,
    const std::string& error) {
  CreateSessionDescriptionMsg* msg = new CreateSessionDescriptionMsg(observer);
  msg->error = error;
  signaling_thread()->Post(this, MSG_CREATE_SESSIONDESCRIPTION_FAILED, msg);
}

bool PeerConnection::GetOptionsForOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options) {
  if (!ConvertRtcOptionsForOffer(rtc_options, session_options)) {
    return false;
  }

  SetStreams(session_options, local_streams_, rtp_data_channels_);
  // Offer to receive audio/video if the constraint is not set and there are
  // send streams, or we're currently receiving.
  if (rtc_options.offer_to_receive_audio == RTCOfferAnswerOptions::kUndefined) {
    session_options->recv_audio =
        session_options->HasSendMediaStream(cricket::MEDIA_TYPE_AUDIO) ||
        !remote_audio_tracks_.empty();
  }
  if (rtc_options.offer_to_receive_video == RTCOfferAnswerOptions::kUndefined) {
    session_options->recv_video =
        session_options->HasSendMediaStream(cricket::MEDIA_TYPE_VIDEO) ||
        !remote_video_tracks_.empty();
  }
  session_options->bundle_enabled =
      session_options->bundle_enabled &&
      (session_options->has_audio() || session_options->has_video() ||
       session_options->has_data());

  if (session_->data_channel_type() == cricket::DCT_SCTP && HasDataChannels()) {
    session_options->data_channel_type = cricket::DCT_SCTP;
  }
  return true;
}

bool PeerConnection::GetOptionsForAnswer(
    const MediaConstraintsInterface* constraints,
    cricket::MediaSessionOptions* session_options) {
  session_options->recv_audio = false;
  session_options->recv_video = false;
  if (!ParseConstraintsForAnswer(constraints, session_options)) {
    return false;
  }

  SetStreams(session_options, local_streams_, rtp_data_channels_);
  session_options->bundle_enabled =
      session_options->bundle_enabled &&
      (session_options->has_audio() || session_options->has_video() ||
       session_options->has_data());

  // RTP data channel is handled in MediaSessionOptions::AddStream. SCTP streams
  // are not signaled in the SDP so does not go through that path and must be
  // handled here.
  if (session_->data_channel_type() == cricket::DCT_SCTP) {
    session_options->data_channel_type = cricket::DCT_SCTP;
  }
  return true;
}

void PeerConnection::UpdateRemoteStreamsList(
    const cricket::StreamParamsVec& streams,
    cricket::MediaType media_type,
    StreamCollection* new_streams) {
  TrackInfos* current_tracks = GetRemoteTracks(media_type);

  // Find removed tracks. I.e., tracks where the track id or ssrc don't match
  // the
  // new StreamParam.
  auto track_it = current_tracks->begin();
  while (track_it != current_tracks->end()) {
    const TrackInfo& info = *track_it;
    const cricket::StreamParams* params =
        cricket::GetStreamBySsrc(streams, info.ssrc);
    if (!params || params->id != info.track_id) {
      OnRemoteTrackRemoved(info.stream_label, info.track_id, media_type);
      track_it = current_tracks->erase(track_it);
    } else {
      ++track_it;
    }
  }

  // Find new and active tracks.
  for (const cricket::StreamParams& params : streams) {
    // The sync_label is the MediaStream label and the |stream.id| is the
    // track id.
    const std::string& stream_label = params.sync_label;
    const std::string& track_id = params.id;
    uint32_t ssrc = params.first_ssrc();

    rtc::scoped_refptr<MediaStreamInterface> stream =
        remote_streams_->find(stream_label);
    if (!stream) {
      // This is a new MediaStream. Create a new remote MediaStream.
      stream = remote_stream_factory_->CreateMediaStream(stream_label);
      remote_streams_->AddStream(stream);
      new_streams->AddStream(stream);
    }

    const TrackInfo* track_info =
        FindTrackInfo(*current_tracks, stream_label, track_id);
    if (!track_info) {
      current_tracks->push_back(TrackInfo(stream_label, track_id, ssrc));
      OnRemoteTrackSeen(stream_label, track_id, ssrc, media_type);
    }
  }
}

void PeerConnection::OnRemoteTrackSeen(const std::string& stream_label,
                                       const std::string& track_id,
                                       uint32_t ssrc,
                                       cricket::MediaType media_type) {
  MediaStreamInterface* stream = remote_streams_->find(stream_label);

  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    AudioTrackInterface* audio_track =
        remote_stream_factory_->AddAudioTrack(stream, track_id);
    CreateAudioReceiver(stream, audio_track, ssrc);
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    VideoTrackInterface* video_track =
        remote_stream_factory_->AddVideoTrack(stream, track_id);
    CreateVideoReceiver(stream, video_track, ssrc);
  } else {
    RTC_DCHECK(false && "Invalid media type");
  }
}

void PeerConnection::OnRemoteTrackRemoved(const std::string& stream_label,
                                          const std::string& track_id,
                                          cricket::MediaType media_type) {
  MediaStreamInterface* stream = remote_streams_->find(stream_label);

  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    rtc::scoped_refptr<AudioTrackInterface> audio_track =
        stream->FindAudioTrack(track_id);
    if (audio_track) {
      audio_track->set_state(webrtc::MediaStreamTrackInterface::kEnded);
      stream->RemoveTrack(audio_track);
      DestroyAudioReceiver(stream, audio_track);
    }
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    rtc::scoped_refptr<VideoTrackInterface> video_track =
        stream->FindVideoTrack(track_id);
    if (video_track) {
      video_track->set_state(webrtc::MediaStreamTrackInterface::kEnded);
      stream->RemoveTrack(video_track);
      DestroyVideoReceiver(stream, video_track);
    }
  } else {
    ASSERT(false && "Invalid media type");
  }
}

void PeerConnection::UpdateEndedRemoteMediaStreams() {
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams_to_remove;
  for (size_t i = 0; i < remote_streams_->count(); ++i) {
    MediaStreamInterface* stream = remote_streams_->at(i);
    if (stream->GetAudioTracks().empty() && stream->GetVideoTracks().empty()) {
      streams_to_remove.push_back(stream);
    }
  }

  for (const auto& stream : streams_to_remove) {
    remote_streams_->RemoveStream(stream);
    observer_->OnRemoveStream(stream);
  }
}

void PeerConnection::MaybeCreateDefaultStream() {
  if (!remote_info_.IsDefaultMediaStreamNeeded()) {
    return;
  }

  bool default_created = false;

  rtc::scoped_refptr<MediaStreamInterface> default_remote_stream =
      remote_streams_->find(kDefaultStreamLabel);
  if (default_remote_stream == nullptr) {
    default_created = true;
    default_remote_stream =
        remote_stream_factory_->CreateMediaStream(kDefaultStreamLabel);
    remote_streams_->AddStream(default_remote_stream);
  }
  if (remote_info_.default_audio_track_needed &&
      default_remote_stream->GetAudioTracks().size() == 0) {
    remote_audio_tracks_.push_back(
        TrackInfo(kDefaultStreamLabel, kDefaultAudioTrackLabel, 0));
    OnRemoteTrackSeen(kDefaultStreamLabel, kDefaultAudioTrackLabel, 0,
                      cricket::MEDIA_TYPE_AUDIO);
  }
  if (remote_info_.default_video_track_needed &&
      default_remote_stream->GetVideoTracks().size() == 0) {
    remote_video_tracks_.push_back(
        TrackInfo(kDefaultStreamLabel, kDefaultVideoTrackLabel, 0));
    OnRemoteTrackSeen(kDefaultStreamLabel, kDefaultVideoTrackLabel, 0,
                      cricket::MEDIA_TYPE_VIDEO);
  }
  if (default_created) {
    stats_->AddStream(default_remote_stream);
    observer_->OnAddStream(default_remote_stream);
  }
}

void PeerConnection::EndRemoteTracks(cricket::MediaType media_type) {
  TrackInfos* current_tracks = GetRemoteTracks(media_type);
  for (TrackInfos::iterator track_it = current_tracks->begin();
       track_it != current_tracks->end(); ++track_it) {
    const TrackInfo& info = *track_it;
    MediaStreamInterface* stream = remote_streams_->find(info.stream_label);
    if (media_type == cricket::MEDIA_TYPE_AUDIO) {
      AudioTrackInterface* track = stream->FindAudioTrack(info.track_id);
      // There's no guarantee the track is still available, e.g. the track may
      // have been removed from the stream by javascript.
      if (track) {
        track->set_state(webrtc::MediaStreamTrackInterface::kEnded);
      }
    }
    if (media_type == cricket::MEDIA_TYPE_VIDEO) {
      VideoTrackInterface* track = stream->FindVideoTrack(info.track_id);
      // There's no guarantee the track is still available, e.g. the track may
      // have been removed from the stream by javascript.
      if (track) {
        track->set_state(webrtc::MediaStreamTrackInterface::kEnded);
      }
    }
  }
}

void PeerConnection::UpdateLocalTracks(
    const std::vector<cricket::StreamParams>& streams,
    cricket::MediaType media_type) {
  TrackInfos* current_tracks = GetLocalTracks(media_type);

  // Find removed tracks. I.e., tracks where the track id, stream label or ssrc
  // don't match the new StreamParam.
  TrackInfos::iterator track_it = current_tracks->begin();
  while (track_it != current_tracks->end()) {
    const TrackInfo& info = *track_it;
    const cricket::StreamParams* params =
        cricket::GetStreamBySsrc(streams, info.ssrc);
    if (!params || params->id != info.track_id ||
        params->sync_label != info.stream_label) {
      OnLocalTrackRemoved(info.stream_label, info.track_id, info.ssrc,
                          media_type);
      track_it = current_tracks->erase(track_it);
    } else {
      ++track_it;
    }
  }

  // Find new and active tracks.
  for (const cricket::StreamParams& params : streams) {
    // The sync_label is the MediaStream label and the |stream.id| is the
    // track id.
    const std::string& stream_label = params.sync_label;
    const std::string& track_id = params.id;
    uint32_t ssrc = params.first_ssrc();
    const TrackInfo* track_info =
        FindTrackInfo(*current_tracks, stream_label, track_id);
    if (!track_info) {
      current_tracks->push_back(TrackInfo(stream_label, track_id, ssrc));
      OnLocalTrackSeen(stream_label, track_id, params.first_ssrc(), media_type);
    }
  }
}

void PeerConnection::OnLocalTrackSeen(const std::string& stream_label,
                                      const std::string& track_id,
                                      uint32_t ssrc,
                                      cricket::MediaType media_type) {
  MediaStreamInterface* stream = local_streams_->find(stream_label);
  if (!stream) {
    LOG(LS_WARNING) << "An unknown local MediaStream with label "
                    << stream_label << " has been configured.";
    return;
  }

  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    AudioTrackInterface* audio_track = stream->FindAudioTrack(track_id);
    if (!audio_track) {
      LOG(LS_WARNING) << "An unknown local AudioTrack with id , " << track_id
                      << " has been configured.";
      return;
    }
    CreateAudioSender(stream, audio_track, ssrc);
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    VideoTrackInterface* video_track = stream->FindVideoTrack(track_id);
    if (!video_track) {
      LOG(LS_WARNING) << "An unknown local VideoTrack with id , " << track_id
                      << " has been configured.";
      return;
    }
    CreateVideoSender(stream, video_track, ssrc);
  } else {
    RTC_DCHECK(false && "Invalid media type");
  }
}

void PeerConnection::OnLocalTrackRemoved(const std::string& stream_label,
                                         const std::string& track_id,
                                         uint32_t ssrc,
                                         cricket::MediaType media_type) {
  MediaStreamInterface* stream = local_streams_->find(stream_label);
  if (!stream) {
    // This is the normal case. I.e., RemoveLocalStream has been called and the
    // SessionDescriptions has been renegotiated.
    return;
  }
  // A track has been removed from the SessionDescription but the MediaStream
  // is still associated with PeerConnection. This only occurs if the SDP
  // doesn't match with the calls to AddLocalStream and RemoveLocalStream.
  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    AudioTrackInterface* audio_track = stream->FindAudioTrack(track_id);
    if (!audio_track) {
      return;
    }
    DestroyAudioSender(stream, audio_track, ssrc);
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    VideoTrackInterface* video_track = stream->FindVideoTrack(track_id);
    if (!video_track) {
      return;
    }
    DestroyVideoSender(stream, video_track);
  } else {
    RTC_DCHECK(false && "Invalid media type.");
  }
}

void PeerConnection::UpdateLocalRtpDataChannels(
    const cricket::StreamParamsVec& streams) {
  std::vector<std::string> existing_channels;

  // Find new and active data channels.
  for (const cricket::StreamParams& params : streams) {
    // |it->sync_label| is actually the data channel label. The reason is that
    // we use the same naming of data channels as we do for
    // MediaStreams and Tracks.
    // For MediaStreams, the sync_label is the MediaStream label and the
    // track label is the same as |streamid|.
    const std::string& channel_label = params.sync_label;
    auto data_channel_it = rtp_data_channels_.find(channel_label);
    if (!VERIFY(data_channel_it != rtp_data_channels_.end())) {
      continue;
    }
    // Set the SSRC the data channel should use for sending.
    data_channel_it->second->SetSendSsrc(params.first_ssrc());
    existing_channels.push_back(data_channel_it->first);
  }

  UpdateClosingRtpDataChannels(existing_channels, true);
}

void PeerConnection::UpdateRemoteRtpDataChannels(
    const cricket::StreamParamsVec& streams) {
  std::vector<std::string> existing_channels;

  // Find new and active data channels.
  for (const cricket::StreamParams& params : streams) {
    // The data channel label is either the mslabel or the SSRC if the mslabel
    // does not exist. Ex a=ssrc:444330170 mslabel:test1.
    std::string label = params.sync_label.empty()
                            ? rtc::ToString(params.first_ssrc())
                            : params.sync_label;
    auto data_channel_it = rtp_data_channels_.find(label);
    if (data_channel_it == rtp_data_channels_.end()) {
      // This is a new data channel.
      CreateRemoteRtpDataChannel(label, params.first_ssrc());
    } else {
      data_channel_it->second->SetReceiveSsrc(params.first_ssrc());
    }
    existing_channels.push_back(label);
  }

  UpdateClosingRtpDataChannels(existing_channels, false);
}

void PeerConnection::UpdateClosingRtpDataChannels(
    const std::vector<std::string>& active_channels,
    bool is_local_update) {
  auto it = rtp_data_channels_.begin();
  while (it != rtp_data_channels_.end()) {
    DataChannel* data_channel = it->second;
    if (std::find(active_channels.begin(), active_channels.end(),
                  data_channel->label()) != active_channels.end()) {
      ++it;
      continue;
    }

    if (is_local_update) {
      data_channel->SetSendSsrc(0);
    } else {
      data_channel->RemotePeerRequestClose();
    }

    if (data_channel->state() == DataChannel::kClosed) {
      rtp_data_channels_.erase(it);
      it = rtp_data_channels_.begin();
    } else {
      ++it;
    }
  }
}

void PeerConnection::CreateRemoteRtpDataChannel(const std::string& label,
                                                uint32_t remote_ssrc) {
  rtc::scoped_refptr<DataChannel> channel(
      InternalCreateDataChannel(label, nullptr));
  if (!channel.get()) {
    LOG(LS_WARNING) << "Remote peer requested a DataChannel but"
                    << "CreateDataChannel failed.";
    return;
  }
  channel->SetReceiveSsrc(remote_ssrc);
  observer_->OnDataChannel(
      DataChannelProxy::Create(signaling_thread(), channel));
}

rtc::scoped_refptr<DataChannel> PeerConnection::InternalCreateDataChannel(
    const std::string& label,
    const InternalDataChannelInit* config) {
  if (IsClosed()) {
    return nullptr;
  }
  if (session_->data_channel_type() == cricket::DCT_NONE) {
    LOG(LS_ERROR)
        << "InternalCreateDataChannel: Data is not supported in this call.";
    return nullptr;
  }
  InternalDataChannelInit new_config =
      config ? (*config) : InternalDataChannelInit();
  if (session_->data_channel_type() == cricket::DCT_SCTP) {
    if (new_config.id < 0) {
      rtc::SSLRole role;
      if (session_->GetSslRole(&role) &&
          !sid_allocator_.AllocateSid(role, &new_config.id)) {
        LOG(LS_ERROR) << "No id can be allocated for the SCTP data channel.";
        return nullptr;
      }
    } else if (!sid_allocator_.ReserveSid(new_config.id)) {
      LOG(LS_ERROR) << "Failed to create a SCTP data channel "
                    << "because the id is already in use or out of range.";
      return nullptr;
    }
  }

  rtc::scoped_refptr<DataChannel> channel(DataChannel::Create(
      session_.get(), session_->data_channel_type(), label, new_config));
  if (!channel) {
    sid_allocator_.ReleaseSid(new_config.id);
    return nullptr;
  }

  if (channel->data_channel_type() == cricket::DCT_RTP) {
    if (rtp_data_channels_.find(channel->label()) != rtp_data_channels_.end()) {
      LOG(LS_ERROR) << "DataChannel with label " << channel->label()
                    << " already exists.";
      return nullptr;
    }
    rtp_data_channels_[channel->label()] = channel;
  } else {
    RTC_DCHECK(channel->data_channel_type() == cricket::DCT_SCTP);
    sctp_data_channels_.push_back(channel);
    channel->SignalClosed.connect(this,
                                  &PeerConnection::OnSctpDataChannelClosed);
  }

  return channel;
}

bool PeerConnection::HasDataChannels() const {
  return !rtp_data_channels_.empty() || !sctp_data_channels_.empty();
}

void PeerConnection::AllocateSctpSids(rtc::SSLRole role) {
  for (const auto& channel : sctp_data_channels_) {
    if (channel->id() < 0) {
      int sid;
      if (!sid_allocator_.AllocateSid(role, &sid)) {
        LOG(LS_ERROR) << "Failed to allocate SCTP sid.";
        continue;
      }
      channel->SetSctpSid(sid);
    }
  }
}

void PeerConnection::OnSctpDataChannelClosed(DataChannel* channel) {
  for (auto it = sctp_data_channels_.begin(); it != sctp_data_channels_.end();
       ++it) {
    if (it->get() == channel) {
      if (channel->id() >= 0) {
        sid_allocator_.ReleaseSid(channel->id());
      }
      sctp_data_channels_.erase(it);
      return;
    }
  }
}

void PeerConnection::OnVoiceChannelDestroyed() {
  EndRemoteTracks(cricket::MEDIA_TYPE_AUDIO);
}

void PeerConnection::OnVideoChannelDestroyed() {
  EndRemoteTracks(cricket::MEDIA_TYPE_VIDEO);
}

void PeerConnection::OnDataChannelCreated() {
  for (const auto& channel : sctp_data_channels_) {
    channel->OnTransportChannelCreated();
  }
}

void PeerConnection::OnDataChannelDestroyed() {
  // Use a temporary copy of the RTP/SCTP DataChannel list because the
  // DataChannel may callback to us and try to modify the list.
  std::map<std::string, rtc::scoped_refptr<DataChannel>> temp_rtp_dcs;
  temp_rtp_dcs.swap(rtp_data_channels_);
  for (const auto& kv : temp_rtp_dcs) {
    kv.second->OnTransportChannelDestroyed();
  }

  std::vector<rtc::scoped_refptr<DataChannel>> temp_sctp_dcs;
  temp_sctp_dcs.swap(sctp_data_channels_);
  for (const auto& channel : temp_sctp_dcs) {
    channel->OnTransportChannelDestroyed();
  }
}

void PeerConnection::OnDataChannelOpenMessage(
    const std::string& label,
    const InternalDataChannelInit& config) {
  rtc::scoped_refptr<DataChannel> channel(
      InternalCreateDataChannel(label, &config));
  if (!channel.get()) {
    LOG(LS_ERROR) << "Failed to create DataChannel from the OPEN message.";
    return;
  }

  observer_->OnDataChannel(
      DataChannelProxy::Create(signaling_thread(), channel));
}

std::vector<rtc::scoped_refptr<RtpSenderInterface>>::iterator
PeerConnection::FindSenderForTrack(MediaStreamTrackInterface* track) {
  return std::find_if(
      senders_.begin(), senders_.end(),
      [track](const rtc::scoped_refptr<RtpSenderInterface>& sender) {
        return sender->track() == track;
      });
}

std::vector<rtc::scoped_refptr<RtpReceiverInterface>>::iterator
PeerConnection::FindReceiverForTrack(MediaStreamTrackInterface* track) {
  return std::find_if(
      receivers_.begin(), receivers_.end(),
      [track](const rtc::scoped_refptr<RtpReceiverInterface>& receiver) {
        return receiver->track() == track;
      });
}

PeerConnection::TrackInfos* PeerConnection::GetRemoteTracks(
    cricket::MediaType media_type) {
  RTC_DCHECK(media_type == cricket::MEDIA_TYPE_AUDIO ||
             media_type == cricket::MEDIA_TYPE_VIDEO);
  return (media_type == cricket::MEDIA_TYPE_AUDIO) ? &remote_audio_tracks_
                                                   : &remote_video_tracks_;
}

PeerConnection::TrackInfos* PeerConnection::GetLocalTracks(
    cricket::MediaType media_type) {
  RTC_DCHECK(media_type == cricket::MEDIA_TYPE_AUDIO ||
             media_type == cricket::MEDIA_TYPE_VIDEO);
  return (media_type == cricket::MEDIA_TYPE_AUDIO) ? &local_audio_tracks_
                                                   : &local_video_tracks_;
}

const PeerConnection::TrackInfo* PeerConnection::FindTrackInfo(
    const PeerConnection::TrackInfos& infos,
    const std::string& stream_label,
    const std::string track_id) const {
  for (const TrackInfo& track_info : infos) {
    if (track_info.stream_label == stream_label &&
        track_info.track_id == track_id) {
      return &track_info;
    }
  }
  return nullptr;
}

DataChannel* PeerConnection::FindDataChannelBySid(int sid) const {
  for (const auto& channel : sctp_data_channels_) {
    if (channel->id() == sid) {
      return channel;
    }
  }
  return nullptr;
}

}  // namespace webrtc
