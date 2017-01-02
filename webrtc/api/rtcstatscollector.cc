/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/rtcstatscollector.h"

#include <memory>
#include <utility>
#include <vector>

#include "webrtc/api/peerconnection.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/webrtcsession.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/p2pconstants.h"
#include "webrtc/p2p/base/port.h"

namespace webrtc {

namespace {

std::string RTCCertificateIDFromFingerprint(const std::string& fingerprint) {
  return "RTCCertificate_" + fingerprint;
}

std::string RTCCodecStatsIDFromDirectionMediaAndPayload(
    bool inbound, bool audio, uint32_t payload_type) {
  // TODO(hbos): When we are able to handle multiple m= lines of the same media
  // type (and multiple BaseChannels for the same type is possible?) this needs
  // to be updated to differentiate the transport being used, and stats need to
  // be collected for all of them. crbug.com/659117
  if (inbound) {
    return audio ? "RTCCodec_InboundAudio_" + rtc::ToString<>(payload_type)
                 : "RTCCodec_InboundVideo_" + rtc::ToString<>(payload_type);
  }
  return audio ? "RTCCodec_OutboundAudio_" + rtc::ToString<>(payload_type)
               : "RTCCodec_OutboundVideo_" + rtc::ToString<>(payload_type);
}

std::string RTCIceCandidatePairStatsIDFromConnectionInfo(
    const cricket::ConnectionInfo& info) {
  return "RTCIceCandidatePair_" + info.local_candidate.id() + "_" +
      info.remote_candidate.id();
}

std::string RTCMediaStreamTrackStatsIDFromMediaStreamTrackInterface(
    const MediaStreamTrackInterface& track) {
  return "RTCMediaStreamTrack_" + track.id();
}

std::string RTCTransportStatsIDFromTransportChannel(
    const std::string& transport_name, int channel_component) {
  return "RTCTransport_" + transport_name + "_" +
      rtc::ToString<>(channel_component);
}

std::string RTCTransportStatsIDFromBaseChannel(
    const ProxyTransportMap& proxy_to_transport,
    const cricket::BaseChannel& base_channel) {
  auto proxy_it = proxy_to_transport.find(base_channel.content_name());
  if (proxy_it == proxy_to_transport.cend())
    return "";
  return RTCTransportStatsIDFromTransportChannel(
      proxy_it->second, cricket::ICE_CANDIDATE_COMPONENT_RTP);
}

std::string RTCInboundRTPStreamStatsIDFromSSRC(bool audio, uint32_t ssrc) {
  return audio ? "RTCInboundRTPAudioStream_" + rtc::ToString<>(ssrc)
               : "RTCInboundRTPVideoStream_" + rtc::ToString<>(ssrc);
}

std::string RTCOutboundRTPStreamStatsIDFromSSRC(bool audio, uint32_t ssrc) {
  return audio ? "RTCOutboundRTPAudioStream_" + rtc::ToString<>(ssrc)
               : "RTCOutboundRTPVideoStream_" + rtc::ToString<>(ssrc);
}

const char* CandidateTypeToRTCIceCandidateType(const std::string& type) {
  if (type == cricket::LOCAL_PORT_TYPE)
    return RTCIceCandidateType::kHost;
  if (type == cricket::STUN_PORT_TYPE)
    return RTCIceCandidateType::kSrflx;
  if (type == cricket::PRFLX_PORT_TYPE)
    return RTCIceCandidateType::kPrflx;
  if (type == cricket::RELAY_PORT_TYPE)
    return RTCIceCandidateType::kRelay;
  RTC_NOTREACHED();
  return nullptr;
}

const char* DataStateToRTCDataChannelState(
    DataChannelInterface::DataState state) {
  switch (state) {
    case DataChannelInterface::kConnecting:
      return RTCDataChannelState::kConnecting;
    case DataChannelInterface::kOpen:
      return RTCDataChannelState::kOpen;
    case DataChannelInterface::kClosing:
      return RTCDataChannelState::kClosing;
    case DataChannelInterface::kClosed:
      return RTCDataChannelState::kClosed;
    default:
      RTC_NOTREACHED();
      return nullptr;
  }
}

const char* IceCandidatePairStateToRTCStatsIceCandidatePairState(
    cricket::IceCandidatePairState state) {
  switch (state) {
    case cricket::IceCandidatePairState::WAITING:
      return RTCStatsIceCandidatePairState::kWaiting;
    case cricket::IceCandidatePairState::IN_PROGRESS:
      return RTCStatsIceCandidatePairState::kInProgress;
    case cricket::IceCandidatePairState::SUCCEEDED:
      return RTCStatsIceCandidatePairState::kSucceeded;
    case cricket::IceCandidatePairState::FAILED:
      return RTCStatsIceCandidatePairState::kFailed;
    default:
      RTC_NOTREACHED();
      return nullptr;
  }
}

std::unique_ptr<RTCCodecStats> CodecStatsFromRtpCodecParameters(
    uint64_t timestamp_us, bool inbound, bool audio,
    const RtpCodecParameters& codec_params) {
  RTC_DCHECK_GE(codec_params.payload_type, 0);
  RTC_DCHECK_LE(codec_params.payload_type, 127);
  uint32_t payload_type = static_cast<uint32_t>(codec_params.payload_type);
  std::unique_ptr<RTCCodecStats> codec_stats(new RTCCodecStats(
      RTCCodecStatsIDFromDirectionMediaAndPayload(inbound, audio, payload_type),
      timestamp_us));
  codec_stats->payload_type = payload_type;
  codec_stats->codec = (audio ? "audio/" : "video/") + codec_params.mime_type;
  codec_stats->clock_rate = static_cast<uint32_t>(codec_params.clock_rate);
  return codec_stats;
}

void SetMediaStreamTrackStatsFromMediaStreamTrackInterface(
    const MediaStreamTrackInterface& track,
    RTCMediaStreamTrackStats* track_stats) {
  track_stats->track_identifier = track.id();
  track_stats->ended = (track.state() == MediaStreamTrackInterface::kEnded);
}

// Provides the media independent counters (both audio and video).
void SetInboundRTPStreamStatsFromMediaReceiverInfo(
    const cricket::MediaReceiverInfo& media_receiver_info,
    RTCInboundRTPStreamStats* inbound_stats) {
  RTC_DCHECK(inbound_stats);
  inbound_stats->ssrc = rtc::ToString<>(media_receiver_info.ssrc());
  // TODO(hbos): Support the remote case. crbug.com/657855
  inbound_stats->is_remote = false;
  // TODO(hbos): Set |codec_id| when we have |RTCCodecStats|. Maybe relevant:
  // |media_receiver_info.codec_name|. crbug.com/657854, 657855, 659117
  inbound_stats->packets_received =
      static_cast<uint32_t>(media_receiver_info.packets_rcvd);
  inbound_stats->bytes_received =
      static_cast<uint64_t>(media_receiver_info.bytes_rcvd);
  inbound_stats->packets_lost =
      static_cast<uint32_t>(media_receiver_info.packets_lost);
  inbound_stats->fraction_lost =
      static_cast<double>(media_receiver_info.fraction_lost);
}

void SetInboundRTPStreamStatsFromVoiceReceiverInfo(
    const cricket::VoiceReceiverInfo& voice_receiver_info,
    RTCInboundRTPStreamStats* inbound_audio) {
  SetInboundRTPStreamStatsFromMediaReceiverInfo(
      voice_receiver_info, inbound_audio);
  inbound_audio->media_type = "audio";
  inbound_audio->jitter =
      static_cast<double>(voice_receiver_info.jitter_ms) /
          rtc::kNumMillisecsPerSec;
  // |fir_count|, |pli_count| and |sli_count| are only valid for video and are
  // purposefully left undefined for audio.
}

void SetInboundRTPStreamStatsFromVideoReceiverInfo(
    const cricket::VideoReceiverInfo& video_receiver_info,
    RTCInboundRTPStreamStats* inbound_video) {
  SetInboundRTPStreamStatsFromMediaReceiverInfo(
      video_receiver_info, inbound_video);
  inbound_video->media_type = "video";
  inbound_video->fir_count =
      static_cast<uint32_t>(video_receiver_info.firs_sent);
  inbound_video->pli_count =
      static_cast<uint32_t>(video_receiver_info.plis_sent);
  inbound_video->nack_count =
      static_cast<uint32_t>(video_receiver_info.nacks_sent);
  inbound_video->frames_decoded = video_receiver_info.frames_decoded;
}

// Provides the media independent counters (both audio and video).
void SetOutboundRTPStreamStatsFromMediaSenderInfo(
    const cricket::MediaSenderInfo& media_sender_info,
    RTCOutboundRTPStreamStats* outbound_stats) {
  RTC_DCHECK(outbound_stats);
  outbound_stats->ssrc = rtc::ToString<>(media_sender_info.ssrc());
  // TODO(hbos): Support the remote case. crbug.com/657856
  outbound_stats->is_remote = false;
  // TODO(hbos): Set |codec_id| when we have |RTCCodecStats|. Maybe relevant:
  // |media_sender_info.codec_name|. crbug.com/657854, 657856, 659117
  outbound_stats->packets_sent =
      static_cast<uint32_t>(media_sender_info.packets_sent);
  outbound_stats->bytes_sent =
      static_cast<uint64_t>(media_sender_info.bytes_sent);
  if (media_sender_info.rtt_ms >= 0) {
    outbound_stats->round_trip_time = static_cast<double>(
        media_sender_info.rtt_ms) / rtc::kNumMillisecsPerSec;
  }
}

void SetOutboundRTPStreamStatsFromVoiceSenderInfo(
    const cricket::VoiceSenderInfo& voice_sender_info,
    RTCOutboundRTPStreamStats* outbound_audio) {
  SetOutboundRTPStreamStatsFromMediaSenderInfo(
      voice_sender_info, outbound_audio);
  outbound_audio->media_type = "audio";
  // |fir_count|, |pli_count| and |sli_count| are only valid for video and are
  // purposefully left undefined for audio.
}

void SetOutboundRTPStreamStatsFromVideoSenderInfo(
    const cricket::VideoSenderInfo& video_sender_info,
    RTCOutboundRTPStreamStats* outbound_video) {
  SetOutboundRTPStreamStatsFromMediaSenderInfo(
      video_sender_info, outbound_video);
  outbound_video->media_type = "video";
  outbound_video->fir_count =
      static_cast<uint32_t>(video_sender_info.firs_rcvd);
  outbound_video->pli_count =
      static_cast<uint32_t>(video_sender_info.plis_rcvd);
  outbound_video->nack_count =
      static_cast<uint32_t>(video_sender_info.nacks_rcvd);
  if (video_sender_info.qp_sum)
    outbound_video->qp_sum = *video_sender_info.qp_sum;
  outbound_video->frames_encoded = video_sender_info.frames_encoded;
}

void ProduceCertificateStatsFromSSLCertificateStats(
    int64_t timestamp_us, const rtc::SSLCertificateStats& certificate_stats,
    RTCStatsReport* report) {
  RTCCertificateStats* prev_certificate_stats = nullptr;
  for (const rtc::SSLCertificateStats* s = &certificate_stats; s;
       s = s->issuer.get()) {
    std::string certificate_stats_id =
        RTCCertificateIDFromFingerprint(s->fingerprint);
    // It is possible for the same certificate to show up multiple times, e.g.
    // if local and remote side use the same certificate in a loopback call.
    // If the report already contains stats for this certificate, skip it.
    if (report->Get(certificate_stats_id)) {
      RTC_DCHECK_EQ(s, &certificate_stats);
      break;
    }
    RTCCertificateStats* certificate_stats = new RTCCertificateStats(
        certificate_stats_id, timestamp_us);
    certificate_stats->fingerprint = s->fingerprint;
    certificate_stats->fingerprint_algorithm = s->fingerprint_algorithm;
    certificate_stats->base64_certificate = s->base64_certificate;
    if (prev_certificate_stats)
      prev_certificate_stats->issuer_certificate_id = certificate_stats->id();
    report->AddStats(std::unique_ptr<RTCCertificateStats>(certificate_stats));
    prev_certificate_stats = certificate_stats;
  }
}

const std::string& ProduceIceCandidateStats(
    int64_t timestamp_us, const cricket::Candidate& candidate, bool is_local,
    const std::string& transport_id, RTCStatsReport* report) {
  const std::string& id = "RTCIceCandidate_" + candidate.id();
  const RTCStats* stats = report->Get(id);
  if (!stats) {
    std::unique_ptr<RTCIceCandidateStats> candidate_stats;
    if (is_local)
      candidate_stats.reset(new RTCLocalIceCandidateStats(id, timestamp_us));
    else
      candidate_stats.reset(new RTCRemoteIceCandidateStats(id, timestamp_us));
    candidate_stats->transport_id = transport_id;
    candidate_stats->ip = candidate.address().ipaddr().ToString();
    candidate_stats->port = static_cast<int32_t>(candidate.address().port());
    candidate_stats->protocol = candidate.protocol();
    candidate_stats->candidate_type = CandidateTypeToRTCIceCandidateType(
        candidate.type());
    candidate_stats->priority = static_cast<int32_t>(candidate.priority());

    stats = candidate_stats.get();
    report->AddStats(std::move(candidate_stats));
  }
  RTC_DCHECK_EQ(stats->type(), is_local ? RTCLocalIceCandidateStats::kType
                                        : RTCRemoteIceCandidateStats::kType);
  return stats->id();
}

void ProduceMediaStreamAndTrackStats(
    int64_t timestamp_us,
    rtc::scoped_refptr<StreamCollectionInterface> streams,
    bool is_local,
    RTCStatsReport* report) {
  // TODO(hbos): When "AddTrack" is implemented we should iterate tracks to
  // find which streams exist, not iterate streams to find tracks.
  // crbug.com/659137
  // TODO(hbos): Return stats of detached tracks. We have to perform stats
  // gathering at the time of detachment to get accurate stats and timestamps.
  // crbug.com/659137
  if (!streams)
    return;
  for (size_t i = 0; i < streams->count(); ++i) {
    MediaStreamInterface* stream = streams->at(i);

    std::unique_ptr<RTCMediaStreamStats> stream_stats(
        new RTCMediaStreamStats(
            (is_local ? "RTCMediaStream_local_" : "RTCMediaStream_remote_") +
            stream->label(), timestamp_us));
    stream_stats->stream_identifier = stream->label();
    stream_stats->track_ids = std::vector<std::string>();
    // Audio Tracks
    for (const rtc::scoped_refptr<AudioTrackInterface>& audio_track :
         stream->GetAudioTracks()) {
      std::string id = RTCMediaStreamTrackStatsIDFromMediaStreamTrackInterface(
        *audio_track.get());
      if (report->Get(id)) {
        // Skip track, stats already exist for it.
        continue;
      }
      std::unique_ptr<RTCMediaStreamTrackStats> audio_track_stats(
          new RTCMediaStreamTrackStats(id, timestamp_us));
      stream_stats->track_ids->push_back(audio_track_stats->id());
      SetMediaStreamTrackStatsFromMediaStreamTrackInterface(
          *audio_track.get(),
          audio_track_stats.get());
      audio_track_stats->remote_source = !is_local;
      audio_track_stats->detached = false;
      int signal_level;
      if (audio_track->GetSignalLevel(&signal_level)) {
        // Convert signal level from [0,32767] int to [0,1] double.
        RTC_DCHECK_GE(signal_level, 0);
        RTC_DCHECK_LE(signal_level, 32767);
        audio_track_stats->audio_level = signal_level / 32767.0;
      }
      if (audio_track->GetAudioProcessor()) {
        AudioProcessorInterface::AudioProcessorStats audio_processor_stats;
        audio_track->GetAudioProcessor()->GetStats(&audio_processor_stats);
        if (audio_processor_stats.echo_return_loss != -100) {
          audio_track_stats->echo_return_loss = static_cast<double>(
              audio_processor_stats.echo_return_loss);
        }
        if (audio_processor_stats.echo_return_loss_enhancement != -100) {
          audio_track_stats->echo_return_loss_enhancement = static_cast<double>(
              audio_processor_stats.echo_return_loss_enhancement);
        }
      }
      report->AddStats(std::move(audio_track_stats));
    }
    // Video Tracks
    for (const rtc::scoped_refptr<VideoTrackInterface>& video_track :
         stream->GetVideoTracks()) {
      std::string id = RTCMediaStreamTrackStatsIDFromMediaStreamTrackInterface(
          *video_track.get());
      if (report->Get(id)) {
        // Skip track, stats already exist for it.
        continue;
      }
      std::unique_ptr<RTCMediaStreamTrackStats> video_track_stats(
          new RTCMediaStreamTrackStats(id, timestamp_us));
      stream_stats->track_ids->push_back(video_track_stats->id());
      SetMediaStreamTrackStatsFromMediaStreamTrackInterface(
          *video_track.get(),
          video_track_stats.get());
      video_track_stats->remote_source = !is_local;
      video_track_stats->detached = false;
      if (video_track->GetSource()) {
        VideoTrackSourceInterface::Stats video_track_source_stats;
        if (video_track->GetSource()->GetStats(&video_track_source_stats)) {
          video_track_stats->frame_width = static_cast<uint32_t>(
              video_track_source_stats.input_width);
          video_track_stats->frame_height = static_cast<uint32_t>(
              video_track_source_stats.input_height);
        }
      }
      report->AddStats(std::move(video_track_stats));
    }
    report->AddStats(std::move(stream_stats));
  }
}

}  // namespace

rtc::scoped_refptr<RTCStatsCollector> RTCStatsCollector::Create(
    PeerConnection* pc, int64_t cache_lifetime_us) {
  return rtc::scoped_refptr<RTCStatsCollector>(
      new rtc::RefCountedObject<RTCStatsCollector>(pc, cache_lifetime_us));
}

RTCStatsCollector::RTCStatsCollector(PeerConnection* pc,
                                     int64_t cache_lifetime_us)
    : pc_(pc),
      signaling_thread_(pc->session()->signaling_thread()),
      worker_thread_(pc->session()->worker_thread()),
      network_thread_(pc->session()->network_thread()),
      num_pending_partial_reports_(0),
      partial_report_timestamp_us_(0),
      cache_timestamp_us_(0),
      cache_lifetime_us_(cache_lifetime_us) {
  RTC_DCHECK(pc_);
  RTC_DCHECK(signaling_thread_);
  RTC_DCHECK(worker_thread_);
  RTC_DCHECK(network_thread_);
  RTC_DCHECK_GE(cache_lifetime_us_, 0);
  pc_->SignalDataChannelCreated.connect(
      this, &RTCStatsCollector::OnDataChannelCreated);
}

RTCStatsCollector::~RTCStatsCollector() {
  RTC_DCHECK_EQ(num_pending_partial_reports_, 0);
}

void RTCStatsCollector::GetStatsReport(
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  RTC_DCHECK(callback);
  callbacks_.push_back(callback);

  // "Now" using a monotonically increasing timer.
  int64_t cache_now_us = rtc::TimeMicros();
  if (cached_report_ &&
      cache_now_us - cache_timestamp_us_ <= cache_lifetime_us_) {
    // We have a fresh cached report to deliver.
    DeliverCachedReport();
  } else if (!num_pending_partial_reports_) {
    // Only start gathering stats if we're not already gathering stats. In the
    // case of already gathering stats, |callback_| will be invoked when there
    // are no more pending partial reports.

    // "Now" using a system clock, relative to the UNIX epoch (Jan 1, 1970,
    // UTC), in microseconds. The system clock could be modified and is not
    // necessarily monotonically increasing.
    int64_t timestamp_us = rtc::TimeUTCMicros();

    num_pending_partial_reports_ = 2;
    partial_report_timestamp_us_ = cache_now_us;

    // Prepare |channel_names_| and |media_info_| for use in
    // |ProducePartialResultsOnNetworkThread|.
    channel_name_pairs_.reset(new ChannelNamePairs());
    if (pc_->session()->voice_channel()) {
      channel_name_pairs_->voice = rtc::Optional<ChannelNamePair>(
          ChannelNamePair(pc_->session()->voice_channel()->content_name(),
                          pc_->session()->voice_channel()->transport_name()));
    }
    if (pc_->session()->video_channel()) {
      channel_name_pairs_->video = rtc::Optional<ChannelNamePair>(
          ChannelNamePair(pc_->session()->video_channel()->content_name(),
                          pc_->session()->video_channel()->transport_name()));
    }
    if (pc_->session()->data_channel()) {
      channel_name_pairs_->data = rtc::Optional<ChannelNamePair>(
          ChannelNamePair(pc_->session()->data_channel()->content_name(),
                          pc_->session()->data_channel()->transport_name()));
    }
    media_info_.reset(PrepareMediaInfo_s().release());

    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, network_thread_,
        rtc::Bind(&RTCStatsCollector::ProducePartialResultsOnNetworkThread,
            rtc::scoped_refptr<RTCStatsCollector>(this), timestamp_us));
    ProducePartialResultsOnSignalingThread(timestamp_us);
  }
}

