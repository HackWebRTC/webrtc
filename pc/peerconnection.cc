/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/peerconnection.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "api/jsepicecandidate.h"
#include "api/jsepsessiondescription.h"
#include "api/mediaconstraintsinterface.h"
#include "api/mediastreamproxy.h"
#include "api/mediastreamtrackproxy.h"
#include "call/call.h"
#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "media/sctp/sctptransport.h"
#include "pc/audiotrack.h"
#include "pc/channelmanager.h"
#include "pc/dtmfsender.h"
#include "pc/mediastream.h"
#include "pc/mediastreamobserver.h"
#include "pc/remoteaudiosource.h"
#include "pc/rtpreceiver.h"
#include "pc/rtpsender.h"
#include "pc/streamcollection.h"
#include "pc/videocapturertracksource.h"
#include "pc/videotrack.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/safe_conversions.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"

namespace {

using webrtc::DataChannel;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaStreamInterface;
using webrtc::PeerConnectionInterface;
using webrtc::RTCError;
using webrtc::RTCErrorType;
using webrtc::RtpSenderInternal;
using webrtc::RtpSenderInterface;
using webrtc::RtpSenderProxy;
using webrtc::RtpSenderProxyWithInternal;
using webrtc::StreamCollection;

static const char kDefaultStreamLabel[] = "default";
static const char kDefaultAudioTrackLabel[] = "defaulta0";
static const char kDefaultVideoTrackLabel[] = "defaultv0";

// The length of RTCP CNAMEs.
static const int kRtcpCnameLength = 16;

enum {
  MSG_SET_SESSIONDESCRIPTION_SUCCESS = 0,
  MSG_SET_SESSIONDESCRIPTION_FAILED,
  MSG_CREATE_SESSIONDESCRIPTION_FAILED,
  MSG_GETSTATS,
  MSG_FREE_DATACHANNELS,
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

// Add options to |[audio/video]_media_description_options| from |senders|.
void AddRtpSenderOptions(
    const std::vector<rtc::scoped_refptr<
        RtpSenderProxyWithInternal<RtpSenderInternal>>>& senders,
    cricket::MediaDescriptionOptions* audio_media_description_options,
    cricket::MediaDescriptionOptions* video_media_description_options) {
  for (const auto& sender : senders) {
    if (sender->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      if (audio_media_description_options) {
        audio_media_description_options->AddAudioSender(
            sender->id(), sender->internal()->stream_ids());
      }
    } else {
      RTC_DCHECK(sender->media_type() == cricket::MEDIA_TYPE_VIDEO);
      if (video_media_description_options) {
        video_media_description_options->AddVideoSender(
            sender->id(), sender->internal()->stream_ids(), 1);
      }
    }
  }
}

// Add options to |session_options| from |rtp_data_channels|.
void AddRtpDataChannelOptions(
    const std::map<std::string, rtc::scoped_refptr<DataChannel>>&
        rtp_data_channels,
    cricket::MediaDescriptionOptions* data_media_description_options) {
  if (!data_media_description_options) {
    return;
  }
  // Check for data channels.
  for (const auto& kv : rtp_data_channels) {
    const DataChannel* channel = kv.second;
    if (channel->state() == DataChannel::kConnecting ||
        channel->state() == DataChannel::kOpen) {
      // Legacy RTP data channels are signaled with the track/stream ID set to
      // the data channel's label.
      data_media_description_options->AddRtpDataChannel(channel->label(),
                                                        channel->label());
    }
  }
}

uint32_t ConvertIceTransportTypeToCandidateFilter(
    PeerConnectionInterface::IceTransportsType type) {
  switch (type) {
    case PeerConnectionInterface::kNone:
      return cricket::CF_NONE;
    case PeerConnectionInterface::kRelay:
      return cricket::CF_RELAY;
    case PeerConnectionInterface::kNoHost:
      return (cricket::CF_ALL & ~cricket::CF_HOST);
    case PeerConnectionInterface::kAll:
      return cricket::CF_ALL;
    default:
      RTC_NOTREACHED();
  }
  return cricket::CF_NONE;
}

// Helper method to set a voice/video channel on all applicable senders
// and receivers when one is created/destroyed by WebRtcSession.
//
// Used by On(Voice|Video)Channel(Created|Destroyed)
template <class SENDER,
          class RECEIVER,
          class CHANNEL,
          class SENDERS,
          class RECEIVERS>
void SetChannelOnSendersAndReceivers(CHANNEL* channel,
                                     SENDERS& senders,
                                     RECEIVERS& receivers,
                                     cricket::MediaType media_type) {
  for (auto& sender : senders) {
    if (sender->media_type() == media_type) {
      static_cast<SENDER*>(sender->internal())->SetChannel(channel);
    }
  }
  for (auto& receiver : receivers) {
    if (receiver->media_type() == media_type) {
      if (!channel) {
        receiver->internal()->Stop();
      }
      static_cast<RECEIVER*>(receiver->internal())->SetChannel(channel);
    }
  }
}

// Helper to set an error and return from a method.
bool SafeSetError(webrtc::RTCErrorType type, webrtc::RTCError* error) {
  if (error) {
    error->set_type(type);
  }
  return type == webrtc::RTCErrorType::NONE;
}

bool SafeSetError(webrtc::RTCError error, webrtc::RTCError* error_out) {
  if (error_out) {
    *error_out = std::move(error);
  }
  return error.ok();
}

}  // namespace

namespace webrtc {

bool PeerConnectionInterface::RTCConfiguration::operator==(
    const PeerConnectionInterface::RTCConfiguration& o) const {
  // This static_assert prevents us from accidentally breaking operator==.
  // Note: Order matters! Fields must be ordered the same as RTCConfiguration.
  struct stuff_being_tested_for_equality {
    IceServers servers;
    IceTransportsType type;
    BundlePolicy bundle_policy;
    RtcpMuxPolicy rtcp_mux_policy;
    std::vector<rtc::scoped_refptr<rtc::RTCCertificate>> certificates;
    int ice_candidate_pool_size;
    bool disable_ipv6;
    bool disable_ipv6_on_wifi;
    int max_ipv6_networks;
    bool enable_rtp_data_channel;
    rtc::Optional<int> screencast_min_bitrate;
    rtc::Optional<bool> combined_audio_video_bwe;
    rtc::Optional<bool> enable_dtls_srtp;
    TcpCandidatePolicy tcp_candidate_policy;
    CandidateNetworkPolicy candidate_network_policy;
    int audio_jitter_buffer_max_packets;
    bool audio_jitter_buffer_fast_accelerate;
    int ice_connection_receiving_timeout;
    int ice_backup_candidate_pair_ping_interval;
    ContinualGatheringPolicy continual_gathering_policy;
    bool prioritize_most_likely_ice_candidate_pairs;
    struct cricket::MediaConfig media_config;
    bool enable_quic;
    bool prune_turn_ports;
    bool presume_writable_when_fully_relayed;
    bool enable_ice_renomination;
    bool redetermine_role_on_ice_restart;
    rtc::Optional<int> ice_check_min_interval;
    rtc::Optional<rtc::IntervalRange> ice_regather_interval_range;
    webrtc::TurnCustomizer* turn_customizer;
  };
  static_assert(sizeof(stuff_being_tested_for_equality) == sizeof(*this),
                "Did you add something to RTCConfiguration and forget to "
                "update operator==?");
  return type == o.type && servers == o.servers &&
         bundle_policy == o.bundle_policy &&
         rtcp_mux_policy == o.rtcp_mux_policy &&
         tcp_candidate_policy == o.tcp_candidate_policy &&
         candidate_network_policy == o.candidate_network_policy &&
         audio_jitter_buffer_max_packets == o.audio_jitter_buffer_max_packets &&
         audio_jitter_buffer_fast_accelerate ==
             o.audio_jitter_buffer_fast_accelerate &&
         ice_connection_receiving_timeout ==
             o.ice_connection_receiving_timeout &&
         ice_backup_candidate_pair_ping_interval ==
             o.ice_backup_candidate_pair_ping_interval &&
         continual_gathering_policy == o.continual_gathering_policy &&
         certificates == o.certificates &&
         prioritize_most_likely_ice_candidate_pairs ==
             o.prioritize_most_likely_ice_candidate_pairs &&
         media_config == o.media_config && disable_ipv6 == o.disable_ipv6 &&
         disable_ipv6_on_wifi == o.disable_ipv6_on_wifi &&
         max_ipv6_networks == o.max_ipv6_networks &&
         enable_rtp_data_channel == o.enable_rtp_data_channel &&
         enable_quic == o.enable_quic &&
         screencast_min_bitrate == o.screencast_min_bitrate &&
         combined_audio_video_bwe == o.combined_audio_video_bwe &&
         enable_dtls_srtp == o.enable_dtls_srtp &&
         ice_candidate_pool_size == o.ice_candidate_pool_size &&
         prune_turn_ports == o.prune_turn_ports &&
         presume_writable_when_fully_relayed ==
             o.presume_writable_when_fully_relayed &&
         enable_ice_renomination == o.enable_ice_renomination &&
         redetermine_role_on_ice_restart == o.redetermine_role_on_ice_restart &&
         ice_check_min_interval == o.ice_check_min_interval &&
         ice_regather_interval_range == o.ice_regather_interval_range &&
         turn_customizer == o.turn_customizer;
}

bool PeerConnectionInterface::RTCConfiguration::operator!=(
    const PeerConnectionInterface::RTCConfiguration& o) const {
  return !(*this == o);
}

// Generate a RTCP CNAME when a PeerConnection is created.
std::string GenerateRtcpCname() {
  std::string cname;
  if (!rtc::CreateRandomString(kRtcpCnameLength, &cname)) {
    LOG(LS_ERROR) << "Failed to generate CNAME.";
    RTC_NOTREACHED();
  }
  return cname;
}

bool ValidateOfferAnswerOptions(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options) {
  return IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_audio) &&
         IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_video);
}

