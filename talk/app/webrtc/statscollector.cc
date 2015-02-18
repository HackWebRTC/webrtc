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

#include "talk/app/webrtc/statscollector.h"

#include <utility>
#include <vector>

#include "talk/session/media/channel.h"
#include "webrtc/base/base64.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/timing.h"

using rtc::scoped_ptr;

namespace webrtc {
namespace {

// The following is the enum RTCStatsIceCandidateType from
// http://w3c.github.io/webrtc-stats/#rtcstatsicecandidatetype-enum such that
// our stats report for ice candidate type could conform to that.
const char STATSREPORT_LOCAL_PORT_TYPE[] = "host";
const char STATSREPORT_STUN_PORT_TYPE[] = "serverreflexive";
const char STATSREPORT_PRFLX_PORT_TYPE[] = "peerreflexive";
const char STATSREPORT_RELAY_PORT_TYPE[] = "relayed";

// Strings used by the stats collector to report adapter types. This fits the
// general stype of http://w3c.github.io/webrtc-stats than what
// AdapterTypeToString does.
const char* STATSREPORT_ADAPTER_TYPE_ETHERNET = "lan";
const char* STATSREPORT_ADAPTER_TYPE_WIFI = "wlan";
const char* STATSREPORT_ADAPTER_TYPE_WWAN = "wwan";
const char* STATSREPORT_ADAPTER_TYPE_VPN = "vpn";
const char* STATSREPORT_ADAPTER_TYPE_LOOPBACK = "loopback";

bool GetTransportIdFromProxy(const cricket::ProxyTransportMap& map,
                             const std::string& proxy,
                             std::string* transport) {
  // TODO(hta): Remove handling of empty proxy name once tests do not use it.
  if (proxy.empty()) {
    transport->clear();
    return true;
  }

  cricket::ProxyTransportMap::const_iterator found = map.find(proxy);
  if (found == map.end()) {
    LOG(LS_ERROR) << "No transport ID mapping for " << proxy;
    return false;
  }

  // Component 1 is always used for RTP.
  scoped_ptr<StatsReport::Id> id(
      StatsReport::NewComponentId(found->second, 1));
  // TODO(tommi): Should |transport| simply be of type StatsReport::Id?
  // When we support more value types than string (e.g. int, double, vector etc)
  // we should also support a value type for Id.
  *transport = id->ToString();
  return true;
}

void AddTrackReport(StatsCollection* reports, const std::string& track_id) {
  // Adds an empty track report.
  rtc::scoped_ptr<StatsReport::Id> id(
      StatsReport::NewTypedId(StatsReport::kStatsReportTypeTrack, track_id));
  StatsReport* report = reports->ReplaceOrAddNew(id.Pass());
  report->AddValue(StatsReport::kStatsValueNameTrackId, track_id);
}

template <class TrackVector>
void CreateTrackReports(const TrackVector& tracks, StatsCollection* reports) {
  for (size_t j = 0; j < tracks.size(); ++j) {
    webrtc::MediaStreamTrackInterface* track = tracks[j];
    AddTrackReport(reports, track->id());
  }
}

void ExtractStats(const cricket::VoiceReceiverInfo& info, StatsReport* report) {
  report->AddValue(StatsReport::kStatsValueNameAudioOutputLevel,
                   info.audio_level);
  report->AddValue(StatsReport::kStatsValueNameBytesReceived,
                   info.bytes_rcvd);
  report->AddValue(StatsReport::kStatsValueNameJitterReceived,
                   info.jitter_ms);
  report->AddValue(StatsReport::kStatsValueNameJitterBufferMs,
                   info.jitter_buffer_ms);
  report->AddValue(StatsReport::kStatsValueNamePreferredJitterBufferMs,
                   info.jitter_buffer_preferred_ms);
  report->AddValue(StatsReport::kStatsValueNameCurrentDelayMs,
                   info.delay_estimate_ms);
  report->AddValue(StatsReport::kStatsValueNameExpandRate,
                   rtc::ToString<float>(info.expand_rate));
  report->AddValue(StatsReport::kStatsValueNameSpeechExpandRate,
                   rtc::ToString<float>(info.speech_expand_rate));
  report->AddValue(StatsReport::kStatsValueNameSecondaryDecodedRate,
                   rtc::ToString<float>(info.secondary_decoded_rate));
  report->AddValue(StatsReport::kStatsValueNamePacketsReceived,
                   info.packets_rcvd);
  report->AddValue(StatsReport::kStatsValueNamePacketsLost,
                   info.packets_lost);
  report->AddValue(StatsReport::kStatsValueNameDecodingCTSG,
                   info.decoding_calls_to_silence_generator);
  report->AddValue(StatsReport::kStatsValueNameDecodingCTN,
                   info.decoding_calls_to_neteq);
  report->AddValue(StatsReport::kStatsValueNameDecodingNormal,
                   info.decoding_normal);
  report->AddValue(StatsReport::kStatsValueNameDecodingPLC,
                   info.decoding_plc);
  report->AddValue(StatsReport::kStatsValueNameDecodingCNG,
                   info.decoding_cng);
  report->AddValue(StatsReport::kStatsValueNameDecodingPLCCNG,
                   info.decoding_plc_cng);
  report->AddValue(StatsReport::kStatsValueNameCaptureStartNtpTimeMs,
                   info.capture_start_ntp_time_ms);
  report->AddValue(StatsReport::kStatsValueNameCodecName, info.codec_name);
}

void ExtractStats(const cricket::VoiceSenderInfo& info, StatsReport* report) {
  report->AddValue(StatsReport::kStatsValueNameAudioInputLevel,
                   info.audio_level);
  report->AddValue(StatsReport::kStatsValueNameBytesSent,
                   info.bytes_sent);
  report->AddValue(StatsReport::kStatsValueNamePacketsSent,
                   info.packets_sent);
  report->AddValue(StatsReport::kStatsValueNamePacketsLost,
                   info.packets_lost);
  report->AddValue(StatsReport::kStatsValueNameJitterReceived,
                   info.jitter_ms);
  report->AddValue(StatsReport::kStatsValueNameRtt, info.rtt_ms);
  report->AddValue(StatsReport::kStatsValueNameEchoCancellationQualityMin,
                   rtc::ToString<float>(info.aec_quality_min));
  report->AddValue(StatsReport::kStatsValueNameEchoDelayMedian,
                   info.echo_delay_median_ms);
  report->AddValue(StatsReport::kStatsValueNameEchoDelayStdDev,
                   info.echo_delay_std_ms);
  report->AddValue(StatsReport::kStatsValueNameEchoReturnLoss,
                   info.echo_return_loss);
  report->AddValue(StatsReport::kStatsValueNameEchoReturnLossEnhancement,
                   info.echo_return_loss_enhancement);
  report->AddValue(StatsReport::kStatsValueNameCodecName, info.codec_name);
  report->AddBoolean(StatsReport::kStatsValueNameTypingNoiseState,
                     info.typing_noise_detected);
}

void ExtractStats(const cricket::VideoReceiverInfo& info, StatsReport* report) {
  report->AddValue(StatsReport::kStatsValueNameBytesReceived,
                   info.bytes_rcvd);
  report->AddValue(StatsReport::kStatsValueNamePacketsReceived,
                   info.packets_rcvd);
  report->AddValue(StatsReport::kStatsValueNamePacketsLost,
                   info.packets_lost);

  report->AddValue(StatsReport::kStatsValueNameFirsSent,
                   info.firs_sent);
  report->AddValue(StatsReport::kStatsValueNamePlisSent,
                   info.plis_sent);
  report->AddValue(StatsReport::kStatsValueNameNacksSent,
                   info.nacks_sent);
  report->AddValue(StatsReport::kStatsValueNameFrameWidthReceived,
                   info.frame_width);
  report->AddValue(StatsReport::kStatsValueNameFrameHeightReceived,
                   info.frame_height);
  report->AddValue(StatsReport::kStatsValueNameFrameRateReceived,
                   info.framerate_rcvd);
  report->AddValue(StatsReport::kStatsValueNameFrameRateDecoded,
                   info.framerate_decoded);
  report->AddValue(StatsReport::kStatsValueNameFrameRateOutput,
                   info.framerate_output);

  report->AddValue(StatsReport::kStatsValueNameDecodeMs,
                   info.decode_ms);
  report->AddValue(StatsReport::kStatsValueNameMaxDecodeMs,
                   info.max_decode_ms);
  report->AddValue(StatsReport::kStatsValueNameCurrentDelayMs,
                   info.current_delay_ms);
  report->AddValue(StatsReport::kStatsValueNameTargetDelayMs,
                   info.target_delay_ms);
  report->AddValue(StatsReport::kStatsValueNameJitterBufferMs,
                   info.jitter_buffer_ms);
  report->AddValue(StatsReport::kStatsValueNameMinPlayoutDelayMs,
                   info.min_playout_delay_ms);
  report->AddValue(StatsReport::kStatsValueNameRenderDelayMs,
                   info.render_delay_ms);

  report->AddValue(StatsReport::kStatsValueNameCaptureStartNtpTimeMs,
                   info.capture_start_ntp_time_ms);
}

void ExtractStats(const cricket::VideoSenderInfo& info, StatsReport* report) {
  report->AddValue(StatsReport::kStatsValueNameBytesSent,
                   info.bytes_sent);
  report->AddValue(StatsReport::kStatsValueNamePacketsSent,
                   info.packets_sent);
  report->AddValue(StatsReport::kStatsValueNamePacketsLost,
                   info.packets_lost);

  report->AddValue(StatsReport::kStatsValueNameFirsReceived,
                   info.firs_rcvd);
  report->AddValue(StatsReport::kStatsValueNamePlisReceived,
                   info.plis_rcvd);
  report->AddValue(StatsReport::kStatsValueNameNacksReceived,
                   info.nacks_rcvd);
  report->AddValue(StatsReport::kStatsValueNameFrameWidthInput,
                   info.input_frame_width);
  report->AddValue(StatsReport::kStatsValueNameFrameHeightInput,
                   info.input_frame_height);
  report->AddValue(StatsReport::kStatsValueNameFrameWidthSent,
                   info.send_frame_width);
  report->AddValue(StatsReport::kStatsValueNameFrameHeightSent,
                   info.send_frame_height);
  report->AddValue(StatsReport::kStatsValueNameFrameRateInput,
                   info.framerate_input);
  report->AddValue(StatsReport::kStatsValueNameFrameRateSent,
                   info.framerate_sent);
  report->AddValue(StatsReport::kStatsValueNameRtt, info.rtt_ms);
  report->AddValue(StatsReport::kStatsValueNameCodecName, info.codec_name);
  report->AddBoolean(StatsReport::kStatsValueNameCpuLimitedResolution,
                     (info.adapt_reason & 0x1) > 0);
  report->AddBoolean(StatsReport::kStatsValueNameBandwidthLimitedResolution,
                     (info.adapt_reason & 0x2) > 0);
  report->AddBoolean(StatsReport::kStatsValueNameViewLimitedResolution,
                     (info.adapt_reason & 0x4) > 0);
  report->AddValue(StatsReport::kStatsValueNameAdaptationChanges,
                   info.adapt_changes);
  report->AddValue(StatsReport::kStatsValueNameAvgEncodeMs, info.avg_encode_ms);
  report->AddValue(StatsReport::kStatsValueNameCaptureJitterMs,
                   info.capture_jitter_ms);
  report->AddValue(StatsReport::kStatsValueNameCaptureQueueDelayMsPerS,
                   info.capture_queue_delay_ms_per_s);
  report->AddValue(StatsReport::kStatsValueNameEncodeUsagePercent,
                   info.encode_usage_percent);
}

void ExtractStats(const cricket::BandwidthEstimationInfo& info,
                  double stats_gathering_started,
                  PeerConnectionInterface::StatsOutputLevel level,
                  StatsReport* report) {
  ASSERT(report->type() == StatsReport::kStatsReportTypeBwe);

  // Clear out stats from previous GatherStats calls if any.
  if (report->timestamp() != stats_gathering_started) {
    report->ResetValues();
    report->set_timestamp(stats_gathering_started);
  }

  report->AddValue(StatsReport::kStatsValueNameAvailableSendBandwidth,
                   info.available_send_bandwidth);
  report->AddValue(StatsReport::kStatsValueNameAvailableReceiveBandwidth,
                   info.available_recv_bandwidth);
  report->AddValue(StatsReport::kStatsValueNameTargetEncBitrate,
                   info.target_enc_bitrate);
  report->AddValue(StatsReport::kStatsValueNameActualEncBitrate,
                   info.actual_enc_bitrate);
  report->AddValue(StatsReport::kStatsValueNameRetransmitBitrate,
                   info.retransmit_bitrate);
  report->AddValue(StatsReport::kStatsValueNameTransmitBitrate,
                   info.transmit_bitrate);
  report->AddValue(StatsReport::kStatsValueNameBucketDelay,
                   info.bucket_delay);
  if (level >= PeerConnectionInterface::kStatsOutputLevelDebug) {
    report->AddValue(
        StatsReport::kStatsValueNameRecvPacketGroupPropagationDeltaSumDebug,
        info.total_received_propagation_delta_ms);
    if (info.recent_received_propagation_delta_ms.size() > 0) {
      report->AddValue(
          StatsReport::kStatsValueNameRecvPacketGroupPropagationDeltaDebug,
          info.recent_received_propagation_delta_ms);
      report->AddValue(
          StatsReport::kStatsValueNameRecvPacketGroupArrivalTimeDebug,
          info.recent_received_packet_group_arrival_time_ms);
    }
  }
}

void ExtractRemoteStats(const cricket::MediaSenderInfo& info,
                        StatsReport* report) {
  report->set_timestamp(info.remote_stats[0].timestamp);
  // TODO(hta): Extract some stats here.
}

void ExtractRemoteStats(const cricket::MediaReceiverInfo& info,
                        StatsReport* report) {
  report->set_timestamp(info.remote_stats[0].timestamp);
  // TODO(hta): Extract some stats here.
}

// Template to extract stats from a data vector.
// In order to use the template, the functions that are called from it,
// ExtractStats and ExtractRemoteStats, must be defined and overloaded
// for each type.
template<typename T>
void ExtractStatsFromList(const std::vector<T>& data,
                          const std::string& transport_id,
                          StatsCollector* collector,
                          StatsReport::Direction direction) {
  for (const auto& d : data) {
    uint32 ssrc = d.ssrc();
    // Each track can have stats for both local and remote objects.
    // TODO(hta): Handle the case of multiple SSRCs per object.
    StatsReport* report = collector->PrepareReport(true, ssrc, transport_id,
                                                   direction);
    if (report)
      ExtractStats(d, report);

    if (!d.remote_stats.empty()) {
      report = collector->PrepareReport(false, ssrc, transport_id, direction);
      if (report)
        ExtractRemoteStats(d, report);
    }
  }
}

}  // namespace

const char* IceCandidateTypeToStatsType(const std::string& candidate_type) {
  if (candidate_type == cricket::LOCAL_PORT_TYPE) {
    return STATSREPORT_LOCAL_PORT_TYPE;
  }
  if (candidate_type == cricket::STUN_PORT_TYPE) {
    return STATSREPORT_STUN_PORT_TYPE;
  }
  if (candidate_type == cricket::PRFLX_PORT_TYPE) {
    return STATSREPORT_PRFLX_PORT_TYPE;
  }
  if (candidate_type == cricket::RELAY_PORT_TYPE) {
    return STATSREPORT_RELAY_PORT_TYPE;
  }
  ASSERT(false);
  return "unknown";
}

const char* AdapterTypeToStatsType(rtc::AdapterType type) {
  switch (type) {
    case rtc::ADAPTER_TYPE_UNKNOWN:
      return "unknown";
    case rtc::ADAPTER_TYPE_ETHERNET:
      return STATSREPORT_ADAPTER_TYPE_ETHERNET;
    case rtc::ADAPTER_TYPE_WIFI:
      return STATSREPORT_ADAPTER_TYPE_WIFI;
    case rtc::ADAPTER_TYPE_CELLULAR:
      return STATSREPORT_ADAPTER_TYPE_WWAN;
    case rtc::ADAPTER_TYPE_VPN:
      return STATSREPORT_ADAPTER_TYPE_VPN;
    case rtc::ADAPTER_TYPE_LOOPBACK:
      return STATSREPORT_ADAPTER_TYPE_LOOPBACK;
    default:
      ASSERT(false);
      return "";
  }
}

StatsCollector::StatsCollector(WebRtcSession* session)
    : session_(session),
      stats_gathering_started_(0) {
  ASSERT(session_);
}

StatsCollector::~StatsCollector() {
  ASSERT(session_->signaling_thread()->IsCurrent());
}

double StatsCollector::GetTimeNow() {
  return rtc::Timing::WallTimeNow() * rtc::kNumMillisecsPerSec;
}

// Adds a MediaStream with tracks that can be used as a |selector| in a call
// to GetStats.
void StatsCollector::AddStream(MediaStreamInterface* stream) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  ASSERT(stream != NULL);