void RTCStatsCollector::ClearCachedStatsReport() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  cached_report_ = nullptr;
}

void RTCStatsCollector::WaitForPendingRequest() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  if (num_pending_partial_reports_) {
    rtc::Thread::Current()->ProcessMessages(0);
    while (num_pending_partial_reports_) {
      rtc::Thread::Current()->SleepMs(1);
      rtc::Thread::Current()->ProcessMessages(0);
    }
  }
}

void RTCStatsCollector::ProducePartialResultsOnSignalingThread(
    int64_t timestamp_us) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<RTCStatsReport> report = RTCStatsReport::Create(
      timestamp_us);

  ProduceDataChannelStats_s(timestamp_us, report.get());
  ProduceMediaStreamAndTrackStats_s(timestamp_us, report.get());
  ProducePeerConnectionStats_s(timestamp_us, report.get());

  AddPartialResults(report);
}

void RTCStatsCollector::ProducePartialResultsOnNetworkThread(
    int64_t timestamp_us) {
  RTC_DCHECK(network_thread_->IsCurrent());
  rtc::scoped_refptr<RTCStatsReport> report = RTCStatsReport::Create(
      timestamp_us);

  std::unique_ptr<SessionStats> session_stats =
      pc_->session()->GetStats(*channel_name_pairs_);
  if (session_stats) {
    std::map<std::string, CertificateStatsPair> transport_cert_stats =
        PrepareTransportCertificateStats_n(*session_stats);

    ProduceCertificateStats_n(
        timestamp_us, transport_cert_stats, report.get());
    ProduceCodecStats_n(
        timestamp_us, *media_info_, report.get());
    ProduceIceCandidateAndPairStats_n(
        timestamp_us, *session_stats, report.get());
    ProduceRTPStreamStats_n(
        timestamp_us, *session_stats, *media_info_, report.get());
    ProduceTransportStats_n(
        timestamp_us, *session_stats, transport_cert_stats, report.get());
  }

  AddPartialResults(report);
}