// From |rtc_options|, fill parts of |session_options| shared by all generated
// m= sections (in other words, nothing that involves a map/array).
void ExtractSharedMediaSessionOptions(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options) {
  session_options->vad_enabled = rtc_options.voice_activity_detection;
  session_options->bundle_enabled = rtc_options.use_rtp_mux;
}

bool ConvertConstraintsToOfferAnswerOptions(
    const MediaConstraintsInterface* constraints,
    PeerConnectionInterface::RTCOfferAnswerOptions* offer_answer_options) {
  if (!constraints) {
    return true;
  }

  bool value = false;
  size_t mandatory_constraints_satisfied = 0;

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kOfferToReceiveAudio, &value,
                     &mandatory_constraints_satisfied)) {
    offer_answer_options->offer_to_receive_audio =
        value ? PeerConnectionInterface::RTCOfferAnswerOptions::
                    kOfferToReceiveMediaTrue
              : 0;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kOfferToReceiveVideo, &value,
                     &mandatory_constraints_satisfied)) {
    offer_answer_options->offer_to_receive_video =
        value ? PeerConnectionInterface::RTCOfferAnswerOptions::
                    kOfferToReceiveMediaTrue
              : 0;
  }
  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kVoiceActivityDetection, &value,
                     &mandatory_constraints_satisfied)) {
    offer_answer_options->voice_activity_detection = value;
  }
  if (FindConstraint(constraints, MediaConstraintsInterface::kUseRtpMux, &value,
                     &mandatory_constraints_satisfied)) {
    offer_answer_options->use_rtp_mux = value;
  }
  if (FindConstraint(constraints, MediaConstraintsInterface::kIceRestart,
                     &value, &mandatory_constraints_satisfied)) {
    offer_answer_options->ice_restart = value;
  }

  return mandatory_constraints_satisfied == constraints->GetMandatory().size();
}

PeerConnection::PeerConnection(PeerConnectionFactory* factory,
                               std::unique_ptr<RtcEventLog> event_log,
                               std::unique_ptr<Call> call)
    : factory_(factory),
      observer_(NULL),
      uma_observer_(NULL),
      event_log_(std::move(event_log)),
      signaling_state_(kStable),
      ice_connection_state_(kIceConnectionNew),
      ice_gathering_state_(kIceGatheringNew),
      rtcp_cname_(GenerateRtcpCname()),
      local_streams_(StreamCollection::Create()),
      remote_streams_(StreamCollection::Create()),
      call_(std::move(call)) {}

PeerConnection::~PeerConnection() {
  TRACE_EVENT0("webrtc", "PeerConnection::~PeerConnection");
  RTC_DCHECK(signaling_thread()->IsCurrent());
  // Need to detach RTP senders/receivers from WebRtcSession,
  // since it's about to be destroyed.
  for (const auto& sender : senders_) {
    sender->internal()->Stop();
  }
  for (const auto& receiver : receivers_) {
    receiver->internal()->Stop();
  }
  // Destroy stats_ because it depends on session_.
  stats_.reset(nullptr);
  if (stats_collector_) {
    stats_collector_->WaitForPendingRequest();
    stats_collector_ = nullptr;
  }
  // Now destroy session_ before destroying other members,
  // because its destruction fires signals (such as VoiceChannelDestroyed)
  // which will trigger some final actions in PeerConnection...
  owned_session_.reset(nullptr);
  session_ = nullptr;
  // port_allocator_ lives on the network thread and should be destroyed there.
  network_thread()->Invoke<void>(RTC_FROM_HERE,
                                 [this] { port_allocator_.reset(); });
  // call_ and event_log_ must be destroyed on the worker thread.
  worker_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
    call_.reset();
    event_log_.reset();
  });
}

bool PeerConnection::Initialize(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    std::unique_ptr<cricket::PortAllocator> allocator,
    std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
    PeerConnectionObserver* observer) {
  TRACE_EVENT0("webrtc", "PeerConnection::Initialize");

  RTCError config_error = ValidateConfiguration(configuration);
  if (!config_error.ok()) {
    LOG(LS_ERROR) << "Invalid configuration: " << config_error.message();
    return false;
  }

  if (!allocator) {
    LOG(LS_ERROR) << "PeerConnection initialized without a PortAllocator? "
                  << "This shouldn't happen if using PeerConnectionFactory.";
    return false;
  }

  if (!observer) {
    // TODO(deadbeef): Why do we do this?
    LOG(LS_ERROR) << "PeerConnection initialized without a "
                  << "PeerConnectionObserver";
    return false;
  }
  observer_ = observer;
  port_allocator_ = std::move(allocator);

  // The port allocator lives on the network thread and should be initialized
  // there.
  if (!network_thread()->Invoke<bool>(
          RTC_FROM_HERE, rtc::Bind(&PeerConnection::InitializePortAllocator_n,
                                   this, configuration))) {
    return false;
  }

  owned_session_.reset(new WebRtcSession(
      call_.get(), factory_->channel_manager(), configuration.media_config,
      event_log_.get(), network_thread(), worker_thread(), signaling_thread(),
      port_allocator_.get(),
      std::unique_ptr<cricket::TransportController>(
          factory_->CreateTransportController(
              port_allocator_.get(),
              configuration.redetermine_role_on_ice_restart)),
#ifdef HAVE_SCTP
      std::unique_ptr<cricket::SctpTransportInternalFactory>(
          new cricket::SctpTransportFactory(network_thread()))
#else
      nullptr
#endif
          ));
  session_ = owned_session_.get();

  stats_.reset(new StatsCollector(this));
  stats_collector_ = RTCStatsCollector::Create(this);

  // Initialize the WebRtcSession. It creates transport channels etc.
  if (!session_->Initialize(factory_->options(), std::move(cert_generator),
                            configuration)) {
    return false;
  }

  // Register PeerConnection as receiver of local ice candidates.
  // All the callbacks will be posted to the application from PeerConnection.
  session_->RegisterIceObserver(this);
  session_->SignalState.connect(this, &PeerConnection::OnSessionStateChange);
  session_->SignalVoiceChannelCreated.connect(
      this, &PeerConnection::OnVoiceChannelCreated);
  session_->SignalVoiceChannelDestroyed.connect(
      this, &PeerConnection::OnVoiceChannelDestroyed);
  session_->SignalVideoChannelCreated.connect(
      this, &PeerConnection::OnVideoChannelCreated);
  session_->SignalVideoChannelDestroyed.connect(
      this, &PeerConnection::OnVideoChannelDestroyed);
  session_->SignalDataChannelCreated.connect(
      this, &PeerConnection::OnDataChannelCreated);
  session_->SignalDataChannelDestroyed.connect(
      this, &PeerConnection::OnDataChannelDestroyed);
  session_->SignalDataChannelOpenMessage.connect(
      this, &PeerConnection::OnDataChannelOpenMessage);

  configuration_ = configuration;
  return true;
}

RTCError PeerConnection::ValidateConfiguration(
    const RTCConfiguration& config) const {
  if (config.ice_regather_interval_range &&
      config.continual_gathering_policy == GATHER_ONCE) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "ice_regather_interval_range specified but continual "
                    "gathering policy is GATHER_ONCE");
  }
  return RTCError::OK();
}

rtc::scoped_refptr<StreamCollectionInterface>
PeerConnection::local_streams() {
  return local_streams_;
}

rtc::scoped_refptr<StreamCollectionInterface>
PeerConnection::remote_streams() {
  return remote_streams_;
}

bool PeerConnection::AddStream(MediaStreamInterface* local_stream) {
  TRACE_EVENT0("webrtc", "PeerConnection::AddStream");
  if (IsClosed()) {
    return false;
  }
  if (!CanAddLocalMediaStream(local_streams_, local_stream)) {
    return false;
  }

  local_streams_->AddStream(local_stream);
  MediaStreamObserver* observer = new MediaStreamObserver(local_stream);
  observer->SignalAudioTrackAdded.connect(this,
                                          &PeerConnection::OnAudioTrackAdded);
  observer->SignalAudioTrackRemoved.connect(
      this, &PeerConnection::OnAudioTrackRemoved);
  observer->SignalVideoTrackAdded.connect(this,
                                          &PeerConnection::OnVideoTrackAdded);
  observer->SignalVideoTrackRemoved.connect(
      this, &PeerConnection::OnVideoTrackRemoved);
  stream_observers_.push_back(std::unique_ptr<MediaStreamObserver>(observer));

  for (const auto& track : local_stream->GetAudioTracks()) {
    AddAudioTrack(track.get(), local_stream);
  }
  for (const auto& track : local_stream->GetVideoTracks()) {
    AddVideoTrack(track.get(), local_stream);
  }

  stats_->AddStream(local_stream);
  observer_->OnRenegotiationNeeded();
  return true;
}