  CreateTrackReports<AudioTrackVector>(stream->GetAudioTracks(),
                                       &reports_);
  CreateTrackReports<VideoTrackVector>(stream->GetVideoTracks(),
                                       &reports_);
}

void StatsCollector::AddLocalAudioTrack(AudioTrackInterface* audio_track,
                                        uint32 ssrc) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  ASSERT(audio_track != NULL);
  for (LocalAudioTrackVector::iterator it = local_audio_tracks_.begin();
       it != local_audio_tracks_.end(); ++it) {
    ASSERT(it->first != audio_track || it->second != ssrc);
  }

  local_audio_tracks_.push_back(std::make_pair(audio_track, ssrc));

  // Create the kStatsReportTypeTrack report for the new track if there is no
  // report yet.
  rtc::scoped_ptr<StatsReport::Id> id(
      StatsReport::NewTypedId(StatsReport::kStatsReportTypeTrack,
                              audio_track->id()));
  StatsReport* report = reports_.Find(*id.get());
  if (!report) {
    report = reports_.InsertNew(id.Pass());
    report->AddValue(StatsReport::kStatsValueNameTrackId, audio_track->id());
  }
}

void StatsCollector::RemoveLocalAudioTrack(AudioTrackInterface* audio_track,
                                           uint32 ssrc) {
  ASSERT(audio_track != NULL);
  for (LocalAudioTrackVector::iterator it = local_audio_tracks_.begin();
       it != local_audio_tracks_.end(); ++it) {
    if (it->first == audio_track && it->second == ssrc) {
      local_audio_tracks_.erase(it);
      return;
    }
  }

  ASSERT(false);
}