void RTCStatsCollector::AddPartialResults(
    const rtc::scoped_refptr<RTCStatsReport>& partial_report) {
  if (!signaling_thread_->IsCurrent()) {
    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, signaling_thread_,
        rtc::Bind(&RTCStatsCollector::AddPartialResults_s,
                  rtc::scoped_refptr<RTCStatsCollector>(this),
                  partial_report));
    return;
  }
  AddPartialResults_s(partial_report);
}

void RTCStatsCollector::AddPartialResults_s(
    rtc::scoped_refptr<RTCStatsReport> partial_report) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  RTC_DCHECK_GT(num_pending_partial_reports_, 0);
  if (!partial_report_)
    partial_report_ = partial_report;
  else
    partial_report_->TakeMembersFrom(partial_report);
  --num_pending_partial_reports_;
  if (!num_pending_partial_reports_) {
    cache_timestamp_us_ = partial_report_timestamp_us_;
    cached_report_ = partial_report_;
    partial_report_ = nullptr;
    DeliverCachedReport();
  }
}

void RTCStatsCollector::DeliverCachedReport() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  RTC_DCHECK(!callbacks_.empty());
  RTC_DCHECK(cached_report_);
  for (const rtc::scoped_refptr<RTCStatsCollectorCallback>& callback :
       callbacks_) {
    callback->OnStatsDelivered(cached_report_);
  }
  callbacks_.clear();
}

