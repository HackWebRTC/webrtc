/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/rtp_demuxer.h"

#include "webrtc/call/rtp_packet_sink_interface.h"
#include "webrtc/call/rtp_rtcp_demuxer_helper.h"
#include "webrtc/call/ssrc_binding_observer.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/logging.h"

namespace webrtc {

RtpDemuxer::RtpDemuxer() = default;

RtpDemuxer::~RtpDemuxer() {
  RTC_DCHECK(ssrc_sinks_.empty());
  RTC_DCHECK(rsid_sinks_.empty());
}

bool RtpDemuxer::AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  // The association might already have been set by a different
  // configuration source.
  // We cannot RTC_DCHECK against an attempt to remap an SSRC, because
  // such a configuration might have come from the network (1. resolution
  // of an RSID or 2. RTCP messages with RSID resolutions).
  return ssrc_sinks_.emplace(ssrc, sink).second;
}

void RtpDemuxer::AddSink(const std::string& rsid,
                         RtpPacketSinkInterface* sink) {
  RTC_DCHECK(StreamId::IsLegalName(rsid));
  RTC_DCHECK(sink);
  RTC_DCHECK(rsid_sinks_.find(rsid) == rsid_sinks_.cend());

  rsid_sinks_.emplace(rsid, sink);
}

bool RtpDemuxer::RemoveSink(const RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  return (RemoveFromMapByValue(&ssrc_sinks_, sink) +
          RemoveFromMapByValue(&rsid_sinks_, sink)) > 0;
}

bool RtpDemuxer::OnRtpPacket(const RtpPacketReceived& packet) {
  ResolveRsidToSsrcAssociations(packet);

  auto it_range = ssrc_sinks_.equal_range(packet.Ssrc());
  for (auto it = it_range.first; it != it_range.second; ++it) {
    it->second->OnRtpPacket(packet);
  }
  return it_range.first != it_range.second;
}

void RtpDemuxer::RegisterSsrcBindingObserver(SsrcBindingObserver* observer) {
  RTC_DCHECK(observer);
  RTC_DCHECK(!ContainerHasKey(ssrc_binding_observers_, observer));

  ssrc_binding_observers_.push_back(observer);
}
void RtpDemuxer::RegisterRsidResolutionObserver(SsrcBindingObserver* observer) {
  RegisterSsrcBindingObserver(observer);
}

void RtpDemuxer::DeregisterSsrcBindingObserver(
    const SsrcBindingObserver* observer) {
  RTC_DCHECK(observer);
  auto it = std::find(ssrc_binding_observers_.begin(),
                      ssrc_binding_observers_.end(), observer);
  RTC_DCHECK(it != ssrc_binding_observers_.end());
  ssrc_binding_observers_.erase(it);
}
void RtpDemuxer::DeregisterRsidResolutionObserver(
    const SsrcBindingObserver* observer) {
  DeregisterSsrcBindingObserver(observer);
}

void RtpDemuxer::ResolveRsidToSsrcAssociations(
    const RtpPacketReceived& packet) {
  std::string rsid;
  if (!packet.GetExtension<RtpStreamId>(&rsid)) {
    return;
  }

  auto it = rsid_sinks_.find(rsid);
  if (it == rsid_sinks_.end()) {
    // Might be unknown, or we might have already associated this RSID
    // with a sink.
    return;
  }

  // If a sink is associated with an RSID, we should associate it with
  // this SSRC.
  if (!AddSink(packet.Ssrc(), it->second)) {
    // In the faulty case of RSIDs mapped to SSRCs which are already associated
    // with a sink, avoid propagating the problem to the resolution observers.
    LOG(LS_WARNING) << "RSID (" << rsid << ") resolved to preconfigured SSRC ("
                    << packet.Ssrc() << ").";
    return;
  }

  // We make the assumption that observers are only interested in notifications
  // for RSIDs which are registered with this module. (RTCP sinks are normally
  // created with RTP sinks.)
  NotifyObserversOfRsidResolution(rsid, packet.Ssrc());

  // This RSID cannot later be associated with another SSRC.
  rsid_sinks_.erase(it);
}

void RtpDemuxer::NotifyObserversOfRsidResolution(const std::string& rsid,
                                                 uint32_t ssrc) {
  for (auto* observer : ssrc_binding_observers_) {
    observer->OnSsrcBoundToRsid(rsid, ssrc);
  }
}

}  // namespace webrtc