void StatsCollector::GetStats(MediaStreamTrackInterface* track,
                              StatsReports* reports) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  ASSERT(reports != NULL);
  ASSERT(reports->empty());

  if (!track) {
    reports->reserve(reports_.size());
    for (auto* r : reports_)
      reports->push_back(r);
    return;
  }

  StatsReport* report = reports_.Find(StatsReport::NewTypedId(
      StatsReport::kStatsReportTypeSession, session_->id()));
  if (report)
    reports->push_back(report);

  report = reports_.Find(StatsReport::NewTypedId(
      StatsReport::kStatsReportTypeTrack, track->id()));

  if (!report)
    return;

  reports->push_back(report);

  std::string track_id;
  for (const auto* r : reports_) {
    if (r->type() != StatsReport::kStatsReportTypeSsrc)
      continue;

    const StatsReport::Value* v =
        r->FindValue(StatsReport::kStatsValueNameTrackId);
    if (v && v->value == track->id())
      reports->push_back(r);
  }
}

void
StatsCollector::UpdateStats(PeerConnectionInterface::StatsOutputLevel level) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  double time_now = GetTimeNow();
  // Calls to UpdateStats() that occur less than kMinGatherStatsPeriod number of
  // ms apart will be ignored.
  const double kMinGatherStatsPeriod = 50;
  if (stats_gathering_started_ != 0 &&
      stats_gathering_started_ + kMinGatherStatsPeriod > time_now) {
    return;
  }
  stats_gathering_started_ = time_now;

  if (session_) {
    ExtractSessionInfo();
    ExtractVoiceInfo();
    ExtractVideoInfo(level);
    ExtractDataInfo();
  }
}