void RTCStatsCollector::ProduceCertificateStats_n(
    int64_t timestamp_us,
    const std::map<std::string, CertificateStatsPair>& transport_cert_stats,
    RTCStatsReport* report) const {
  RTC_DCHECK(network_thread_->IsCurrent());
  for (const auto& transport_cert_stats_pair : transport_cert_stats) {
    if (transport_cert_stats_pair.second.local) {
      ProduceCertificateStatsFromSSLCertificateStats(
          timestamp_us, *transport_cert_stats_pair.second.local.get(), report);
    }
    if (transport_cert_stats_pair.second.remote) {
      ProduceCertificateStatsFromSSLCertificateStats(
          timestamp_us, *transport_cert_stats_pair.second.remote.get(), report);
    }
  }
}

void RTCStatsCollector::ProduceCodecStats_n(
    int64_t timestamp_us, const MediaInfo& media_info,
    RTCStatsReport* report) const {
  RTC_DCHECK(network_thread_->IsCurrent());
  // Audio
  if (media_info.voice) {
    // Inbound
    for (const auto& pair : media_info.voice->receive_codecs) {
      report->AddStats(CodecStatsFromRtpCodecParameters(
          timestamp_us, true, true, pair.second));
    }
    // Outbound
    for (const auto& pair : media_info.voice->send_codecs) {
      report->AddStats(CodecStatsFromRtpCodecParameters(
          timestamp_us, false, true, pair.second));
    }
  }
  // Video
  if (media_info.video) {
    // Inbound
    for (const auto& pair : media_info.video->receive_codecs) {
      report->AddStats(CodecStatsFromRtpCodecParameters(
          timestamp_us, true, false, pair.second));
    }
    // Outbound
    for (const auto& pair : media_info.video->send_codecs) {
      report->AddStats(CodecStatsFromRtpCodecParameters(
          timestamp_us, false, false, pair.second));
    }
  }
}

