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
#include "webrtc/api/webrtcsession.h"
#include "webrtc/base/checks.h"
#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/p2pconstants.h"
#include "webrtc/p2p/base/port.h"

namespace webrtc {

namespace {

std::string RTCCertificateIDFromFingerprint(const std::string& fingerprint) {
  return "RTCCertificate_" + fingerprint;
}

std::string RTCIceCandidatePairStatsIDFromConnectionInfo(
    const cricket::ConnectionInfo& info) {
  return "RTCIceCandidatePair_" + info.local_candidate.id() + "_" +
      info.remote_candidate.id();
}

std::string RTCTransportStatsIDFromTransportChannel(
    const std::string& transport_name, int channel_component) {
  return "RTCTransport_" + transport_name + "_" +
      rtc::ToString<>(channel_component);
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

    num_pending_partial_reports_ = 3;
    partial_report_timestamp_us_ = cache_now_us;
    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, signaling_thread_,
        rtc::Bind(&RTCStatsCollector::ProducePartialResultsOnSignalingThread,
            rtc::scoped_refptr<RTCStatsCollector>(this), timestamp_us));
    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, worker_thread_,
        rtc::Bind(&RTCStatsCollector::ProducePartialResultsOnWorkerThread,
            rtc::scoped_refptr<RTCStatsCollector>(this), timestamp_us));
    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, network_thread_,
        rtc::Bind(&RTCStatsCollector::ProducePartialResultsOnNetworkThread,
            rtc::scoped_refptr<RTCStatsCollector>(this), timestamp_us));
  }
}

void RTCStatsCollector::ClearCachedStatsReport() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  cached_report_ = nullptr;
}

void RTCStatsCollector::ProducePartialResultsOnSignalingThread(
    int64_t timestamp_us) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<RTCStatsReport> report = RTCStatsReport::Create();

  SessionStats session_stats;
  if (pc_->session()->GetTransportStats(&session_stats)) {
    std::map<std::string, CertificateStatsPair> transport_cert_stats =
        PrepareTransportCertificateStats_s(session_stats);

    ProduceCertificateStats_s(
        timestamp_us, transport_cert_stats, report.get());
    ProduceIceCandidateAndPairStats_s(
        timestamp_us, session_stats, report.get());
    ProduceTransportStats_s(
        timestamp_us, session_stats, transport_cert_stats, report.get());
  }
  ProduceDataChannelStats_s(timestamp_us, report.get());
  ProducePeerConnectionStats_s(timestamp_us, report.get());

  AddPartialResults(report);
}

void RTCStatsCollector::ProducePartialResultsOnWorkerThread(
    int64_t timestamp_us) {
  RTC_DCHECK(worker_thread_->IsCurrent());
  rtc::scoped_refptr<RTCStatsReport> report = RTCStatsReport::Create();

  // TODO(hbos): Gather stats on worker thread.

  AddPartialResults(report);
}

void RTCStatsCollector::ProducePartialResultsOnNetworkThread(
    int64_t timestamp_us) {
  RTC_DCHECK(network_thread_->IsCurrent());
  rtc::scoped_refptr<RTCStatsReport> report = RTCStatsReport::Create();

  // TODO(hbos): Gather stats on network thread.

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

void RTCStatsCollector::ProduceCertificateStats_s(
    int64_t timestamp_us,
    const std::map<std::string, CertificateStatsPair>& transport_cert_stats,
    RTCStatsReport* report) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  for (const auto& kvp : transport_cert_stats) {
    if (kvp.second.local) {
      ProduceCertificateStatsFromSSLCertificateStats_s(
          timestamp_us, *kvp.second.local.get(), report);
    }
    if (kvp.second.remote) {
      ProduceCertificateStatsFromSSLCertificateStats_s(
          timestamp_us, *kvp.second.remote.get(), report);
    }
  }
}