void PeerConnection::RemoveStream(MediaStreamInterface* local_stream) {
  TRACE_EVENT0("webrtc", "PeerConnection::RemoveStream");
  if (!IsClosed()) {
    for (const auto& track : local_stream->GetAudioTracks()) {
      RemoveAudioTrack(track.get(), local_stream);
    }
    for (const auto& track : local_stream->GetVideoTracks()) {
      RemoveVideoTrack(track.get(), local_stream);
    }
  }
  local_streams_->RemoveStream(local_stream);
  stream_observers_.erase(
      std::remove_if(
          stream_observers_.begin(), stream_observers_.end(),
          [local_stream](const std::unique_ptr<MediaStreamObserver>& observer) {
            return observer->stream()->label().compare(local_stream->label()) ==
                   0;
          }),
      stream_observers_.end());

  if (IsClosed()) {
    return;
  }
  observer_->OnRenegotiationNeeded();
}

rtc::scoped_refptr<RtpSenderInterface> PeerConnection::AddTrack(
    MediaStreamTrackInterface* track,
    std::vector<MediaStreamInterface*> streams) {
  TRACE_EVENT0("webrtc", "PeerConnection::AddTrack");
  if (IsClosed()) {
    return nullptr;
  }
  if (streams.size() >= 2) {
    LOG(LS_ERROR)
        << "Adding a track with two streams is not currently supported.";
    return nullptr;
  }
  // TODO(deadbeef): Support adding a track to two different senders.
  if (FindSenderForTrack(track) != senders_.end()) {
    LOG(LS_ERROR) << "Sender for track " << track->id() << " already exists.";
    return nullptr;
  }

  // TODO(deadbeef): Support adding a track to multiple streams.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> new_sender;
  if (track->kind() == MediaStreamTrackInterface::kAudioKind) {
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(),
        new AudioRtpSender(static_cast<AudioTrackInterface*>(track),
                           session_->voice_channel(), stats_.get()));
    if (!streams.empty()) {
      new_sender->internal()->set_stream_id(streams[0]->label());
    }
    const TrackInfo* track_info = FindTrackInfo(
        local_audio_tracks_, new_sender->internal()->stream_id(), track->id());
    if (track_info) {
      new_sender->internal()->SetSsrc(track_info->ssrc);
    }
  } else if (track->kind() == MediaStreamTrackInterface::kVideoKind) {
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(),
        new VideoRtpSender(static_cast<VideoTrackInterface*>(track),
                           session_->video_channel()));
    if (!streams.empty()) {
      new_sender->internal()->set_stream_id(streams[0]->label());
    }
    const TrackInfo* track_info = FindTrackInfo(
        local_video_tracks_, new_sender->internal()->stream_id(), track->id());
    if (track_info) {
      new_sender->internal()->SetSsrc(track_info->ssrc);
    }
  } else {
    LOG(LS_ERROR) << "CreateSender called with invalid kind: " << track->kind();
    return rtc::scoped_refptr<RtpSenderInterface>();
  }

  senders_.push_back(new_sender);
  observer_->OnRenegotiationNeeded();
  return new_sender;
}

bool PeerConnection::RemoveTrack(RtpSenderInterface* sender) {
  TRACE_EVENT0("webrtc", "PeerConnection::RemoveTrack");
  if (IsClosed()) {
    return false;
  }

  auto it = std::find(senders_.begin(), senders_.end(), sender);
  if (it == senders_.end()) {
    LOG(LS_ERROR) << "Couldn't find sender " << sender->id() << " to remove.";
    return false;
  }
  (*it)->internal()->Stop();
  senders_.erase(it);

  observer_->OnRenegotiationNeeded();
  return true;
}

rtc::scoped_refptr<DtmfSenderInterface> PeerConnection::CreateDtmfSender(
    AudioTrackInterface* track) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateDtmfSender");
  if (IsClosed()) {
    return nullptr;
  }
  if (!track) {
    LOG(LS_ERROR) << "CreateDtmfSender - track is NULL.";
    return nullptr;
  }
  auto it = FindSenderForTrack(track);
  if (it == senders_.end()) {
    LOG(LS_ERROR) << "CreateDtmfSender called with a non-added track.";
    return nullptr;
  }

  return (*it)->GetDtmfSender();
}

rtc::scoped_refptr<RtpSenderInterface> PeerConnection::CreateSender(
    const std::string& kind,
    const std::string& stream_id) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateSender");
  if (IsClosed()) {
    return nullptr;
  }
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> new_sender;
  if (kind == MediaStreamTrackInterface::kAudioKind) {
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(),
        new AudioRtpSender(session_->voice_channel(), stats_.get()));
  } else if (kind == MediaStreamTrackInterface::kVideoKind) {
    new_sender = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread(), new VideoRtpSender(session_->video_channel()));
  } else {
    LOG(LS_ERROR) << "CreateSender called with invalid kind: " << kind;
    return new_sender;
  }
  if (!stream_id.empty()) {
    new_sender->internal()->set_stream_id(stream_id);
  }
  senders_.push_back(new_sender);
  return new_sender;
}

std::vector<rtc::scoped_refptr<RtpSenderInterface>> PeerConnection::GetSenders()
    const {
  std::vector<rtc::scoped_refptr<RtpSenderInterface>> ret;
  for (const auto& sender : senders_) {
    ret.push_back(sender.get());
  }
  return ret;
}

std::vector<rtc::scoped_refptr<RtpReceiverInterface>>
PeerConnection::GetReceivers() const {
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> ret;
  for (const auto& receiver : receivers_) {
    ret.push_back(receiver.get());
  }
  return ret;
}

bool PeerConnection::GetStats(StatsObserver* observer,
                              MediaStreamTrackInterface* track,
                              StatsOutputLevel level) {
  TRACE_EVENT0("webrtc", "PeerConnection::GetStats");
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (!observer) {
    LOG(LS_ERROR) << "GetStats - observer is NULL.";
    return false;
  }

  stats_->UpdateStats(level);
  // The StatsCollector is used to tell if a track is valid because it may
  // remember tracks that the PeerConnection previously removed.
  if (track && !stats_->IsValidTrack(track->id())) {
    LOG(LS_WARNING) << "GetStats is called with an invalid track: "
                    << track->id();
    return false;
  }
  signaling_thread()->Post(RTC_FROM_HERE, this, MSG_GETSTATS,
                           new GetStatsMsg(observer, track));
  return true;
}

void PeerConnection::GetStats(RTCStatsCollectorCallback* callback) {
  RTC_DCHECK(stats_collector_);
  stats_collector_->GetStatsReport(callback);
}

PeerConnectionInterface::SignalingState PeerConnection::signaling_state() {
  return signaling_state_;
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
  TRACE_EVENT0("webrtc", "PeerConnection::CreateDataChannel");
#ifdef HAVE_QUIC
  if (session_->data_channel_type() == cricket::DCT_QUIC) {
    // TODO(zhihuang): Handle case when config is NULL.
    if (!config) {
      LOG(LS_ERROR) << "Missing config for QUIC data channel.";
      return nullptr;
    }
    // TODO(zhihuang): Allow unreliable or ordered QUIC data channels.
    if (!config->reliable || config->ordered) {
      LOG(LS_ERROR) << "QUIC data channel does not implement unreliable or "
                       "ordered delivery.";
      return nullptr;
    }
    return session_->quic_data_transport()->CreateDataChannel(label, config);
  }
#endif  // HAVE_QUIC

  bool first_datachannel = !HasDataChannels();

  std::unique_ptr<InternalDataChannelInit> internal_config;
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
  TRACE_EVENT0("webrtc", "PeerConnection::CreateOffer");
  if (!observer) {
    LOG(LS_ERROR) << "CreateOffer - observer is NULL.";
    return;
  }
  PeerConnectionInterface::RTCOfferAnswerOptions offer_answer_options;
  // Always create an offer even if |ConvertConstraintsToOfferAnswerOptions|
  // returns false for now. Because |ConvertConstraintsToOfferAnswerOptions|
  // compares the mandatory fields parsed with the mandatory fields added in the
  // |constraints| and some downstream applications might create offers with
  // mandatory fields which would not be parsed in the helper method. For
  // example, in Chromium/remoting, |kEnableDtlsSrtp| is added to the
  // |constraints| as a mandatory field but it is not parsed.
  ConvertConstraintsToOfferAnswerOptions(constraints, &offer_answer_options);

  CreateOffer(observer, offer_answer_options);
}

void PeerConnection::CreateOffer(CreateSessionDescriptionObserver* observer,
                                 const RTCOfferAnswerOptions& options) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateOffer");
  if (!observer) {
    LOG(LS_ERROR) << "CreateOffer - observer is NULL.";
    return;
  }

  if (!ValidateOfferAnswerOptions(options)) {
    std::string error = "CreateOffer called with invalid options.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(observer, error);
    return;
  }

  cricket::MediaSessionOptions session_options;
  GetOptionsForOffer(options, &session_options);
  session_->CreateOffer(observer, options, session_options);
}