void RTCStatsCollector::ProduceDataChannelStats_s(
    int64_t timestamp_us, RTCStatsReport* report) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  for (const rtc::scoped_refptr<DataChannel>& data_channel :
       pc_->sctp_data_channels()) {
    std::unique_ptr<RTCDataChannelStats> data_channel_stats(
        new RTCDataChannelStats(
            "RTCDataChannel_" + rtc::ToString<>(data_channel->id()),
            timestamp_us));
    data_channel_stats->label = data_channel->label();
    data_channel_stats->protocol = data_channel->protocol();
    data_channel_stats->datachannelid = data_channel->id();
    data_channel_stats->state =
        DataStateToRTCDataChannelState(data_channel->state());
    data_channel_stats->messages_sent = data_channel->messages_sent();
    data_channel_stats->bytes_sent = data_channel->bytes_sent();
    data_channel_stats->messages_received = data_channel->messages_received();
    data_channel_stats->bytes_received = data_channel->bytes_received();
    report->AddStats(std::move(data_channel_stats));
  }
}

void RTCStatsCollector::ProduceIceCandidateAndPairStats_n(
      int64_t timestamp_us, const SessionStats& session_stats,
      RTCStatsReport* report) const {
  RTC_DCHECK(network_thread_->IsCurrent());
  for (const auto& transport_stats : session_stats.transport_stats) {
    for (const auto& channel_stats : transport_stats.second.channel_stats) {
      std::string transport_id = RTCTransportStatsIDFromTransportChannel(
          transport_stats.second.transport_name, channel_stats.component);
      for (const cricket::ConnectionInfo& info :
           channel_stats.connection_infos) {
        std::unique_ptr<RTCIceCandidatePairStats> candidate_pair_stats(
            new RTCIceCandidatePairStats(
                RTCIceCandidatePairStatsIDFromConnectionInfo(info),
                timestamp_us));

        candidate_pair_stats->transport_id = transport_id;
        // TODO(hbos): There could be other candidates that are not paired with
        // anything. We don't have a complete list. Local candidates come from
        // Port objects, and prflx candidates (both local and remote) are only
        // stored in candidate pairs. crbug.com/632723
        candidate_pair_stats->local_candidate_id = ProduceIceCandidateStats(
            timestamp_us, info.local_candidate, true, transport_id, report);
        candidate_pair_stats->remote_candidate_id = ProduceIceCandidateStats(
            timestamp_us, info.remote_candidate, false, transport_id, report);
        candidate_pair_stats->state =
            IceCandidatePairStateToRTCStatsIceCandidatePairState(info.state);
        candidate_pair_stats->priority = info.priority;
        // TODO(hbos): This writable is different than the spec. It goes to
        // false after a certain amount of time without a response passes.
        // crbug.com/633550
        candidate_pair_stats->writable = info.writable;
        candidate_pair_stats->bytes_sent =
            static_cast<uint64_t>(info.sent_total_bytes);
        candidate_pair_stats->bytes_received =
            static_cast<uint64_t>(info.recv_total_bytes);
        // TODO(hbos): The |info.rtt| measurement is smoothed. It shouldn't be
        // smoothed according to the spec. crbug.com/633550. See
        // https://w3c.github.io/webrtc-stats/#dom-rtcicecandidatepairstats-currentrtt
        candidate_pair_stats->current_round_trip_time =
            static_cast<double>(info.rtt) / rtc::kNumMillisecsPerSec;
        candidate_pair_stats->requests_received =
            static_cast<uint64_t>(info.recv_ping_requests);
        candidate_pair_stats->requests_sent = static_cast<uint64_t>(
            info.sent_ping_requests_before_first_response);
        candidate_pair_stats->responses_received =
            static_cast<uint64_t>(info.recv_ping_responses);
        candidate_pair_stats->responses_sent =
            static_cast<uint64_t>(info.sent_ping_responses);
        RTC_DCHECK_GE(info.sent_ping_requests_total,
                      info.sent_ping_requests_before_first_response);
        candidate_pair_stats->consent_requests_sent = static_cast<uint64_t>(
            info.sent_ping_requests_total -
            info.sent_ping_requests_before_first_response);

        report->AddStats(std::move(candidate_pair_stats));
      }
    }
  }
}