void RTCStatsCollector::ProduceCertificateStatsFromSSLCertificateStats_s(
    int64_t timestamp_us, const rtc::SSLCertificateStats& certificate_stats,
    RTCStatsReport* report) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  RTCCertificateStats* prev_certificate_stats = nullptr;
  for (const rtc::SSLCertificateStats* s = &certificate_stats; s;
       s = s->issuer.get()) {
    RTCCertificateStats* certificate_stats = new RTCCertificateStats(
        RTCCertificateIDFromFingerprint(s->fingerprint), timestamp_us);
    certificate_stats->fingerprint = s->fingerprint;
    certificate_stats->fingerprint_algorithm = s->fingerprint_algorithm;
    certificate_stats->base64_certificate = s->base64_certificate;
    if (prev_certificate_stats)
      prev_certificate_stats->issuer_certificate_id = certificate_stats->id();
    report->AddStats(std::unique_ptr<RTCCertificateStats>(certificate_stats));
    prev_certificate_stats = certificate_stats;
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

void RTCStatsCollector::ProduceIceCandidateAndPairStats_s(
      int64_t timestamp_us, const SessionStats& session_stats,
      RTCStatsReport* report) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  for (const auto& transport_stats : session_stats.transport_stats) {
    for (const auto& channel_stats : transport_stats.second.channel_stats) {
      for (const cricket::ConnectionInfo& info :
           channel_stats.connection_infos) {
        std::unique_ptr<RTCIceCandidatePairStats> candidate_pair_stats(
            new RTCIceCandidatePairStats(
                RTCIceCandidatePairStatsIDFromConnectionInfo(info),
                timestamp_us));

        // TODO(hbos): Set all of the |RTCIceCandidatePairStats|'s members,
        // crbug.com/633550.

        // TODO(hbos): Set candidate_pair_stats->transport_id. Should be ID to
        // RTCTransportStats which does not exist yet: crbug.com/653873.

        // TODO(hbos): There could be other candidates that are not paired with
        // anything. We don't have a complete list. Local candidates come from
        // Port objects, and prflx candidates (both local and remote) are only
        // stored in candidate pairs. crbug.com/632723
        candidate_pair_stats->local_candidate_id = ProduceIceCandidateStats_s(
            timestamp_us, info.local_candidate, true, report);
        candidate_pair_stats->remote_candidate_id = ProduceIceCandidateStats_s(
            timestamp_us, info.remote_candidate, false, report);

        // TODO(hbos): Set candidate_pair_stats->state.
        // TODO(hbos): Set candidate_pair_stats->priority.
        // TODO(hbos): Set candidate_pair_stats->nominated.
        // TODO(hbos): This writable is different than the spec. It goes to
        // false after a certain amount of time without a response passes.
        // crbug.com/633550
        candidate_pair_stats->writable = info.writable;
        // TODO(hbos): Set candidate_pair_stats->readable.
        candidate_pair_stats->bytes_sent =
            static_cast<uint64_t>(info.sent_total_bytes);
        candidate_pair_stats->bytes_received =
            static_cast<uint64_t>(info.recv_total_bytes);
        // TODO(hbos): Set candidate_pair_stats->total_rtt.
        // TODO(hbos): The |info.rtt| measurement is smoothed. It shouldn't be
        // smoothed according to the spec. crbug.com/633550. See
        // https://w3c.github.io/webrtc-stats/#dom-rtcicecandidatepairstats-currentrtt
        candidate_pair_stats->current_rtt =
            static_cast<double>(info.rtt) / 1000.0;
        // TODO(hbos): Set candidate_pair_stats->available_outgoing_bitrate.
        // TODO(hbos): Set candidate_pair_stats->available_incoming_bitrate.
        // TODO(hbos): Set candidate_pair_stats->requests_received.
        candidate_pair_stats->requests_sent =
            static_cast<uint64_t>(info.sent_ping_requests_total);
        candidate_pair_stats->responses_received =
            static_cast<uint64_t>(info.recv_ping_responses);
        candidate_pair_stats->responses_sent =
            static_cast<uint64_t>(info.sent_ping_responses);
        // TODO(hbos): Set candidate_pair_stats->retransmissions_received.
        // TODO(hbos): Set candidate_pair_stats->retransmissions_sent.
        // TODO(hbos): Set candidate_pair_stats->consent_requests_received.
        // TODO(hbos): Set candidate_pair_stats->consent_requests_sent.
        // TODO(hbos): Set candidate_pair_stats->consent_responses_received.
        // TODO(hbos): Set candidate_pair_stats->consent_responses_sent.

        report->AddStats(std::move(candidate_pair_stats));
      }
    }
  }
}