void PeerConnection::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const MediaConstraintsInterface* constraints) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateAnswer");
  if (!observer) {
    LOG(LS_ERROR) << "CreateAnswer - observer is NULL.";
    return;
  }

  if (!session_->remote_description() ||
      session_->remote_description()->type() !=
          SessionDescriptionInterface::kOffer) {
    std::string error = "CreateAnswer called without remote offer.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(observer, error);
    return;
  }

  PeerConnectionInterface::RTCOfferAnswerOptions offer_answer_options;
  if (!ConvertConstraintsToOfferAnswerOptions(constraints,
                                              &offer_answer_options)) {
    std::string error = "CreateAnswer called with invalid constraints.";
    LOG(LS_ERROR) << error;
    PostCreateSessionDescriptionFailure(observer, error);
    return;
  }

  cricket::MediaSessionOptions session_options;
  GetOptionsForAnswer(offer_answer_options, &session_options);
  session_->CreateAnswer(observer, session_options);
}

void PeerConnection::CreateAnswer(CreateSessionDescriptionObserver* observer,
                                  const RTCOfferAnswerOptions& options) {
  TRACE_EVENT0("webrtc", "PeerConnection::CreateAnswer");
  if (!observer) {
    LOG(LS_ERROR) << "CreateAnswer - observer is NULL.";
    return;
  }

  cricket::MediaSessionOptions session_options;
  GetOptionsForAnswer(options, &session_options);

  session_->CreateAnswer(observer, session_options);
}

void PeerConnection::SetLocalDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  TRACE_EVENT0("webrtc", "PeerConnection::SetLocalDescription");
  if (IsClosed()) {
    return;
  }
  if (!observer) {
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
      session_->GetSctpSslRole(&role)) {
    AllocateSctpSids(role);
  }

  // Update state and SSRC of local MediaStreams and DataChannels based on the
  // local session description.
  const cricket::ContentInfo* audio_content =
      GetFirstAudioContent(desc->description());
  if (audio_content) {
    if (audio_content->rejected) {
      RemoveTracks(cricket::MEDIA_TYPE_AUDIO);
    } else {
      const cricket::AudioContentDescription* audio_desc =
          static_cast<const cricket::AudioContentDescription*>(
              audio_content->description);
      UpdateLocalTracks(audio_desc->streams(), audio_desc->type());
    }
  }

  const cricket::ContentInfo* video_content =
      GetFirstVideoContent(desc->description());
  if (video_content) {
    if (video_content->rejected) {
      RemoveTracks(cricket::MEDIA_TYPE_VIDEO);
    } else {
      const cricket::VideoContentDescription* video_desc =
          static_cast<const cricket::VideoContentDescription*>(
              video_content->description);
      UpdateLocalTracks(video_desc->streams(), video_desc->type());
    }
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
  signaling_thread()->Post(RTC_FROM_HERE, this,
                           MSG_SET_SESSIONDESCRIPTION_SUCCESS, msg);

  // According to JSEP, after setLocalDescription, changing the candidate pool
  // size is not allowed, and changing the set of ICE servers will not result
  // in new candidates being gathered.
  port_allocator_->FreezeCandidatePool();

  // MaybeStartGathering needs to be called after posting
  // MSG_SET_SESSIONDESCRIPTION_SUCCESS, so that we don't signal any candidates
  // before signaling that SetLocalDescription completed.
  session_->MaybeStartGathering();

  if (desc->type() == SessionDescriptionInterface::kAnswer) {
    // TODO(deadbeef): We already had to hop to the network thread for
    // MaybeStartGathering...
    network_thread()->Invoke<void>(
        RTC_FROM_HERE,
        rtc::Bind(&cricket::PortAllocator::DiscardCandidatePool,
                  port_allocator_.get()));
  }
}

void PeerConnection::SetRemoteDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  TRACE_EVENT0("webrtc", "PeerConnection::SetRemoteDescription");
  if (IsClosed()) {
    return;
  }
  if (!observer) {
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
      session_->GetSctpSslRole(&role)) {
    AllocateSctpSids(role);
  }

  const cricket::SessionDescription* remote_desc = desc->description();
  const cricket::ContentInfo* audio_content = GetFirstAudioContent(remote_desc);
  const cricket::ContentInfo* video_content = GetFirstVideoContent(remote_desc);
  const cricket::AudioContentDescription* audio_desc =
      GetFirstAudioContentDescription(remote_desc);
  const cricket::VideoContentDescription* video_desc =
      GetFirstVideoContentDescription(remote_desc);
  const cricket::DataContentDescription* data_desc =
      GetFirstDataContentDescription(remote_desc);

  // Check if the descriptions include streams, just in case the peer supports
  // MSID, but doesn't indicate so with "a=msid-semantic".
  if (remote_desc->msid_supported() ||
      (audio_desc && !audio_desc->streams().empty()) ||
      (video_desc && !video_desc->streams().empty())) {
    remote_peer_supports_msid_ = true;
  }

  // We wait to signal new streams until we finish processing the description,
  // since only at that point will new streams have all their tracks.
  rtc::scoped_refptr<StreamCollection> new_streams(StreamCollection::Create());

  // Find all audio rtp streams and create corresponding remote AudioTracks
  // and MediaStreams.
  if (audio_content) {
    if (audio_content->rejected) {
      RemoveTracks(cricket::MEDIA_TYPE_AUDIO);
    } else {
      bool default_audio_track_needed =
          !remote_peer_supports_msid_ &&
          MediaContentDirectionHasSend(audio_desc->direction());
      UpdateRemoteStreamsList(GetActiveStreams(audio_desc),
                              default_audio_track_needed, audio_desc->type(),
                              new_streams);
    }
  }

  // Find all video rtp streams and create corresponding remote VideoTracks
  // and MediaStreams.
  if (video_content) {
    if (video_content->rejected) {
      RemoveTracks(cricket::MEDIA_TYPE_VIDEO);
    } else {
      bool default_video_track_needed =
          !remote_peer_supports_msid_ &&
          MediaContentDirectionHasSend(video_desc->direction());
      UpdateRemoteStreamsList(GetActiveStreams(video_desc),
                              default_video_track_needed, video_desc->type(),
                              new_streams);
    }
  }

  // Update the DataChannels with the information from the remote peer.
  if (data_desc) {
    if (rtc::starts_with(data_desc->protocol().data(),
                         cricket::kMediaProtocolRtpPrefix)) {
      UpdateRemoteRtpDataChannels(GetActiveStreams(data_desc));
    }
  }

  // Iterate new_streams and notify the observer about new MediaStreams.
  for (size_t i = 0; i < new_streams->count(); ++i) {
    MediaStreamInterface* new_stream = new_streams->at(i);
    stats_->AddStream(new_stream);
    observer_->OnAddStream(
        rtc::scoped_refptr<MediaStreamInterface>(new_stream));
  }

  UpdateEndedRemoteMediaStreams();

  SetSessionDescriptionMsg* msg = new SetSessionDescriptionMsg(observer);
  signaling_thread()->Post(RTC_FROM_HERE, this,
                           MSG_SET_SESSIONDESCRIPTION_SUCCESS, msg);

  if (desc->type() == SessionDescriptionInterface::kAnswer) {
    // TODO(deadbeef): We already had to hop to the network thread for
    // MaybeStartGathering...
    network_thread()->Invoke<void>(
        RTC_FROM_HERE,
        rtc::Bind(&cricket::PortAllocator::DiscardCandidatePool,
                  port_allocator_.get()));
  }
}

PeerConnectionInterface::RTCConfiguration PeerConnection::GetConfiguration() {
  return configuration_;
}

