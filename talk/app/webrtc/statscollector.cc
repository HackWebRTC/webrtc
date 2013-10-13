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

#include "talk/app/webrtc/statscollector.h"

#include <utility>
#include <vector>

#include "talk/base/base64.h"
#include "talk/base/scoped_ptr.h"
#include "talk/session/media/channel.h"

namespace webrtc {

// The items below are in alphabetical order.
const char StatsReport::kStatsValueNameActiveConnection[] =
    "googActiveConnection";
const char StatsReport::kStatsValueNameActualEncBitrate[] =
    "googActualEncBitrate";
const char StatsReport::kStatsValueNameAudioOutputLevel[] = "audioOutputLevel";
const char StatsReport::kStatsValueNameAudioInputLevel[] = "audioInputLevel";
const char StatsReport::kStatsValueNameAvailableReceiveBandwidth[] =
    "googAvailableReceiveBandwidth";
const char StatsReport::kStatsValueNameAvailableSendBandwidth[] =
    "googAvailableSendBandwidth";
const char StatsReport::kStatsValueNameBucketDelay[] = "googBucketDelay";
const char StatsReport::kStatsValueNameBytesReceived[] = "bytesReceived";
const char StatsReport::kStatsValueNameBytesSent[] = "bytesSent";
const char StatsReport::kStatsValueNameChannelId[] = "googChannelId";
const char StatsReport::kStatsValueNameCodecName[] = "googCodecName";
const char StatsReport::kStatsValueNameComponent[] = "googComponent";
const char StatsReport::kStatsValueNameContentName[] = "googContentName";
const char StatsReport::kStatsValueNameDer[] = "googDerBase64";
// Echo metrics from the audio processing module.
const char StatsReport::kStatsValueNameEchoCancellationQualityMin[] =
    "googEchoCancellationQualityMin";
const char StatsReport::kStatsValueNameEchoDelayMedian[] =
    "googEchoCancellationEchoDelayMedian";
const char StatsReport::kStatsValueNameEchoDelayStdDev[] =
    "googEchoCancellationEchoDelayStdDev";
const char StatsReport::kStatsValueNameEchoReturnLoss[] =
    "googEchoCancellationReturnLoss";
const char StatsReport::kStatsValueNameEchoReturnLossEnhancement[] =
    "googEchoCancellationReturnLossEnhancement";

const char StatsReport::kStatsValueNameFingerprint[] = "googFingerprint";
const char StatsReport::kStatsValueNameFingerprintAlgorithm[] =
    "googFingerprintAlgorithm";
const char StatsReport::kStatsValueNameFirsReceived[] = "googFirsReceived";
const char StatsReport::kStatsValueNameFirsSent[] = "googFirsSent";
const char StatsReport::kStatsValueNameFrameHeightReceived[] =
    "googFrameHeightReceived";
const char StatsReport::kStatsValueNameFrameHeightSent[] =
    "googFrameHeightSent";
const char StatsReport::kStatsValueNameFrameRateReceived[] =
    "googFrameRateReceived";
const char StatsReport::kStatsValueNameFrameRateDecoded[] =
    "googFrameRateDecoded";
const char StatsReport::kStatsValueNameFrameRateOutput[] =
    "googFrameRateOutput";
const char StatsReport::kStatsValueNameFrameRateInput[] = "googFrameRateInput";
const char StatsReport::kStatsValueNameFrameRateSent[] = "googFrameRateSent";
const char StatsReport::kStatsValueNameFrameWidthReceived[] =
    "googFrameWidthReceived";
const char StatsReport::kStatsValueNameFrameWidthSent[] = "googFrameWidthSent";
const char StatsReport::kStatsValueNameInitiator[] = "googInitiator";
const char StatsReport::kStatsValueNameIssuerId[] = "googIssuerId";
const char StatsReport::kStatsValueNameJitterReceived[] = "googJitterReceived";
const char StatsReport::kStatsValueNameLocalAddress[] = "googLocalAddress";
const char StatsReport::kStatsValueNameLocalCertificateId[] =
    "googLocalCertificateId";
const char StatsReport::kStatsValueNameNacksReceived[] = "googNacksReceived";
const char StatsReport::kStatsValueNameNacksSent[] = "googNacksSent";
const char StatsReport::kStatsValueNamePacketsReceived[] = "packetsReceived";
const char StatsReport::kStatsValueNamePacketsSent[] = "packetsSent";
const char StatsReport::kStatsValueNamePacketsLost[] = "packetsLost";
const char StatsReport::kStatsValueNameReadable[] = "googReadable";
const char StatsReport::kStatsValueNameRemoteAddress[] = "googRemoteAddress";
const char StatsReport::kStatsValueNameRemoteCertificateId[] =
    "googRemoteCertificateId";
const char StatsReport::kStatsValueNameRetransmitBitrate[] =
    "googRetransmitBitrate";
const char StatsReport::kStatsValueNameRtt[] = "googRtt";
const char StatsReport::kStatsValueNameSsrc[] = "ssrc";
const char StatsReport::kStatsValueNameTargetEncBitrate[] =
    "googTargetEncBitrate";
const char StatsReport::kStatsValueNameTransmitBitrate[] =
    "googTransmitBitrate";
const char StatsReport::kStatsValueNameTransportId[] = "transportId";
const char StatsReport::kStatsValueNameTransportType[] = "googTransportType";
const char StatsReport::kStatsValueNameTrackId[] = "googTrackId";
const char StatsReport::kStatsValueNameTypingNoiseState[] =
    "googTypingNoiseState";
const char StatsReport::kStatsValueNameWritable[] = "googWritable";

const char StatsReport::kStatsReportTypeSession[] = "googLibjingleSession";
const char StatsReport::kStatsReportTypeBwe[] = "VideoBwe";
const char StatsReport::kStatsReportTypeSsrc[] = "ssrc";
const char StatsReport::kStatsReportTypeTrack[] = "googTrack";
const char StatsReport::kStatsReportTypeIceCandidate[] = "iceCandidate";
const char StatsReport::kStatsReportTypeTransport[] = "googTransport";
const char StatsReport::kStatsReportTypeComponent[] = "googComponent";
const char StatsReport::kStatsReportTypeCandidatePair[] = "googCandidatePair";
const char StatsReport::kStatsReportTypeCertificate[] = "googCertificate";

const char StatsReport::kStatsReportVideoBweId[] = "bweforvideo";


// Implementations of functions in statstypes.h
void StatsReport::AddValue(const std::string& name, const std::string& value) {
  Value temp;
  temp.name = name;
  temp.value = value;
  values.push_back(temp);
}

void StatsReport::AddValue(const std::string& name, int64 value) {
  AddValue(name, talk_base::ToString<int64>(value));
}

void StatsReport::AddBoolean(const std::string& name, bool value) {
  AddValue(name, value ? "true" : "false");
}

namespace {
typedef std::map<std::string, StatsReport> StatsMap;

std::string StatsId(const std::string& type, const std::string& id) {
  return type + "_" + id;
}

bool ExtractValueFromReport(
    const StatsReport& report,
    const std::string& name,
    std::string* value) {
  StatsReport::Values::const_iterator it = report.values.begin();
  for (; it != report.values.end(); ++it) {
    if (it->name == name) {
      *value = it->value;
      return true;
    }
  }
  return false;
}

template <class TrackVector>
void CreateTrackReports(const TrackVector& tracks, StatsMap* reports) {
  for (size_t j = 0; j < tracks.size(); ++j) {
    webrtc::MediaStreamTrackInterface* track = tracks[j];
    // Adds an empty track report.
    StatsReport report;
    report.type = StatsReport::kStatsReportTypeTrack;
    report.id = StatsId(StatsReport::kStatsReportTypeTrack, track->id());
    report.AddValue(StatsReport::kStatsValueNameTrackId,
                    track->id());
    (*reports)[report.id] = report;
  }
}

void ExtractStats(const cricket::VoiceReceiverInfo& info, StatsReport* report) {
  report->AddValue(StatsReport::kStatsValueNameAudioOutputLevel,
                   info.audio_level);
  report->AddValue(StatsReport::kStatsValueNameBytesReceived,
                   info.bytes_rcvd);
  report->AddValue(StatsReport::kStatsValueNameJitterReceived,
                   info.jitter_ms);
  report->AddValue(StatsReport::kStatsValueNamePacketsReceived,
                   info.packets_rcvd);
  report->AddValue(StatsReport::kStatsValueNamePacketsLost,
                   info.packets_lost);
}

void ExtractStats(const cricket::VoiceSenderInfo& info, StatsReport* report) {
  report->AddValue(StatsReport::kStatsValueNameAudioInputLevel,
                   info.audio_level);
  report->AddValue(StatsReport::kStatsValueNameBytesSent,
                   info.bytes_sent);
  report->AddValue(StatsReport::kStatsValueNamePacketsSent,
                   info.packets_sent);
  report->AddValue(StatsReport::kStatsValueNameJitterReceived,
                   info.jitter_ms);
  report->AddValue(StatsReport::kStatsValueNameRtt, info.rtt_ms);
  report->AddValue(StatsReport::kStatsValueNameEchoCancellationQualityMin,
                   talk_base::ToString<float>(info.aec_quality_min));
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
}

void ExtractStats(const cricket::VideoSenderInfo& info, StatsReport* report) {
  report->AddValue(StatsReport::kStatsValueNameBytesSent,
                   info.bytes_sent);
  report->AddValue(StatsReport::kStatsValueNamePacketsSent,
                   info.packets_sent);

  report->AddValue(StatsReport::kStatsValueNameFirsReceived,
                   info.firs_rcvd);
  report->AddValue(StatsReport::kStatsValueNameNacksReceived,
                   info.nacks_rcvd);
  report->AddValue(StatsReport::kStatsValueNameFrameWidthSent,
                   info.frame_width);
  report->AddValue(StatsReport::kStatsValueNameFrameHeightSent,
                   info.frame_height);
  report->AddValue(StatsReport::kStatsValueNameFrameRateInput,
                   info.framerate_input);
  report->AddValue(StatsReport::kStatsValueNameFrameRateSent,
                   info.framerate_sent);
  report->AddValue(StatsReport::kStatsValueNameRtt, info.rtt_ms);
  report->AddValue(StatsReport::kStatsValueNameCodecName, info.codec_name);
}

void ExtractStats(const cricket::BandwidthEstimationInfo& info,
                  double stats_gathering_started,
                  StatsReport* report) {
  report->id = StatsReport::kStatsReportVideoBweId;
  report->type = StatsReport::kStatsReportTypeBwe;

  // Clear out stats from previous GatherStats calls if any.
  if (report->timestamp != stats_gathering_started) {
    report->values.clear();
    report->timestamp = stats_gathering_started;
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
}

uint32 ExtractSsrc(const cricket::VoiceReceiverInfo& info) {
  return info.ssrc;
}

uint32 ExtractSsrc(const cricket::VoiceSenderInfo& info) {
  return info.ssrc;
}

uint32 ExtractSsrc(const cricket::VideoReceiverInfo& info) {
  return info.ssrcs[0];
}

uint32 ExtractSsrc(const cricket::VideoSenderInfo& info) {
  return info.ssrcs[0];
}

// Template to extract stats from a data vector.
// ExtractSsrc and ExtractStats must be defined and overloaded for each type.
template<typename T>
void ExtractStatsFromList(const std::vector<T>& data,
                          const std::string& transport_id,
                          StatsCollector* collector) {
  typename std::vector<T>::const_iterator it = data.begin();
  for (; it != data.end(); ++it) {
    std::string id;
    uint32 ssrc = ExtractSsrc(*it);
    StatsReport* report = collector->PrepareReport(ssrc, transport_id);
    if (!report) {
      continue;
    }
    ExtractStats(*it, report);
  }
};

}  // namespace

StatsCollector::StatsCollector()
    : session_(NULL), stats_gathering_started_(0) {
}

// Adds a MediaStream with tracks that can be used as a |selector| in a call
// to GetStats.
void StatsCollector::AddStream(MediaStreamInterface* stream) {
  ASSERT(stream != NULL);

  CreateTrackReports<AudioTrackVector>(stream->GetAudioTracks(),
                                       &reports_);
  CreateTrackReports<VideoTrackVector>(stream->GetVideoTracks(),
                                       &reports_);
}

bool StatsCollector::GetStats(MediaStreamTrackInterface* track,
                              StatsReports* reports) {
  ASSERT(reports != NULL);
  reports->clear();

  StatsMap::iterator it;
  if (!track) {
    for (it = reports_.begin(); it != reports_.end(); ++it) {
      reports->push_back(it->second);
    }
    return true;
  }

  it = reports_.find(StatsId(StatsReport::kStatsReportTypeSession,
                             session_->id()));
  if (it != reports_.end()) {
    reports->push_back(it->second);
  }

  it = reports_.find(StatsId(StatsReport::kStatsReportTypeTrack, track->id()));

  if (it == reports_.end()) {
    LOG(LS_WARNING) << "No StatsReport is available for "<< track->id();
    return false;
  }

  reports->push_back(it->second);

  std::string track_id;
  for (it = reports_.begin(); it != reports_.end(); ++it) {
    if (it->second.type != StatsReport::kStatsReportTypeSsrc) {
      continue;
    }
    if (ExtractValueFromReport(it->second,
                               StatsReport::kStatsValueNameTrackId,
                               &track_id)) {
      if (track_id == track->id()) {
        reports->push_back(it->second);
      }
    }
  }

  return true;
}

void StatsCollector::UpdateStats() {
  double time_now = GetTimeNow();
  // Calls to UpdateStats() that occur less than kMinGatherStatsPeriod number of
  // ms apart will be ignored.
  const double kMinGatherStatsPeriod = 50;
  if (stats_gathering_started_ + kMinGatherStatsPeriod > time_now) {
    return;
  }
  stats_gathering_started_ = time_now;

  if (session_) {
    ExtractSessionInfo();
    ExtractVoiceInfo();
    ExtractVideoInfo();
  }
}

StatsReport* StatsCollector::PrepareReport(uint32 ssrc,
                                           const std::string& transport_id) {
  std::string ssrc_id = talk_base::ToString<uint32>(ssrc);
  StatsMap::iterator it = reports_.find(StatsId(
      StatsReport::kStatsReportTypeSsrc, ssrc_id));

  std::string track_id;
  if (it == reports_.end()) {
    if (!session()->GetTrackIdBySsrc(ssrc, &track_id)) {
      LOG(LS_WARNING) << "The SSRC " << ssrc
                      << " is not associated with a track";
      return NULL;
    }
  } else {
    // Keeps the old track id since we want to report the stats for inactive
    // tracks.
    ExtractValueFromReport(it->second,
                           StatsReport::kStatsValueNameTrackId,
                           &track_id);
  }

  StatsReport* report = &reports_[
      StatsId(StatsReport::kStatsReportTypeSsrc, ssrc_id)];
  report->id = StatsId(StatsReport::kStatsReportTypeSsrc, ssrc_id);
  report->type = StatsReport::kStatsReportTypeSsrc;

  // Clear out stats from previous GatherStats calls if any.
  if (report->timestamp != stats_gathering_started_) {
    report->values.clear();
    report->timestamp = stats_gathering_started_;
  }

  report->AddValue(StatsReport::kStatsValueNameSsrc, ssrc_id);
  report->AddValue(StatsReport::kStatsValueNameTrackId, track_id);
  // Add the mapping of SSRC to transport.
  report->AddValue(StatsReport::kStatsValueNameTransportId,
                   transport_id);
  return report;
}

std::string StatsCollector::AddOneCertificateReport(
    const talk_base::SSLCertificate* cert, const std::string& issuer_id) {
  // TODO(bemasc): Move this computation to a helper class that caches these
  // values to reduce CPU use in GetStats.  This will require adding a fast
  // SSLCertificate::Equals() method to detect certificate changes.

  std::string digest_algorithm;
  if (!cert->GetSignatureDigestAlgorithm(&digest_algorithm))
    return std::string();

  talk_base::scoped_ptr<talk_base::SSLFingerprint> ssl_fingerprint(
      talk_base::SSLFingerprint::Create(digest_algorithm, cert));
  std::string fingerprint = ssl_fingerprint->GetRfc4572Fingerprint();

  talk_base::Buffer der_buffer;
  cert->ToDER(&der_buffer);
  std::string der_base64;
  talk_base::Base64::EncodeFromArray(
      der_buffer.data(), der_buffer.length(), &der_base64);

  StatsReport report;
  report.type = StatsReport::kStatsReportTypeCertificate;
  report.id = StatsId(report.type, fingerprint);
  report.timestamp = stats_gathering_started_;
  report.AddValue(StatsReport::kStatsValueNameFingerprint, fingerprint);
  report.AddValue(StatsReport::kStatsValueNameFingerprintAlgorithm,
                  digest_algorithm);
  report.AddValue(StatsReport::kStatsValueNameDer, der_base64);
  if (!issuer_id.empty())
    report.AddValue(StatsReport::kStatsValueNameIssuerId, issuer_id);
  reports_[report.id] = report;
  return report.id;
}

std::string StatsCollector::AddCertificateReports(
    const talk_base::SSLCertificate* cert) {
  // Produces a chain of StatsReports representing this certificate and the rest
  // of its chain, and adds those reports to |reports_|.  The return value is
  // the id of the leaf report.  The provided cert must be non-null, so at least
  // one report will always be provided and the returned string will never be
  // empty.
  ASSERT(cert != NULL);

  std::string issuer_id;
  talk_base::scoped_ptr<talk_base::SSLCertChain> chain;
  if (cert->GetChain(chain.accept())) {
    // This loop runs in reverse, i.e. from root to leaf, so that each
    // certificate's issuer's report ID is known before the child certificate's
    // report is generated.  The root certificate does not have an issuer ID
    // value.
    for (ptrdiff_t i = chain->GetSize() - 1; i >= 0; --i) {
      const talk_base::SSLCertificate& cert_i = chain->Get(i);
      issuer_id = AddOneCertificateReport(&cert_i, issuer_id);
    }
  }
  // Add the leaf certificate.
  return AddOneCertificateReport(cert, issuer_id);
}

void StatsCollector::ExtractSessionInfo() {
  // Extract information from the base session.
  StatsReport report;
  report.id = StatsId(StatsReport::kStatsReportTypeSession, session_->id());
  report.type = StatsReport::kStatsReportTypeSession;
  report.timestamp = stats_gathering_started_;
  report.values.clear();
  report.AddBoolean(StatsReport::kStatsValueNameInitiator,
                    session_->initiator());

  reports_[report.id] = report;

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
      std::string local_cert_report_id, remote_cert_report_id;
      cricket::Transport* transport =
          session_->GetTransport(transport_iter->second.content_name);
      if (transport) {
        talk_base::scoped_ptr<talk_base::SSLIdentity> identity;
        if (transport->GetIdentity(identity.accept()))
          local_cert_report_id = AddCertificateReports(
              &(identity->certificate()));

        talk_base::scoped_ptr<talk_base::SSLCertificate> cert;
        if (transport->GetRemoteCertificate(cert.accept()))
          remote_cert_report_id = AddCertificateReports(cert.get());
      }
      for (cricket::TransportChannelStatsList::iterator channel_iter
               = transport_iter->second.channel_stats.begin();
           channel_iter != transport_iter->second.channel_stats.end();
           ++channel_iter) {
        StatsReport channel_report;
        std::ostringstream ostc;
        ostc << "Channel-" << transport_iter->second.content_name
             << "-" << channel_iter->component;
        channel_report.id = ostc.str();
        channel_report.type = StatsReport::kStatsReportTypeComponent;
        channel_report.timestamp = stats_gathering_started_;
        channel_report.AddValue(StatsReport::kStatsValueNameComponent,
                                channel_iter->component);
        if (!local_cert_report_id.empty())
          channel_report.AddValue(
              StatsReport::kStatsValueNameLocalCertificateId,
              local_cert_report_id);
        if (!remote_cert_report_id.empty())
          channel_report.AddValue(
              StatsReport::kStatsValueNameRemoteCertificateId,
              remote_cert_report_id);
        reports_[channel_report.id] = channel_report;
        for (size_t i = 0;
             i < channel_iter->connection_infos.size();
             ++i) {
          StatsReport report;
          const cricket::ConnectionInfo& info
              = channel_iter->connection_infos[i];
          std::ostringstream ost;
          ost << "Conn-" << transport_iter->first << "-"
              << channel_iter->component << "-" << i;
          report.id = ost.str();
          report.type = StatsReport::kStatsReportTypeCandidatePair;
          report.timestamp = stats_gathering_started_;
          // Link from connection to its containing channel.
          report.AddValue(StatsReport::kStatsValueNameChannelId,
                          channel_report.id);
          report.AddValue(StatsReport::kStatsValueNameBytesSent,
                          info.sent_total_bytes);
          report.AddValue(StatsReport::kStatsValueNameBytesReceived,
                          info.recv_total_bytes);
          report.AddBoolean(StatsReport::kStatsValueNameWritable,
                            info.writable);
          report.AddBoolean(StatsReport::kStatsValueNameReadable,
                            info.readable);
          report.AddBoolean(StatsReport::kStatsValueNameActiveConnection,
                            info.best_connection);
          report.AddValue(StatsReport::kStatsValueNameLocalAddress,
                          info.local_candidate.address().ToString());
          report.AddValue(StatsReport::kStatsValueNameRemoteAddress,
                          info.remote_candidate.address().ToString());
          reports_[report.id] = report;
        }
      }
    }
  }
}