const std::string& RTCStatsCollector::ProduceIceCandidateStats_s(
    int64_t timestamp_us, const cricket::Candidate& candidate, bool is_local,
    RTCStatsReport* report) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  const std::string& id = "RTCIceCandidate_" + candidate.id();
  const RTCStats* stats = report->Get(id);
  if (!stats) {
    std::unique_ptr<RTCIceCandidateStats> candidate_stats;
    if (is_local)
      candidate_stats.reset(new RTCLocalIceCandidateStats(id, timestamp_us));
    else
      candidate_stats.reset(new RTCRemoteIceCandidateStats(id, timestamp_us));
    candidate_stats->ip = candidate.address().ipaddr().ToString();
    candidate_stats->port = static_cast<int32_t>(candidate.address().port());
    candidate_stats->protocol = candidate.protocol();
    candidate_stats->candidate_type = CandidateTypeToRTCIceCandidateType(
        candidate.type());
    candidate_stats->priority = static_cast<int32_t>(candidate.priority());
    // TODO(hbos): Define candidate_stats->url. crbug.com/632723

    stats = candidate_stats.get();
    report->AddStats(std::move(candidate_stats));
  }
  RTC_DCHECK_EQ(stats->type(), is_local ? RTCLocalIceCandidateStats::kType
                                        : RTCRemoteIceCandidateStats::kType);
  return stats->id();
}

void RTCStatsCollector::ProducePeerConnectionStats_s(
    int64_t timestamp_us, RTCStatsReport* report) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  // TODO(hbos): If data channels are removed from the peer connection this will
  // yield incorrect counts. Address before closing crbug.com/636818. See
  // https://w3c.github.io/webrtc-stats/webrtc-stats.html#pcstats-dict*.
  uint32_t data_channels_opened = 0;
  const std::vector<rtc::scoped_refptr<DataChannel>>& data_channels =
      pc_->sctp_data_channels();
  for (const rtc::scoped_refptr<DataChannel>& data_channel : data_channels) {
    if (data_channel->state() == DataChannelInterface::kOpen)
      ++data_channels_opened;
  }
  // There is always just one |RTCPeerConnectionStats| so its |id| can be a
  // constant.
  std::unique_ptr<RTCPeerConnectionStats> stats(
    new RTCPeerConnectionStats("RTCPeerConnection", timestamp_us));
  stats->data_channels_opened = data_channels_opened;
  stats->data_channels_closed = static_cast<uint32_t>(data_channels.size()) -
                                data_channels_opened;
  report->AddStats(std::move(stats));
}

void RTCStatsCollector::ProduceTransportStats_s(
    int64_t timestamp_us, const SessionStats& session_stats,
    const std::map<std::string, CertificateStatsPair>& transport_cert_stats,
    RTCStatsReport* report) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
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
RTCStatsCollector::PrepareTransportCertificateStats_s(
    const SessionStats& session_stats) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
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

const char* CandidateTypeToRTCIceCandidateTypeForTesting(
    const std::string& type) {
  return CandidateTypeToRTCIceCandidateType(type);
}

const char* DataStateToRTCDataChannelStateForTesting(
    DataChannelInterface::DataState state) {
  return DataStateToRTCDataChannelState(state);
}

}  // namespace webrtc