bool PeerConnection::SetConfiguration(const RTCConfiguration& configuration,
                                      RTCError* error) {
  TRACE_EVENT0("webrtc", "PeerConnection::SetConfiguration");

  if (session_->local_description() &&
      configuration.ice_candidate_pool_size !=
          configuration_.ice_candidate_pool_size) {
    LOG(LS_ERROR) << "Can't change candidate pool size after calling "
                     "SetLocalDescription.";
    return SafeSetError(RTCErrorType::INVALID_MODIFICATION, error);
  }

  // The simplest (and most future-compatible) way to tell if the config was
  // modified in an invalid way is to copy each property we do support
  // modifying, then use operator==. There are far more properties we don't
  // support modifying than those we do, and more could be added.
  RTCConfiguration modified_config = configuration_;
  modified_config.servers = configuration.servers;
  modified_config.type = configuration.type;
  modified_config.ice_candidate_pool_size =
      configuration.ice_candidate_pool_size;
  modified_config.prune_turn_ports = configuration.prune_turn_ports;
  modified_config.ice_check_min_interval = configuration.ice_check_min_interval;
  modified_config.turn_customizer = configuration.turn_customizer;
  if (configuration != modified_config) {
    LOG(LS_ERROR) << "Modifying the configuration in an unsupported way.";
    return SafeSetError(RTCErrorType::INVALID_MODIFICATION, error);
  }

  // Validate the modified configuration.
  RTCError validate_error = ValidateConfiguration(modified_config);
  if (!validate_error.ok()) {
    return SafeSetError(std::move(validate_error), error);
  }

  // Note that this isn't possible through chromium, since it's an unsigned
  // short in WebIDL.
  if (configuration.ice_candidate_pool_size < 0 ||
      configuration.ice_candidate_pool_size > UINT16_MAX) {
    return SafeSetError(RTCErrorType::INVALID_RANGE, error);
  }

  // Parse ICE servers before hopping to network thread.
  cricket::ServerAddresses stun_servers;
  std::vector<cricket::RelayServerConfig> turn_servers;
  RTCErrorType parse_error =
      ParseIceServers(configuration.servers, &stun_servers, &turn_servers);
  if (parse_error != RTCErrorType::NONE) {
    return SafeSetError(parse_error, error);
  }

  // In theory this shouldn't fail.
  if (!network_thread()->Invoke<bool>(
          RTC_FROM_HERE,
          rtc::Bind(&PeerConnection::ReconfigurePortAllocator_n, this,
                    stun_servers, turn_servers, modified_config.type,
                    modified_config.ice_candidate_pool_size,
                    modified_config.prune_turn_ports,
                    modified_config.turn_customizer))) {
    LOG(LS_ERROR) << "Failed to apply configuration to PortAllocator.";
    return SafeSetError(RTCErrorType::INTERNAL_ERROR, error);
  }

  // As described in JSEP, calling setConfiguration with new ICE servers or
  // candidate policy must set a "needs-ice-restart" bit so that the next offer
  // triggers an ICE restart which will pick up the changes.
  if (modified_config.servers != configuration_.servers ||
      modified_config.type != configuration_.type ||
      modified_config.prune_turn_ports != configuration_.prune_turn_ports) {
    session_->SetNeedsIceRestartFlag();
  }

  if (modified_config.ice_check_min_interval !=
      configuration_.ice_check_min_interval) {
    session_->SetIceConfig(session_->ParseIceConfig(modified_config));
  }

  configuration_ = modified_config;
  return SafeSetError(RTCErrorType::NONE, error);
}

bool PeerConnection::AddIceCandidate(
    const IceCandidateInterface* ice_candidate) {
  TRACE_EVENT0("webrtc", "PeerConnection::AddIceCandidate");
  if (IsClosed()) {
    return false;
  }
  return session_->ProcessIceMessage(ice_candidate);
}

bool PeerConnection::RemoveIceCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  TRACE_EVENT0("webrtc", "PeerConnection::RemoveIceCandidates");
  return session_->RemoveRemoteIceCandidates(candidates);
}

void PeerConnection::RegisterUMAObserver(UMAObserver* observer) {
  TRACE_EVENT0("webrtc", "PeerConnection::RegisterUmaObserver");
  uma_observer_ = observer;

  if (session_) {
    session_->set_metrics_observer(uma_observer_);
  }

  // Send information about IPv4/IPv6 status.
  if (uma_observer_) {
    port_allocator_->SetMetricsObserver(uma_observer_);
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

RTCError PeerConnection::SetBitrate(const BitrateParameters& bitrate) {
  if (!worker_thread()->IsCurrent()) {
    return worker_thread()->Invoke<RTCError>(
        RTC_FROM_HERE, rtc::Bind(&PeerConnection::SetBitrate, this, bitrate));
  }

  const bool has_min = static_cast<bool>(bitrate.min_bitrate_bps);
  const bool has_current = static_cast<bool>(bitrate.current_bitrate_bps);
  const bool has_max = static_cast<bool>(bitrate.max_bitrate_bps);
  if (has_min && *bitrate.min_bitrate_bps < 0) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "min_bitrate_bps <= 0");
  }
  if (has_current) {
    if (has_min && *bitrate.current_bitrate_bps < *bitrate.min_bitrate_bps) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "current_bitrate_bps < min_bitrate_bps");
    } else if (*bitrate.current_bitrate_bps < 0) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "curent_bitrate_bps < 0");
    }
  }
  if (has_max) {
    if (has_current &&
        *bitrate.max_bitrate_bps < *bitrate.current_bitrate_bps) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "max_bitrate_bps < current_bitrate_bps");
    } else if (has_min && *bitrate.max_bitrate_bps < *bitrate.min_bitrate_bps) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "max_bitrate_bps < min_bitrate_bps");
    } else if (*bitrate.max_bitrate_bps < 0) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "max_bitrate_bps < 0");
    }
  }

  Call::Config::BitrateConfigMask mask;
  mask.min_bitrate_bps = bitrate.min_bitrate_bps;
  mask.start_bitrate_bps = bitrate.current_bitrate_bps;
  mask.max_bitrate_bps = bitrate.max_bitrate_bps;

  RTC_DCHECK(call_.get());
  call_->SetBitrateConfigMask(mask);

  return RTCError::OK();
}

std::unique_ptr<rtc::SSLCertificate>
PeerConnection::GetRemoteAudioSSLCertificate() {
  if (!session_) {
    return nullptr;
  }
  auto* voice_channel = session_->voice_channel();
  if (!voice_channel) {
    return nullptr;
  }
  return GetRemoteSSLCertificate(voice_channel->transport_name());
}

bool PeerConnection::StartRtcEventLog(rtc::PlatformFile file,
                                      int64_t max_size_bytes) {
  return worker_thread()->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&PeerConnection::StartRtcEventLog_w, this, file,
                               max_size_bytes));
}

void PeerConnection::StopRtcEventLog() {
  worker_thread()->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&PeerConnection::StopRtcEventLog_w, this));
}

const SessionDescriptionInterface* PeerConnection::local_description() const {
  return session_->local_description();
}

const SessionDescriptionInterface* PeerConnection::remote_description() const {
  return session_->remote_description();
}

const SessionDescriptionInterface* PeerConnection::current_local_description()
    const {
  return session_->current_local_description();
}

const SessionDescriptionInterface* PeerConnection::current_remote_description()
    const {
  return session_->current_remote_description();
}

const SessionDescriptionInterface* PeerConnection::pending_local_description()
    const {
  return session_->pending_local_description();
}

const SessionDescriptionInterface* PeerConnection::pending_remote_description()
    const {
  return session_->pending_remote_description();
}

void PeerConnection::Close() {
  TRACE_EVENT0("webrtc", "PeerConnection::Close");
  // Update stats here so that we have the most recent stats for tracks and
  // streams before the channels are closed.
  stats_->UpdateStats(kStatsOutputLevelStandard);

  session_->Close();
  network_thread()->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&cricket::PortAllocator::DiscardCandidatePool,
                port_allocator_.get()));

  worker_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
    call_.reset();
    // The event log must outlive call (and any other object that uses it).
    event_log_.reset();
  });
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
    case MSG_FREE_DATACHANNELS: {
      sctp_data_channels_to_free_.clear();
      break;
    }
    default:
      RTC_NOTREACHED() << "Not implemented";
      break;
  }
}

void PeerConnection::CreateAudioReceiver(MediaStreamInterface* stream,
                                         const std::string& track_id,
                                         uint32_t ssrc) {
  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
      receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
          signaling_thread(),
          new AudioRtpReceiver(track_id, ssrc, session_->voice_channel()));
  stream->AddTrack(
      static_cast<AudioTrackInterface*>(receiver->internal()->track().get()));
  receivers_.push_back(receiver);
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams;
  streams.push_back(rtc::scoped_refptr<MediaStreamInterface>(stream));
  observer_->OnAddTrack(receiver, streams);
}

void PeerConnection::CreateVideoReceiver(MediaStreamInterface* stream,
                                         const std::string& track_id,
                                         uint32_t ssrc) {
  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
      receiver = RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
          signaling_thread(),
          new VideoRtpReceiver(track_id, worker_thread(), ssrc,
                               session_->video_channel()));
  stream->AddTrack(
      static_cast<VideoTrackInterface*>(receiver->internal()->track().get()));
  receivers_.push_back(receiver);
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams;
  streams.push_back(rtc::scoped_refptr<MediaStreamInterface>(stream));
  observer_->OnAddTrack(receiver, streams);
}

// TODO(deadbeef): Keep RtpReceivers around even if track goes away in remote
// description.
rtc::scoped_refptr<RtpReceiverInterface> PeerConnection::RemoveAndStopReceiver(
    const std::string& track_id) {
  auto it = FindReceiverForTrack(track_id);
  if (it == receivers_.end()) {
    LOG(LS_WARNING) << "RtpReceiver for track with id " << track_id
                    << " doesn't exist.";
    return nullptr;
  }
  (*it)->internal()->Stop();
  rtc::scoped_refptr<RtpReceiverInterface> receiver = *it;
  receivers_.erase(it);
  return receiver;
}

void PeerConnection::AddAudioTrack(AudioTrackInterface* track,
                                   MediaStreamInterface* stream) {
  RTC_DCHECK(!IsClosed());
  auto sender = FindSenderForTrack(track);
  if (sender != senders_.end()) {
    // We already have a sender for this track, so just change the stream_id
    // so that it's correct in the next call to CreateOffer.
    (*sender)->internal()->set_stream_id(stream->label());
    return;
  }

  // Normal case; we've never seen this track before.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> new_sender =
      RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
          signaling_thread(),
          new AudioRtpSender(track, {stream->label()},
                             session_->voice_channel(), stats_.get()));
  senders_.push_back(new_sender);
  // If the sender has already been configured in SDP, we call SetSsrc,
  // which will connect the sender to the underlying transport. This can
  // occur if a local session description that contains the ID of the sender
  // is set before AddStream is called. It can also occur if the local
  // session description is not changed and RemoveStream is called, and
  // later AddStream is called again with the same stream.
  const TrackInfo* track_info =
      FindTrackInfo(local_audio_tracks_, stream->label(), track->id());
  if (track_info) {
    new_sender->internal()->SetSsrc(track_info->ssrc);
  }
}