StatsReport* StatsCollector::PrepareReport(
    bool local,
    uint32 ssrc,
    const std::string& transport_id,
    StatsReport::Direction direction) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  const std::string ssrc_id = rtc::ToString<uint32>(ssrc);
  rtc::scoped_ptr<StatsReport::Id> id(StatsReport::NewIdWithDirection(
      local ? StatsReport::kStatsReportTypeSsrc :
              StatsReport::kStatsReportTypeRemoteSsrc,
      ssrc_id, direction));
  StatsReport* report = reports_.Find(*id.get());

  // Use the ID of the track that is currently mapped to the SSRC, if any.
  std::string track_id;
  if (!GetTrackIdBySsrc(ssrc, &track_id, direction)) {
    if (!report) {
      // The ssrc is not used by any track or existing report, return NULL
      // in such case to indicate no report is prepared for the ssrc.
      return NULL;
    }

    // The ssrc is not used by any existing track. Keeps the old track id
    // since we want to report the stats for inactive ssrc.
    const StatsReport::Value* v =
        report->FindValue(StatsReport::kStatsValueNameTrackId);
    if (v)
      track_id = v->value;
  }

  if (!report) {
    report = reports_.InsertNew(id.Pass());
  } else {
    // Clear out stats from previous GatherStats calls if any.
    // This is required since the report will be returned for the new values.
    // Having the old values in the report will lead to multiple values with
    // the same name.
    // TODO(tommi): This seems to be pretty wasteful if some of these values
    // have not changed (we basically throw them away just to recreate them).
    // Figure out a way to not have to do this while not breaking the existing
    // functionality.
    report->ResetValues();
  }

  ASSERT(report->values().empty());
  // FYI - for remote reports, the timestamp will be overwritten later.
  report->set_timestamp(stats_gathering_started_);

  report->AddValue(StatsReport::kStatsValueNameSsrc, ssrc_id);
  report->AddValue(StatsReport::kStatsValueNameTrackId, track_id);
  // Add the mapping of SSRC to transport.
  report->AddValue(StatsReport::kStatsValueNameTransportId,
                   transport_id);
  return report;
}