void RTCStatsCollector::ProduceMediaStreamAndTrackStats_s(
    int64_t timestamp_us, RTCStatsReport* report) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  ProduceMediaStreamAndTrackStats(
      timestamp_us, pc_->local_streams(), true, report);
  ProduceMediaStreamAndTrackStats(
      timestamp_us, pc_->remote_streams(), false, report);
}

void RTCStatsCollector::ProducePeerConnectionStats_s(
    int64_t timestamp_us, RTCStatsReport* report) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  std::unique_ptr<RTCPeerConnectionStats> stats(
    new RTCPeerConnectionStats("RTCPeerConnection", timestamp_us));
  stats->data_channels_opened = internal_record_.data_channels_opened;
  stats->data_channels_closed = internal_record_.data_channels_closed;
  report->AddStats(std::move(stats));
}

void RTCStatsCollector::ProduceRTPStreamStats_n(
    int64_t timestamp_us, const SessionStats& session_stats,
    const MediaInfo& media_info, RTCStatsReport* report) const {
  RTC_DCHECK(network_thread_->IsCurrent());

  // Audio
  if (media_info.voice) {
    std::string transport_id = RTCTransportStatsIDFromBaseChannel(
        session_stats.proxy_to_transport, *pc_->session()->voice_channel());
    RTC_DCHECK(!transport_id.empty());
    // Inbound
    for (const cricket::VoiceReceiverInfo& voice_receiver_info :
         media_info.voice->receivers) {
      // TODO(nisse): SSRC == 0 currently means none. Delete check when that
      // is fixed.
      if (voice_receiver_info.ssrc() == 0)
        continue;
      std::unique_ptr<RTCInboundRTPStreamStats> inbound_audio(
          new RTCInboundRTPStreamStats(
              RTCInboundRTPStreamStatsIDFromSSRC(
                  true, voice_receiver_info.ssrc()),
              timestamp_us));
      SetInboundRTPStreamStatsFromVoiceReceiverInfo(
          voice_receiver_info, inbound_audio.get());
      inbound_audio->transport_id = transport_id;
      if (voice_receiver_info.codec_payload_type) {
        inbound_audio->codec_id =
            RTCCodecStatsIDFromDirectionMediaAndPayload(
                true, true, *voice_receiver_info.codec_payload_type);
      }
      report->AddStats(std::move(inbound_audio));
    }
    // Outbound
    for (const cricket::VoiceSenderInfo& voice_sender_info :
         media_info.voice->senders) {
      // TODO(nisse): SSRC == 0 currently means none. Delete check when that
      // is fixed.
      if (voice_sender_info.ssrc() == 0)
        continue;
      std::unique_ptr<RTCOutboundRTPStreamStats> outbound_audio(
          new RTCOutboundRTPStreamStats(
              RTCOutboundRTPStreamStatsIDFromSSRC(
                  true, voice_sender_info.ssrc()),
              timestamp_us));
      SetOutboundRTPStreamStatsFromVoiceSenderInfo(
          voice_sender_info, outbound_audio.get());
      outbound_audio->transport_id = transport_id;
      if (voice_sender_info.codec_payload_type) {
        outbound_audio->codec_id =
            RTCCodecStatsIDFromDirectionMediaAndPayload(
                false, true, *voice_sender_info.codec_payload_type);
      }
      report->AddStats(std::move(outbound_audio));
    }
  }
  // Video
  if (media_info.video) {
    std::string transport_id = RTCTransportStatsIDFromBaseChannel(
        session_stats.proxy_to_transport, *pc_->session()->video_channel());
    RTC_DCHECK(!transport_id.empty());
    // Inbound
    for (const cricket::VideoReceiverInfo& video_receiver_info :
         media_info.video->receivers) {
      // TODO(nisse): SSRC == 0 currently means none. Delete check when that
      // is fixed.
      if (video_receiver_info.ssrc() == 0)
        continue;
      std::unique_ptr<RTCInboundRTPStreamStats> inbound_video(
          new RTCInboundRTPStreamStats(
              RTCInboundRTPStreamStatsIDFromSSRC(
                  false, video_receiver_info.ssrc()),
              timestamp_us));
      SetInboundRTPStreamStatsFromVideoReceiverInfo(
          video_receiver_info, inbound_video.get());
      inbound_video->transport_id = transport_id;
      if (video_receiver_info.codec_payload_type) {
        inbound_video->codec_id =
            RTCCodecStatsIDFromDirectionMediaAndPayload(
                true, false, *video_receiver_info.codec_payload_type);
      }
      report->AddStats(std::move(inbound_video));
    }
    // Outbound
    for (const cricket::VideoSenderInfo& video_sender_info :
         media_info.video->senders) {
      // TODO(nisse): SSRC == 0 currently means none. Delete check when that
      // is fixed.
      if (video_sender_info.ssrc() == 0)
        continue;
      std::unique_ptr<RTCOutboundRTPStreamStats> outbound_video(
          new RTCOutboundRTPStreamStats(
              RTCOutboundRTPStreamStatsIDFromSSRC(
                  false, video_sender_info.ssrc()),
              timestamp_us));
      SetOutboundRTPStreamStatsFromVideoSenderInfo(
          video_sender_info, outbound_video.get());
      outbound_video->transport_id = transport_id;
      if (video_sender_info.codec_payload_type) {
        outbound_video->codec_id =
            RTCCodecStatsIDFromDirectionMediaAndPayload(
                false, false, *video_sender_info.codec_payload_type);
      }
      report->AddStats(std::move(outbound_video));
    }
  }
}