// TODO(deadbeef): Don't destroy RtpSenders here; they should be kept around
// indefinitely, when we have unified plan SDP.
void PeerConnection::RemoveAudioTrack(AudioTrackInterface* track,
                                      MediaStreamInterface* stream) {
  RTC_DCHECK(!IsClosed());
  auto sender = FindSenderForTrack(track);
  if (sender == senders_.end()) {
    LOG(LS_WARNING) << "RtpSender for track with id " << track->id()
                    << " doesn't exist.";
    return;
  }
  (*sender)->internal()->Stop();
  senders_.erase(sender);
}

void PeerConnection::AddVideoTrack(VideoTrackInterface* track,
                                   MediaStreamInterface* stream) {
  RTC_DCHECK(!IsClosed());
  auto sender = FindSenderForTrack(track);
  if (sender != senders_.end()) {
    // We already have a sender for this track, so just change the stream_id
    // so that it's correct in the next call to CreateOffer.
    (*sender)->internal()->set_stream_id(stream->label());
    return;
  }

  // Normal case; we've never seen this track before.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> new_sender =
      RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
          signaling_thread(), new VideoRtpSender(track, {stream->label()},
                                                 session_->video_channel()));
  senders_.push_back(new_sender);
  const TrackInfo* track_info =
      FindTrackInfo(local_video_tracks_, stream->label(), track->id());
  if (track_info) {
    new_sender->internal()->SetSsrc(track_info->ssrc);
  }
}

void PeerConnection::RemoveVideoTrack(VideoTrackInterface* track,
                                      MediaStreamInterface* stream) {
  RTC_DCHECK(!IsClosed());
  auto sender = FindSenderForTrack(track);
  if (sender == senders_.end()) {
    LOG(LS_WARNING) << "RtpSender for track with id " << track->id()
                    << " doesn't exist.";
    return;
  }
  (*sender)->internal()->Stop();
  senders_.erase(sender);
}

void PeerConnection::OnIceConnectionStateChange(
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

void PeerConnection::OnIceCandidate(
    std::unique_ptr<IceCandidateInterface> candidate) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
  observer_->OnIceCandidate(candidate.get());
}

void PeerConnection::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
  observer_->OnIceCandidatesRemoved(candidates);
}

void PeerConnection::OnIceConnectionReceivingChange(bool receiving) {
  RTC_DCHECK(signaling_thread()->IsCurrent());
  if (IsClosed()) {
    return;
  }
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
}

void PeerConnection::OnAudioTrackAdded(AudioTrackInterface* track,
                                       MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  AddAudioTrack(track, stream);
  observer_->OnRenegotiationNeeded();
}

void PeerConnection::OnAudioTrackRemoved(AudioTrackInterface* track,
                                         MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  RemoveAudioTrack(track, stream);
  observer_->OnRenegotiationNeeded();
}

void PeerConnection::OnVideoTrackAdded(VideoTrackInterface* track,
                                       MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  AddVideoTrack(track, stream);
  observer_->OnRenegotiationNeeded();
}

void PeerConnection::OnVideoTrackRemoved(VideoTrackInterface* track,
                                         MediaStreamInterface* stream) {
  if (IsClosed()) {
    return;
  }
  RemoveVideoTrack(track, stream);
  observer_->OnRenegotiationNeeded();
}

void PeerConnection::PostSetSessionDescriptionFailure(
    SetSessionDescriptionObserver* observer,
    const std::string& error) {
  SetSessionDescriptionMsg* msg = new SetSessionDescriptionMsg(observer);
  msg->error = error;
  signaling_thread()->Post(RTC_FROM_HERE, this,
                           MSG_SET_SESSIONDESCRIPTION_FAILED, msg);
}

void PeerConnection::PostCreateSessionDescriptionFailure(
    CreateSessionDescriptionObserver* observer,
    const std::string& error) {
  CreateSessionDescriptionMsg* msg = new CreateSessionDescriptionMsg(observer);
  msg->error = error;
  signaling_thread()->Post(RTC_FROM_HERE, this,
                           MSG_CREATE_SESSIONDESCRIPTION_FAILED, msg);
}

void PeerConnection::GetOptionsForOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options) {
  ExtractSharedMediaSessionOptions(rtc_options, session_options);

  // Figure out transceiver directional preferences.
  bool send_audio = HasRtpSender(cricket::MEDIA_TYPE_AUDIO);
  bool send_video = HasRtpSender(cricket::MEDIA_TYPE_VIDEO);

  // By default, generate sendrecv/recvonly m= sections.
  bool recv_audio = true;
  bool recv_video = true;

  // By default, only offer a new m= section if we have media to send with it.
  bool offer_new_audio_description = send_audio;
  bool offer_new_video_description = send_video;
  bool offer_new_data_description = HasDataChannels();

  // The "offer_to_receive_X" options allow those defaults to be overridden.
  if (rtc_options.offer_to_receive_audio != RTCOfferAnswerOptions::kUndefined) {
    recv_audio = (rtc_options.offer_to_receive_audio > 0);
    offer_new_audio_description =
        offer_new_audio_description || (rtc_options.offer_to_receive_audio > 0);
  }
  if (rtc_options.offer_to_receive_video != RTCOfferAnswerOptions::kUndefined) {
    recv_video = (rtc_options.offer_to_receive_video > 0);
    offer_new_video_description =
        offer_new_video_description || (rtc_options.offer_to_receive_video > 0);
  }

  rtc::Optional<size_t> audio_index;
  rtc::Optional<size_t> video_index;
  rtc::Optional<size_t> data_index;
  // If a current description exists, generate m= sections in the same order,
  // using the first audio/video/data section that appears and rejecting
  // extraneous ones.
  if (session_->local_description()) {
    GenerateMediaDescriptionOptions(
        session_->local_description(),
        cricket::RtpTransceiverDirection(send_audio, recv_audio),
        cricket::RtpTransceiverDirection(send_video, recv_video), &audio_index,
        &video_index, &data_index, session_options);
  }

  // Add audio/video/data m= sections to the end if needed.
  if (!audio_index && offer_new_audio_description) {
    session_options->media_description_options.push_back(
        cricket::MediaDescriptionOptions(
            cricket::MEDIA_TYPE_AUDIO, cricket::CN_AUDIO,
            cricket::RtpTransceiverDirection(send_audio, recv_audio), false));
    audio_index = rtc::Optional<size_t>(
        session_options->media_description_options.size() - 1);
  }
  if (!video_index && offer_new_video_description) {
    session_options->media_description_options.push_back(
        cricket::MediaDescriptionOptions(
            cricket::MEDIA_TYPE_VIDEO, cricket::CN_VIDEO,
            cricket::RtpTransceiverDirection(send_video, recv_video), false));
    video_index = rtc::Optional<size_t>(
        session_options->media_description_options.size() - 1);
  }
  if (!data_index && offer_new_data_description) {
    session_options->media_description_options.push_back(
        cricket::MediaDescriptionOptions(
            cricket::MEDIA_TYPE_DATA, cricket::CN_DATA,
            cricket::RtpTransceiverDirection(true, true), false));
    data_index = rtc::Optional<size_t>(
        session_options->media_description_options.size() - 1);
  }

  cricket::MediaDescriptionOptions* audio_media_description_options =
      !audio_index ? nullptr
                   : &session_options->media_description_options[*audio_index];
  cricket::MediaDescriptionOptions* video_media_description_options =
      !video_index ? nullptr
                   : &session_options->media_description_options[*video_index];
  cricket::MediaDescriptionOptions* data_media_description_options =
      !data_index ? nullptr
                  : &session_options->media_description_options[*data_index];

  // Apply ICE restart flag and renomination flag.
  for (auto& options : session_options->media_description_options) {
    options.transport_options.ice_restart = rtc_options.ice_restart;
    options.transport_options.enable_ice_renomination =
        configuration_.enable_ice_renomination;
  }

  AddRtpSenderOptions(senders_, audio_media_description_options,
                      video_media_description_options);
  AddRtpDataChannelOptions(rtp_data_channels_, data_media_description_options);

  // Intentionally unset the data channel type for RTP data channel with the
  // second condition. Otherwise the RTP data channels would be successfully
  // negotiated by default and the unit tests in WebRtcDataBrowserTest will fail
  // when building with chromium. We want to leave RTP data channels broken, so
  // people won't try to use them.
  if (!rtp_data_channels_.empty() ||
      session_->data_channel_type() != cricket::DCT_RTP) {
    session_options->data_channel_type = session_->data_channel_type();
  }

  session_options->rtcp_cname = rtcp_cname_;
  session_options->crypto_options = factory_->options().crypto_options;
}