std::string StatsCollector::AddOneCertificateReport(
    const rtc::SSLCertificate* cert, const std::string& issuer_id) {
  ASSERT(session_->signaling_thread()->IsCurrent());

  // TODO(bemasc): Move this computation to a helper class that caches these
  // values to reduce CPU use in GetStats.  This will require adding a fast
  // SSLCertificate::Equals() method to detect certificate changes.

  std::string digest_algorithm;
  if (!cert->GetSignatureDigestAlgorithm(&digest_algorithm))
    return std::string();

  rtc::scoped_ptr<rtc::SSLFingerprint> ssl_fingerprint(
      rtc::SSLFingerprint::Create(digest_algorithm, cert));

  // SSLFingerprint::Create can fail if the algorithm returned by
  // SSLCertificate::GetSignatureDigestAlgorithm is not supported by the
  // implementation of SSLCertificate::ComputeDigest.  This currently happens
  // with MD5- and SHA-224-signed certificates when linked to libNSS.
  if (!ssl_fingerprint)
    return std::string();

  std::string fingerprint = ssl_fingerprint->GetRfc4572Fingerprint();

  rtc::Buffer der_buffer;
  cert->ToDER(&der_buffer);
  std::string der_base64;
  rtc::Base64::EncodeFromArray(
      der_buffer.data(), der_buffer.length(), &der_base64);

  rtc::scoped_ptr<StatsReport::Id> id(
      StatsReport::NewTypedId(
          StatsReport::kStatsReportTypeCertificate, fingerprint));
  StatsReport* report = reports_.ReplaceOrAddNew(id.Pass());
  report->set_timestamp(stats_gathering_started_);
  report->AddValue(StatsReport::kStatsValueNameFingerprint, fingerprint);
  report->AddValue(StatsReport::kStatsValueNameFingerprintAlgorithm,
                   digest_algorithm);
  report->AddValue(StatsReport::kStatsValueNameDer, der_base64);
  if (!issuer_id.empty())
    report->AddValue(StatsReport::kStatsValueNameIssuerId, issuer_id);
  // TODO(tommi): Can we avoid this?
  return report->id().ToString();
}