void RTCStatsCollector::ProduceTransportStats_n(
    int64_t timestamp_us, const SessionStats& session_stats,
    const std::map<std::string, CertificateStatsPair>& transport_cert_stats,
    RTCStatsReport* report) const {
  RTC_DCHECK(network_thread_->IsCurrent());
  for (const auto& transport : session_stats.transport_stats) {
    // Get reference to RTCP channel, if it exists.
    std::string rtcp_transport_stats_id;
    for (const auto& channel_stats : transport.second.channel_stats) {
      if (channel_stats.component ==
          cricket::ICE_CANDIDATE_COMPONENT_RTCP) {
        rtcp_transport_stats_id = RTCTransportStatsIDFromTransportChannel(
            transport.second.transport_name, channel_stats.component);
        break;
      }
    }

    // Get reference to local and remote certificates of this transport, if they
    // exist.
    const auto& certificate_stats_it = transport_cert_stats.find(
        transport.second.transport_name);
    RTC_DCHECK(certificate_stats_it != transport_cert_stats.cend());
    std::string local_certificate_id;
    if (certificate_stats_it->second.local) {
      local_certificate_id = RTCCertificateIDFromFingerprint(
          certificate_stats_it->second.local->fingerprint);
    }
    std::string remote_certificate_id;
    if (certificate_stats_it->second.remote) {
      remote_certificate_id = RTCCertificateIDFromFingerprint(
          certificate_stats_it->second.remote->fingerprint);
    }

    // There is one transport stats for each channel.
    for (const auto& channel_stats : transport.second.channel_stats) {
      std::unique_ptr<RTCTransportStats> transport_stats(
          new RTCTransportStats(
              RTCTransportStatsIDFromTransportChannel(
                  transport.second.transport_name, channel_stats.component),
              timestamp_us));
      transport_stats->bytes_sent = 0;
      transport_stats->bytes_received = 0;
      transport_stats->active_connection = false;
      for (const cricket::ConnectionInfo& info :
           channel_stats.connection_infos) {
        *transport_stats->bytes_sent += info.sent_total_bytes;
        *transport_stats->bytes_received += info.recv_total_bytes;
        if (info.best_connection) {
          transport_stats->active_connection = true;
          transport_stats->selected_candidate_pair_id =
              RTCIceCandidatePairStatsIDFromConnectionInfo(info);
        }
      }
      if (channel_stats.component != cricket::ICE_CANDIDATE_COMPONENT_RTCP &&
          !rtcp_transport_stats_id.empty()) {
        transport_stats->rtcp_transport_stats_id = rtcp_transport_stats_id;
      }
      if (!local_certificate_id.empty())
        transport_stats->local_certificate_id = local_certificate_id;
      if (!remote_certificate_id.empty())
        transport_stats->remote_certificate_id = remote_certificate_id;
      report->AddStats(std::move(transport_stats));
    }
  }
}