void PeerConnection::GetOptionsForAnswer(
    const RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options) {
  ExtractSharedMediaSessionOptions(rtc_options, session_options);

  // Figure out transceiver directional preferences.
  bool send_audio = HasRtpSender(cricket::MEDIA_TYPE_AUDIO);
  bool send_video = HasRtpSender(cricket::MEDIA_TYPE_VIDEO);

  // By default, generate sendrecv/recvonly m= sections. The direction is also
  // restricted by the direction in the offer.
  bool recv_audio = true;
  bool recv_video = true;

  // The "offer_to_receive_X" options allow those defaults to be overridden.
  if (rtc_options.offer_to_receive_audio != RTCOfferAnswerOptions::kUndefined) {
    recv_audio = (rtc_options.offer_to_receive_audio > 0);
  }
  if (rtc_options.offer_to_receive_video != RTCOfferAnswerOptions::kUndefined) {
    recv_video = (rtc_options.offer_to_receive_video > 0);
  }

  rtc::Optional<size_t> audio_index;
  rtc::Optional<size_t> video_index;
  rtc::Optional<size_t> data_index;
  if (session_->remote_description()) {
    // The pending remote description should be an offer.
    RTC_DCHECK(session_->remote_description()->type() ==
               SessionDescriptionInterface::kOffer);
    // Generate m= sections that match those in the offer.
    // Note that mediasession.cc will handle intersection our preferred
    // direction with the offered direction.
    GenerateMediaDescriptionOptions(
        session_->remote_description(),
        cricket::RtpTransceiverDirection(send_audio, recv_audio),
        cricket::RtpTransceiverDirection(send_video, recv_video), &audio_index,
        &video_index, &data_index, session_options);
  }

  cricket::MediaDescriptionOptions* audio_media_description_options =
      !audio_index ? nullptr
                   : &session_options->media_description_options[*audio_index];
  cricket::MediaDescriptionOptions* video_media_description_options =
      !video_index ? nullptr
                   : &session_options->media_description_options[*video_index];
  cricket::MediaDescriptionOptions* data_media_description_options =
      !data_index ? nullptr
                  : &session_options->media_description_options[*data_index];

  // Apply ICE renomination flag.
  for (auto& options : session_options->media_description_options) {
    options.transport_options.enable_ice_renomination =
        configuration_.enable_ice_renomination;
  }

  AddRtpSenderOptions(senders_, audio_media_description_options,
                      video_media_description_options);
  AddRtpDataChannelOptions(rtp_data_channels_, data_media_description_options);

  // Intentionally unset the data channel type for RTP data channel. Otherwise
  // the RTP data channels would be successfully negotiated by default and the
  // unit tests in WebRtcDataBrowserTest will fail when building with chromium.
  // We want to leave RTP data channels broken, so people won't try to use them.
  if (!rtp_data_channels_.empty() ||
      session_->data_channel_type() != cricket::DCT_RTP) {
    session_options->data_channel_type = session_->data_channel_type();
  }

  session_options->rtcp_cname = rtcp_cname_;
  session_options->crypto_options = factory_->options().crypto_options;
}

void PeerConnection::GenerateMediaDescriptionOptions(
    const SessionDescriptionInterface* session_desc,
    cricket::RtpTransceiverDirection audio_direction,
    cricket::RtpTransceiverDirection video_direction,
    rtc::Optional<size_t>* audio_index,
    rtc::Optional<size_t>* video_index,
    rtc::Optional<size_t>* data_index,
    cricket::MediaSessionOptions* session_options) {
  for (const cricket::ContentInfo& content :
       session_desc->description()->contents()) {
    if (IsAudioContent(&content)) {
      // If we already have an audio m= section, reject this extra one.
      if (*audio_index) {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_AUDIO, content.name,
                cricket::RtpTransceiverDirection(false, false), true));
      } else {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_AUDIO, content.name, audio_direction,
                !audio_direction.send && !audio_direction.recv));
        *audio_index = rtc::Optional<size_t>(
            session_options->media_description_options.size() - 1);
      }
    } else if (IsVideoContent(&content)) {
      // If we already have an video m= section, reject this extra one.
      if (*video_index) {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_VIDEO, content.name,
                cricket::RtpTransceiverDirection(false, false), true));
      } else {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_VIDEO, content.name, video_direction,
                !video_direction.send && !video_direction.recv));
        *video_index = rtc::Optional<size_t>(
            session_options->media_description_options.size() - 1);
      }
    } else {
      RTC_DCHECK(IsDataContent(&content));
      // If we already have an data m= section, reject this extra one.
      if (*data_index) {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_DATA, content.name,
                cricket::RtpTransceiverDirection(false, false), true));
      } else {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_DATA, content.name,
                // Direction for data sections is meaningless, but legacy
                // endpoints might expect sendrecv.
                cricket::RtpTransceiverDirection(true, true), false));
        *data_index = rtc::Optional<size_t>(
            session_options->media_description_options.size() - 1);
      }
    }
  }
}

void PeerConnection::RemoveTracks(cricket::MediaType media_type) {
  UpdateLocalTracks(std::vector<cricket::StreamParams>(), media_type);
  UpdateRemoteStreamsList(std::vector<cricket::StreamParams>(), false,
                          media_type, nullptr);
}