std::string StatsCollector::AddCertificateReports(
    const rtc::SSLCertificate* cert) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  // Produces a chain of StatsReports representing this certificate and the rest
  // of its chain, and adds those reports to |reports_|.  The return value is
  // the id of the leaf report.  The provided cert must be non-null, so at least
  // one report will always be provided and the returned string will never be
  // empty.
  ASSERT(cert != NULL);

  std::string issuer_id;
  rtc::scoped_ptr<rtc::SSLCertChain> chain;
  if (cert->GetChain(chain.accept())) {
    // This loop runs in reverse, i.e. from root to leaf, so that each
    // certificate's issuer's report ID is known before the child certificate's
    // report is generated.  The root certificate does not have an issuer ID
    // value.
    for (ptrdiff_t i = chain->GetSize() - 1; i >= 0; --i) {
      const rtc::SSLCertificate& cert_i = chain->Get(i);
      issuer_id = AddOneCertificateReport(&cert_i, issuer_id);
    }
  }
  // Add the leaf certificate.
  return AddOneCertificateReport(cert, issuer_id);
}

std::string StatsCollector::AddCandidateReport(
    const cricket::Candidate& candidate,
    bool local) {
  scoped_ptr<StatsReport::Id> id(
      StatsReport::NewCandidateId(local, candidate.id()));
  StatsReport* report = reports_.Find(*id.get());
  if (!report) {
    report = reports_.InsertNew(id.Pass());
    report->set_timestamp(stats_gathering_started_);
    if (local) {
      report->AddValue(StatsReport::kStatsValueNameCandidateNetworkType,
                       AdapterTypeToStatsType(candidate.network_type()));
    }
    report->AddValue(StatsReport::kStatsValueNameCandidateIPAddress,
                     candidate.address().ipaddr().ToString());
    report->AddValue(StatsReport::kStatsValueNameCandidatePortNumber,
                     candidate.address().PortAsString());
    report->AddValue(StatsReport::kStatsValueNameCandidatePriority,
                     candidate.priority());
    report->AddValue(StatsReport::kStatsValueNameCandidateType,
                     IceCandidateTypeToStatsType(candidate.type()));
    report->AddValue(StatsReport::kStatsValueNameCandidateTransportType,
                     candidate.protocol());
  }

  // TODO(tommi): Necessary?
  return report->id().ToString();
}

void StatsCollector::ExtractSessionInfo() {
  ASSERT(session_->signaling_thread()->IsCurrent());
  // Extract information from the base session.
  rtc::scoped_ptr<StatsReport::Id> id(
      StatsReport::NewTypedId(
          StatsReport::kStatsReportTypeSession, session_->id()));
  StatsReport* report = reports_.ReplaceOrAddNew(id.Pass());
  report->set_timestamp(stats_gathering_started_);
  report->ResetValues();
  report->AddBoolean(StatsReport::kStatsValueNameInitiator,
                     session_->initiator());

  cricket::SessionStats stats;
  if (session_->GetStats(&stats)) {
    // Store the proxy map away for use in SSRC reporting.
    proxy_to_transport_ = stats.proxy_to_transport;

    for (cricket::TransportStatsMap::iterator transport_iter
             = stats.transport_stats.begin();
         transport_iter != stats.transport_stats.end(); ++transport_iter) {
      // Attempt to get a copy of the certificates from the transport and
      // expose them in stats reports.  All channels in a transport share the
      // same local and remote certificates.
      //
      // Note that Transport::GetIdentity and Transport::GetRemoteCertificate
      // invoke method calls on the worker thread and block this thread, but
      // messages are still processed on this thread, which may blow way the
      // existing transports. So we cannot reuse |transport| after these calls.
      std::string local_cert_report_id, remote_cert_report_id;

      cricket::Transport* transport =
          session_->GetTransport(transport_iter->second.content_name);
      rtc::scoped_ptr<rtc::SSLIdentity> identity;
      if (transport && transport->GetIdentity(identity.accept())) {
        local_cert_report_id =
            AddCertificateReports(&(identity->certificate()));
      }

      transport = session_->GetTransport(transport_iter->second.content_name);
      rtc::scoped_ptr<rtc::SSLCertificate> cert;
      if (transport && transport->GetRemoteCertificate(cert.accept())) {
        remote_cert_report_id = AddCertificateReports(cert.get());
      }

      for (cricket::TransportChannelStatsList::iterator channel_iter
               = transport_iter->second.channel_stats.begin();
           channel_iter != transport_iter->second.channel_stats.end();
           ++channel_iter) {
        rtc::scoped_ptr<StatsReport::Id> id(
            StatsReport::NewComponentId(transport_iter->second.content_name,
                channel_iter->component));
        StatsReport* channel_report = reports_.ReplaceOrAddNew(id.Pass());
        channel_report->set_timestamp(stats_gathering_started_);
        channel_report->AddValue(StatsReport::kStatsValueNameComponent,
                                 channel_iter->component);
        if (!local_cert_report_id.empty()) {
          channel_report->AddValue(
              StatsReport::kStatsValueNameLocalCertificateId,
              local_cert_report_id);
        }
        if (!remote_cert_report_id.empty()) {
          channel_report->AddValue(
              StatsReport::kStatsValueNameRemoteCertificateId,
              remote_cert_report_id);
        }
        for (size_t i = 0;
             i < channel_iter->connection_infos.size();
             ++i) {
          rtc::scoped_ptr<StatsReport::Id> id(
              StatsReport::NewCandidatePairId(transport_iter->first,
                  channel_iter->component, static_cast<int>(i)));
          StatsReport* report = reports_.ReplaceOrAddNew(id.Pass());
          report->set_timestamp(stats_gathering_started_);
          // Link from connection to its containing channel.
          // TODO(tommi): Any way to avoid ToString here?
          report->AddValue(StatsReport::kStatsValueNameChannelId,
                           channel_report->id().ToString());

          const cricket::ConnectionInfo& info =
              channel_iter->connection_infos[i];
          report->AddValue(StatsReport::kStatsValueNameBytesSent,
                           info.sent_total_bytes);
          report->AddValue(StatsReport::kStatsValueNameSendPacketsDiscarded,
                           info.sent_discarded_packets);
          report->AddValue(StatsReport::kStatsValueNamePacketsSent,
                           info.sent_total_packets);
          report->AddValue(StatsReport::kStatsValueNameBytesReceived,
                           info.recv_total_bytes);
          report->AddBoolean(StatsReport::kStatsValueNameWritable,
                             info.writable);
          report->AddBoolean(StatsReport::kStatsValueNameReadable,
                             info.readable);
          report->AddBoolean(StatsReport::kStatsValueNameActiveConnection,
                             info.best_connection);
          report->AddValue(StatsReport::kStatsValueNameLocalCandidateId,
                           AddCandidateReport(info.local_candidate, true));
          report->AddValue(
              StatsReport::kStatsValueNameRemoteCandidateId,
              AddCandidateReport(info.remote_candidate, false));
          report->AddValue(StatsReport::kStatsValueNameLocalAddress,
                           info.local_candidate.address().ToString());
          report->AddValue(StatsReport::kStatsValueNameRemoteAddress,
                           info.remote_candidate.address().ToString());
          report->AddValue(StatsReport::kStatsValueNameRtt, info.rtt);
          report->AddValue(StatsReport::kStatsValueNameTransportType,
                           info.local_candidate.protocol());
          report->AddValue(StatsReport::kStatsValueNameLocalCandidateType,
                           info.local_candidate.type());
          report->AddValue(StatsReport::kStatsValueNameRemoteCandidateType,
                           info.remote_candidate.type());
        }
      }
    }
  }
}