std::map<std::string, RTCStatsCollector::CertificateStatsPair>
RTCStatsCollector::PrepareTransportCertificateStats_n(
    const SessionStats& session_stats) const {
  RTC_DCHECK(network_thread_->IsCurrent());
  std::map<std::string, CertificateStatsPair> transport_cert_stats;
  for (const auto& transport_stats : session_stats.transport_stats) {
    CertificateStatsPair certificate_stats_pair;
    rtc::scoped_refptr<rtc::RTCCertificate> local_certificate;
    if (pc_->session()->GetLocalCertificate(
        transport_stats.second.transport_name, &local_certificate)) {
      certificate_stats_pair.local =
          local_certificate->ssl_certificate().GetStats();
    }
    std::unique_ptr<rtc::SSLCertificate> remote_certificate =
        pc_->session()->GetRemoteSSLCertificate(
            transport_stats.second.transport_name);
    if (remote_certificate) {
      certificate_stats_pair.remote = remote_certificate->GetStats();
    }
    transport_cert_stats.insert(
        std::make_pair(transport_stats.second.transport_name,
                       std::move(certificate_stats_pair)));
  }
  return transport_cert_stats;
}

std::unique_ptr<RTCStatsCollector::MediaInfo>
RTCStatsCollector::PrepareMediaInfo_s() const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  std::unique_ptr<MediaInfo> media_info(new MediaInfo());
  if (pc_->session()->voice_channel()) {
    cricket::VoiceMediaInfo voice_media_info;
    if (pc_->session()->voice_channel()->GetStats(&voice_media_info)) {
      media_info->voice = rtc::Optional<cricket::VoiceMediaInfo>(
          std::move(voice_media_info));
    }
  }
  if (pc_->session()->video_channel()) {
    cricket::VideoMediaInfo video_media_info;
    if (pc_->session()->video_channel()->GetStats(&video_media_info)) {
      media_info->video = rtc::Optional<cricket::VideoMediaInfo>(
          std::move(video_media_info));
    }
  }
  return media_info;
}

void RTCStatsCollector::OnDataChannelCreated(DataChannel* channel) {
  channel->SignalOpened.connect(this, &RTCStatsCollector::OnDataChannelOpened);
  channel->SignalClosed.connect(this, &RTCStatsCollector::OnDataChannelClosed);
}

void RTCStatsCollector::OnDataChannelOpened(DataChannel* channel) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  bool result = internal_record_.opened_data_channels.insert(
      reinterpret_cast<uintptr_t>(channel)).second;
  ++internal_record_.data_channels_opened;
  RTC_DCHECK(result);
}

void RTCStatsCollector::OnDataChannelClosed(DataChannel* channel) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  // Only channels that have been fully opened (and have increased the
  // |data_channels_opened_| counter) increase the closed counter.
  if (internal_record_.opened_data_channels.find(
          reinterpret_cast<uintptr_t>(channel)) !=
      internal_record_.opened_data_channels.end()) {
    ++internal_record_.data_channels_closed;
  }
}

const char* CandidateTypeToRTCIceCandidateTypeForTesting(
    const std::string& type) {
  return CandidateTypeToRTCIceCandidateType(type);
}

const char* DataStateToRTCDataChannelStateForTesting(
    DataChannelInterface::DataState state) {
  return DataStateToRTCDataChannelState(state);
}

}  // namespace webrtc