void StatsCollector::ExtractVoiceInfo() {
  if (!session_->voice_channel()) {
    return;
  }
  cricket::VoiceMediaInfo voice_info;
  if (!session_->voice_channel()->GetStats(&voice_info)) {
    LOG(LS_ERROR) << "Failed to get voice channel stats.";
    return;
  }
  std::string transport_id;
  if (!GetTransportIdFromProxy(session_->voice_channel()->content_name(),
                               &transport_id)) {
    LOG(LS_ERROR) << "Failed to get transport name for proxy "
                  << session_->voice_channel()->content_name();
    return;
  }
  ExtractStatsFromList(voice_info.receivers, transport_id, this);
  ExtractStatsFromList(voice_info.senders, transport_id, this);
}

void StatsCollector::ExtractVideoInfo() {
  if (!session_->video_channel()) {
    return;
  }
  cricket::VideoMediaInfo video_info;
  if (!session_->video_channel()->GetStats(&video_info)) {
    LOG(LS_ERROR) << "Failed to get video channel stats.";
    return;
  }
  std::string transport_id;
  if (!GetTransportIdFromProxy(session_->video_channel()->content_name(),
                               &transport_id)) {
    LOG(LS_ERROR) << "Failed to get transport name for proxy "
                  << session_->video_channel()->content_name();
    return;
  }
  ExtractStatsFromList(video_info.receivers, transport_id, this);
  ExtractStatsFromList(video_info.senders, transport_id, this);
  if (video_info.bw_estimations.size() != 1) {
    LOG(LS_ERROR) << "BWEs count: " << video_info.bw_estimations.size();
  } else {
    StatsReport* report = &reports_[StatsReport::kStatsReportVideoBweId];
    ExtractStats(
        video_info.bw_estimations[0], stats_gathering_started_, report);
  }
}

double StatsCollector::GetTimeNow() {
  return timing_.WallTimeNow() * talk_base::kNumMillisecsPerSec;
}

bool StatsCollector::GetTransportIdFromProxy(const std::string& proxy,
                                             std::string* transport) {
  // TODO(hta): Remove handling of empty proxy name once tests do not use it.
  if (proxy.empty()) {
    transport->clear();
    return true;
  }
  if (proxy_to_transport_.find(proxy) == proxy_to_transport_.end()) {
    LOG(LS_ERROR) << "No transport ID mapping for " << proxy;
    return false;
  }
  std::ostringstream ost;
  // Component 1 is always used for RTP.
  ost << "Channel-" << proxy_to_transport_[proxy] << "-1";
  *transport = ost.str();
  return true;
}

}  // namespace webrtc