void StatsCollector::ExtractVoiceInfo() {
  ASSERT(session_->signaling_thread()->IsCurrent());

  if (!session_->voice_channel()) {
    return;
  }
  cricket::VoiceMediaInfo voice_info;
  if (!session_->voice_channel()->GetStats(&voice_info)) {
    LOG(LS_ERROR) << "Failed to get voice channel stats.";
    return;
  }
  std::string transport_id;
  if (!GetTransportIdFromProxy(proxy_to_transport_,
                               session_->voice_channel()->content_name(),
                               &transport_id)) {
    LOG(LS_ERROR) << "Failed to get transport name for proxy "
                  << session_->voice_channel()->content_name();
    return;
  }
  ExtractStatsFromList(voice_info.receivers, transport_id, this,
      StatsReport::kReceive);
  ExtractStatsFromList(voice_info.senders, transport_id, this,
      StatsReport::kSend);

  UpdateStatsFromExistingLocalAudioTracks();
}

void StatsCollector::ExtractVideoInfo(
    PeerConnectionInterface::StatsOutputLevel level) {
  ASSERT(session_->signaling_thread()->IsCurrent());

  if (!session_->video_channel())
    return;

  cricket::StatsOptions options;
  options.include_received_propagation_stats =
      (level >= PeerConnectionInterface::kStatsOutputLevelDebug) ?
          true : false;
  cricket::VideoMediaInfo video_info;
  if (!session_->video_channel()->GetStats(options, &video_info)) {
    LOG(LS_ERROR) << "Failed to get video channel stats.";
    return;
  }
  std::string transport_id;
  if (!GetTransportIdFromProxy(proxy_to_transport_,
                               session_->video_channel()->content_name(),
                               &transport_id)) {
    LOG(LS_ERROR) << "Failed to get transport name for proxy "
                  << session_->video_channel()->content_name();
    return;
  }
  ExtractStatsFromList(video_info.receivers, transport_id, this,
      StatsReport::kReceive);
  ExtractStatsFromList(video_info.senders, transport_id, this,
      StatsReport::kSend);
  if (video_info.bw_estimations.size() != 1) {
    LOG(LS_ERROR) << "BWEs count: " << video_info.bw_estimations.size();
  } else {
    rtc::scoped_ptr<StatsReport::Id> report_id(
        StatsReport::NewBandwidthEstimationId());
    StatsReport* report = reports_.FindOrAddNew(report_id.Pass());
    ExtractStats(
        video_info.bw_estimations[0], stats_gathering_started_, level, report);
  }
}

void StatsCollector::ExtractDataInfo() {
  ASSERT(session_->signaling_thread()->IsCurrent());

  for (const auto& dc :
           session_->mediastream_signaling()->sctp_data_channels()) {
    rtc::scoped_ptr<StatsReport::Id> id(StatsReport::NewTypedIntId(
        StatsReport::kStatsReportTypeDataChannel, dc->id()));
    StatsReport* report = reports_.ReplaceOrAddNew(id.Pass());
    report->set_timestamp(stats_gathering_started_);
    report->AddValue(StatsReport::kStatsValueNameLabel, dc->label());
    report->AddValue(StatsReport::kStatsValueNameDataChannelId, dc->id());
    report->AddValue(StatsReport::kStatsValueNameProtocol, dc->protocol());
    report->AddValue(StatsReport::kStatsValueNameState,
                     DataChannelInterface::DataStateString(dc->state()));
  }
}