void PeerConnection::UpdateRemoteStreamsList(
    const cricket::StreamParamsVec& streams,
    bool default_track_needed,
    cricket::MediaType media_type,
    StreamCollection* new_streams) {
  TrackInfos* current_tracks = GetRemoteTracks(media_type);

  // Find removed tracks. I.e., tracks where the track id or ssrc don't match
  // the new StreamParam.
  auto track_it = current_tracks->begin();
  while (track_it != current_tracks->end()) {
    const TrackInfo& info = *track_it;
    const cricket::StreamParams* params =
        cricket::GetStreamBySsrc(streams, info.ssrc);
    bool track_exists = params && params->id == info.track_id;
    // If this is a default track, and we still need it, don't remove it.
    if ((info.stream_label == kDefaultStreamLabel && default_track_needed) ||
        track_exists) {
      ++track_it;
    } else {
      OnRemoteTrackRemoved(info.stream_label, info.track_id, media_type);
      track_it = current_tracks->erase(track_it);
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
      stream = MediaStreamProxy::Create(rtc::Thread::Current(),
                                        MediaStream::Create(stream_label));
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

  // Add default track if necessary.
  if (default_track_needed) {
    rtc::scoped_refptr<MediaStreamInterface> default_stream =
        remote_streams_->find(kDefaultStreamLabel);
    if (!default_stream) {
      // Create the new default MediaStream.
      default_stream = MediaStreamProxy::Create(
          rtc::Thread::Current(), MediaStream::Create(kDefaultStreamLabel));
      remote_streams_->AddStream(default_stream);
      new_streams->AddStream(default_stream);
    }
    std::string default_track_id = (media_type == cricket::MEDIA_TYPE_AUDIO)
                                       ? kDefaultAudioTrackLabel
                                       : kDefaultVideoTrackLabel;
    const TrackInfo* default_track_info =
        FindTrackInfo(*current_tracks, kDefaultStreamLabel, default_track_id);
    if (!default_track_info) {
      current_tracks->push_back(
          TrackInfo(kDefaultStreamLabel, default_track_id, 0));
      OnRemoteTrackSeen(kDefaultStreamLabel, default_track_id, 0, media_type);
    }
  }
}

void PeerConnection::OnRemoteTrackSeen(const std::string& stream_label,
                                       const std::string& track_id,
                                       uint32_t ssrc,
                                       cricket::MediaType media_type) {
  MediaStreamInterface* stream = remote_streams_->find(stream_label);

  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    CreateAudioReceiver(stream, track_id, ssrc);
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    CreateVideoReceiver(stream, track_id, ssrc);
  } else {
    RTC_NOTREACHED() << "Invalid media type";
  }
}

void PeerConnection::OnRemoteTrackRemoved(const std::string& stream_label,
                                          const std::string& track_id,
                                          cricket::MediaType media_type) {
  MediaStreamInterface* stream = remote_streams_->find(stream_label);

  rtc::scoped_refptr<RtpReceiverInterface> receiver;
  if (media_type == cricket::MEDIA_TYPE_AUDIO) {
    // When the MediaEngine audio channel is destroyed, the RemoteAudioSource
    // will be notified which will end the AudioRtpReceiver::track().
    receiver = RemoveAndStopReceiver(track_id);
    rtc::scoped_refptr<AudioTrackInterface> audio_track =
        stream->FindAudioTrack(track_id);
    if (audio_track) {
      stream->RemoveTrack(audio_track);
    }
  } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
    // Stopping or destroying a VideoRtpReceiver will end the
    // VideoRtpReceiver::track().
    receiver = RemoveAndStopReceiver(track_id);
    rtc::scoped_refptr<VideoTrackInterface> video_track =
        stream->FindVideoTrack(track_id);
    if (video_track) {
      // There's no guarantee the track is still available, e.g. the track may
      // have been removed from the stream by an application.
      stream->RemoveTrack(video_track);
    }
  } else {
    RTC_NOTREACHED() << "Invalid media type";
  }
  if (receiver) {
    observer_->OnRemoveTrack(receiver);
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

  for (auto& stream : streams_to_remove) {
    remote_streams_->RemoveStream(stream);
    observer_->OnRemoveStream(std::move(stream));
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
  RtpSenderInternal* sender = FindSenderById(track_id);
  if (!sender) {
    LOG(LS_WARNING) << "An unknown RtpSender with id " << track_id
                    << " has been configured in the local description.";
    return;
  }

  if (sender->media_type() != media_type) {
    LOG(LS_WARNING) << "An RtpSender has been configured in the local"
                    << " description with an unexpected media type.";
    return;
  }

  sender->set_stream_id(stream_label);
  sender->SetSsrc(ssrc);
}

void PeerConnection::OnLocalTrackRemoved(const std::string& stream_label,
                                         const std::string& track_id,
                                         uint32_t ssrc,
                                         cricket::MediaType media_type) {
  RtpSenderInternal* sender = FindSenderById(track_id);
  if (!sender) {
    // This is the normal case. I.e., RemoveStream has been called and the
    // SessionDescriptions has been renegotiated.
    return;
  }

  // A sender has been removed from the SessionDescription but it's still
  // associated with the PeerConnection. This only occurs if the SDP doesn't
  // match with the calls to CreateSender, AddStream and RemoveStream.
  if (sender->media_type() != media_type) {
    LOG(LS_WARNING) << "An RtpSender has been configured in the local"
                    << " description with an unexpected media type.";
    return;
  }

  sender->SetSsrc(0);
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
    if (data_channel_it == rtp_data_channels_.end()) {
      LOG(LS_ERROR) << "channel label not found";
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
  rtc::scoped_refptr<DataChannelInterface> proxy_channel =
      DataChannelProxy::Create(signaling_thread(), channel);
  observer_->OnDataChannel(std::move(proxy_channel));
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
      if ((session_->GetSctpSslRole(&role)) &&
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
      session_, session_->data_channel_type(), label, new_config));
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

  SignalDataChannelCreated(channel.get());
  return channel;
}

bool PeerConnection::HasDataChannels() const {
#ifdef HAVE_QUIC
  return !rtp_data_channels_.empty() || !sctp_data_channels_.empty() ||
         (session_->quic_data_transport() &&
          session_->quic_data_transport()->HasDataChannels());
#else
  return !rtp_data_channels_.empty() || !sctp_data_channels_.empty();
#endif  // HAVE_QUIC
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
  RTC_DCHECK(signaling_thread()->IsCurrent());
  for (auto it = sctp_data_channels_.begin(); it != sctp_data_channels_.end();
       ++it) {
    if (it->get() == channel) {
      if (channel->id() >= 0) {
        sid_allocator_.ReleaseSid(channel->id());
      }
      // Since this method is triggered by a signal from the DataChannel,
      // we can't free it directly here; we need to free it asynchronously.
      sctp_data_channels_to_free_.push_back(*it);
      sctp_data_channels_.erase(it);
      signaling_thread()->Post(RTC_FROM_HERE, this, MSG_FREE_DATACHANNELS,
                               nullptr);
      return;
    }
  }
}

void PeerConnection::OnVoiceChannelCreated() {
  SetChannelOnSendersAndReceivers<AudioRtpSender, AudioRtpReceiver>(
      session_->voice_channel(), senders_, receivers_,
      cricket::MEDIA_TYPE_AUDIO);
}

void PeerConnection::OnVoiceChannelDestroyed() {
  SetChannelOnSendersAndReceivers<AudioRtpSender, AudioRtpReceiver,
                                  cricket::VoiceChannel>(
      nullptr, senders_, receivers_, cricket::MEDIA_TYPE_AUDIO);
}

void PeerConnection::OnVideoChannelCreated() {
  SetChannelOnSendersAndReceivers<VideoRtpSender, VideoRtpReceiver>(
      session_->video_channel(), senders_, receivers_,
      cricket::MEDIA_TYPE_VIDEO);
}

void PeerConnection::OnVideoChannelDestroyed() {
  SetChannelOnSendersAndReceivers<VideoRtpSender, VideoRtpReceiver,
                                  cricket::VideoChannel>(
      nullptr, senders_, receivers_, cricket::MEDIA_TYPE_VIDEO);
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

  rtc::scoped_refptr<DataChannelInterface> proxy_channel =
      DataChannelProxy::Create(signaling_thread(), channel);
  observer_->OnDataChannel(std::move(proxy_channel));
}

bool PeerConnection::HasRtpSender(cricket::MediaType type) const {
  return std::find_if(
             senders_.begin(), senders_.end(),
             [type](const rtc::scoped_refptr<
                    RtpSenderProxyWithInternal<RtpSenderInternal>>& sender) {
               return sender->media_type() == type;
             }) != senders_.end();
}

RtpSenderInternal* PeerConnection::FindSenderById(const std::string& id) {
  auto it = std::find_if(
      senders_.begin(), senders_.end(),
      [id](const rtc::scoped_refptr<
           RtpSenderProxyWithInternal<RtpSenderInternal>>& sender) {
        return sender->id() == id;
      });
  return it != senders_.end() ? (*it)->internal() : nullptr;
}

std::vector<
    rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>::iterator
PeerConnection::FindSenderForTrack(MediaStreamTrackInterface* track) {
  return std::find_if(
      senders_.begin(), senders_.end(),
      [track](const rtc::scoped_refptr<
              RtpSenderProxyWithInternal<RtpSenderInternal>>& sender) {
        return sender->track() == track;
      });
}

std::vector<rtc::scoped_refptr<
    RtpReceiverProxyWithInternal<RtpReceiverInternal>>>::iterator
PeerConnection::FindReceiverForTrack(const std::string& track_id) {
  return std::find_if(
      receivers_.begin(), receivers_.end(),
      [track_id](const rtc::scoped_refptr<
                 RtpReceiverProxyWithInternal<RtpReceiverInternal>>& receiver) {
        return receiver->id() == track_id;
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

bool PeerConnection::InitializePortAllocator_n(
    const RTCConfiguration& configuration) {
  cricket::ServerAddresses stun_servers;
  std::vector<cricket::RelayServerConfig> turn_servers;
  if (ParseIceServers(configuration.servers, &stun_servers, &turn_servers) !=
      RTCErrorType::NONE) {
    return false;
  }

  port_allocator_->Initialize();

  // To handle both internal and externally created port allocator, we will
  // enable BUNDLE here.
  int portallocator_flags = port_allocator_->flags();
  portallocator_flags |= cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                         cricket::PORTALLOCATOR_ENABLE_IPV6 |
                         cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI;
  // If the disable-IPv6 flag was specified, we'll not override it
  // by experiment.
  if (configuration.disable_ipv6) {
    portallocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
  } else if (webrtc::field_trial::FindFullName("WebRTC-IPv6Default")
                 .find("Disabled") == 0) {
    portallocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6);
  }

  if (configuration.disable_ipv6_on_wifi) {
    portallocator_flags &= ~(cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI);
    LOG(LS_INFO) << "IPv6 candidates on Wi-Fi are disabled.";
  }

  if (configuration.tcp_candidate_policy == kTcpCandidatePolicyDisabled) {
    portallocator_flags |= cricket::PORTALLOCATOR_DISABLE_TCP;
    LOG(LS_INFO) << "TCP candidates are disabled.";
  }

  if (configuration.candidate_network_policy ==
      kCandidateNetworkPolicyLowCost) {
    portallocator_flags |= cricket::PORTALLOCATOR_DISABLE_COSTLY_NETWORKS;
    LOG(LS_INFO) << "Do not gather candidates on high-cost networks";
  }

  port_allocator_->set_flags(portallocator_flags);
  // No step delay is used while allocating ports.
  port_allocator_->set_step_delay(cricket::kMinimumStepDelay);
  port_allocator_->set_candidate_filter(
      ConvertIceTransportTypeToCandidateFilter(configuration.type));
  port_allocator_->set_max_ipv6_networks(configuration.max_ipv6_networks);

  // Call this last since it may create pooled allocator sessions using the
  // properties set above.
  port_allocator_->SetConfiguration(stun_servers, turn_servers,
                                    configuration.ice_candidate_pool_size,
                                    configuration.prune_turn_ports,
                                    configuration.turn_customizer);
  return true;
}

bool PeerConnection::ReconfigurePortAllocator_n(
    const cricket::ServerAddresses& stun_servers,
    const std::vector<cricket::RelayServerConfig>& turn_servers,
    IceTransportsType type,
    int candidate_pool_size,
    bool prune_turn_ports,
    webrtc::TurnCustomizer* turn_customizer) {
  port_allocator_->set_candidate_filter(
      ConvertIceTransportTypeToCandidateFilter(type));
  // Call this last since it may create pooled allocator sessions using the
  // candidate filter set above.
  return port_allocator_->SetConfiguration(
      stun_servers, turn_servers, candidate_pool_size, prune_turn_ports,
      turn_customizer);
}

bool PeerConnection::StartRtcEventLog_w(rtc::PlatformFile file,
                                        int64_t max_size_bytes) {
  if (!event_log_) {
    return false;
  }

  // TODO(eladalon): It would be better to not allow negative values into PC.
  const size_t max_size = (max_size_bytes < 0)
                              ? RtcEventLog::kUnlimitedOutput
                              : rtc::saturated_cast<size_t>(max_size_bytes);

  return event_log_->StartLogging(
      rtc::MakeUnique<RtcEventLogOutputFile>(file, max_size));
}

void PeerConnection::StopRtcEventLog_w() {
  if (event_log_) {
    event_log_->StopLogging();
  }
}

}  // namespace webrtc