StatsReport* StatsCollector::GetReport(const StatsReport::StatsType& type,
                                       const std::string& id,
                                       StatsReport::Direction direction) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  ASSERT(type == StatsReport::kStatsReportTypeSsrc ||
         type == StatsReport::kStatsReportTypeRemoteSsrc);
  return reports_.Find(StatsReport::NewIdWithDirection(type, id, direction));
}

StatsReport* StatsCollector::GetOrCreateReport(
    const StatsReport::StatsType& type,
    const std::string& id,
    StatsReport::Direction direction) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  ASSERT(type == StatsReport::kStatsReportTypeSsrc ||
         type == StatsReport::kStatsReportTypeRemoteSsrc);
  StatsReport* report = GetReport(type, id, direction);
  if (report == NULL) {
    rtc::scoped_ptr<StatsReport::Id> report_id(
        StatsReport::NewIdWithDirection(type, id, direction));
    report = reports_.InsertNew(report_id.Pass());
  }

  return report;
}

void StatsCollector::UpdateStatsFromExistingLocalAudioTracks() {
  ASSERT(session_->signaling_thread()->IsCurrent());
  // Loop through the existing local audio tracks.
  for (LocalAudioTrackVector::const_iterator it = local_audio_tracks_.begin();
       it != local_audio_tracks_.end(); ++it) {
    AudioTrackInterface* track = it->first;
    uint32 ssrc = it->second;
    std::string ssrc_id = rtc::ToString<uint32>(ssrc);
    StatsReport* report = GetReport(StatsReport::kStatsReportTypeSsrc,
                                    ssrc_id,
                                    StatsReport::kSend);
    if (report == NULL) {
      // This can happen if a local audio track is added to a stream on the
      // fly and the report has not been set up yet. Do nothing in this case.
      LOG(LS_ERROR) << "Stats report does not exist for ssrc " << ssrc;
      continue;
    }

    // The same ssrc can be used by both local and remote audio tracks.
    const StatsReport::Value* v =
        report->FindValue(StatsReport::kStatsValueNameTrackId);
    if (!v || v->value != track->id())
      continue;

    UpdateReportFromAudioTrack(track, report);
  }
}

void StatsCollector::UpdateReportFromAudioTrack(AudioTrackInterface* track,
                                                StatsReport* report) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  ASSERT(track != NULL);
  if (report == NULL)
    return;

  int signal_level = 0;
  if (track->GetSignalLevel(&signal_level)) {
    report->ReplaceValue(StatsReport::kStatsValueNameAudioInputLevel,
                         rtc::ToString<int>(signal_level));
  }

  rtc::scoped_refptr<AudioProcessorInterface> audio_processor(
      track->GetAudioProcessor());
  if (audio_processor.get() == NULL)
    return;

  AudioProcessorInterface::AudioProcessorStats stats;
  audio_processor->GetStats(&stats);
  report->ReplaceValue(StatsReport::kStatsValueNameTypingNoiseState,
                       stats.typing_noise_detected ? "true" : "false");
  report->ReplaceValue(StatsReport::kStatsValueNameEchoReturnLoss,
                       rtc::ToString<int>(stats.echo_return_loss));
  report->ReplaceValue(
      StatsReport::kStatsValueNameEchoReturnLossEnhancement,
      rtc::ToString<int>(stats.echo_return_loss_enhancement));
  report->ReplaceValue(StatsReport::kStatsValueNameEchoDelayMedian,
                       rtc::ToString<int>(stats.echo_delay_median_ms));
  report->ReplaceValue(StatsReport::kStatsValueNameEchoCancellationQualityMin,
                       rtc::ToString<float>(stats.aec_quality_min));
  report->ReplaceValue(StatsReport::kStatsValueNameEchoDelayStdDev,
                       rtc::ToString<int>(stats.echo_delay_std_ms));
}

bool StatsCollector::GetTrackIdBySsrc(uint32 ssrc, std::string* track_id,
                                      StatsReport::Direction direction) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  if (direction == StatsReport::kSend) {
    if (!session_->GetLocalTrackIdBySsrc(ssrc, track_id)) {
      LOG(LS_WARNING) << "The SSRC " << ssrc
                      << " is not associated with a sending track";
      return false;
    }
  } else {
    ASSERT(direction == StatsReport::kReceive);
    if (!session_->GetRemoteTrackIdBySsrc(ssrc, track_id)) {
      LOG(LS_WARNING) << "The SSRC " << ssrc
                      << " is not associated with a receiving track";
      return false;
    }
  }

  return true;
}

void StatsCollector::ClearUpdateStatsCacheForTest() {
  stats_gathering_started_ = 0;
}

}  // namespace webrtc
