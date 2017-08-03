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

#include "webrtc/call/rsid_resolution_observer.h"
#include "webrtc/call/rtp_packet_sink_interface.h"
#include "webrtc/call/rtp_rtcp_demuxer_helper.h"
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

void RtpDemuxer::AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  RecordSsrcToSinkAssociation(ssrc, sink);
}

void RtpDemuxer::AddSink(const std::string& rsid,
                         RtpPacketSinkInterface* sink) {
  RTC_DCHECK(StreamId::IsLegalName(rsid));
  RTC_DCHECK(sink);
  RTC_DCHECK(!MultimapAssociationExists(rsid_sinks_, rsid, sink));

  rsid_sinks_.emplace(rsid, sink);
}

bool RtpDemuxer::RemoveSink(const RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  return (RemoveFromMultimapByValue(&ssrc_sinks_, sink) +
          RemoveFromMultimapByValue(&rsid_sinks_, sink)) > 0;
}

void RtpDemuxer::RecordSsrcToSinkAssociation(uint32_t ssrc,
                                             RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  // The association might already have been set by a different
  // configuration source.
  if (!MultimapAssociationExists(ssrc_sinks_, ssrc, sink)) {
    ssrc_sinks_.emplace(ssrc, sink);
  }
}

bool RtpDemuxer::OnRtpPacket(const RtpPacketReceived& packet) {
  // TODO(eladalon): This will now check every single packet, but soon a CL will
  // be added which will change the many-to-many association of packets to sinks
  // to a many-to-one, meaning each packet will be associated with one sink
  // at most. Then, only packets with an unknown SSRC will be checked for RSID.
  ResolveRsidToSsrcAssociations(packet);

  auto it_range = ssrc_sinks_.equal_range(packet.Ssrc());
  for (auto it = it_range.first; it != it_range.second; ++it) {
    it->second->OnRtpPacket(packet);
  }
  return it_range.first != it_range.second;
}

void RtpDemuxer::RegisterRsidResolutionObserver(
    RsidResolutionObserver* observer) {
  RTC_DCHECK(observer);
  RTC_DCHECK(!ContainerHasKey(rsid_resolution_observers_, observer));

  rsid_resolution_observers_.push_back(observer);
}

void RtpDemuxer::DeregisterRsidResolutionObserver(
    const RsidResolutionObserver* observer) {
  RTC_DCHECK(observer);
  auto it = std::find(rsid_resolution_observers_.begin(),
                      rsid_resolution_observers_.end(), observer);
  RTC_DCHECK(it != rsid_resolution_observers_.end());
  rsid_resolution_observers_.erase(it);
}

void RtpDemuxer::ResolveRsidToSsrcAssociations(
    const RtpPacketReceived& packet) {
  std::string rsid;
  if (packet.GetExtension<RtpStreamId>(&rsid)) {
    // All streams associated with this RSID need to be marked as associated
    // with this SSRC (if they aren't already).
    auto it_range = rsid_sinks_.equal_range(rsid);
    for (auto it = it_range.first; it != it_range.second; ++it) {
      RecordSsrcToSinkAssociation(packet.Ssrc(), it->second);
    }

    NotifyObserversOfRsidResolution(rsid, packet.Ssrc());

    // To prevent memory-overuse attacks, forget this RSID. Future packets
    // with this RSID, but a different SSRC, will not spawn new associations.
    rsid_sinks_.erase(it_range.first, it_range.second);
  }
}

void RtpDemuxer::NotifyObserversOfRsidResolution(const std::string& rsid,
                                                 uint32_t ssrc) {
  for (auto* observer : rsid_resolution_observers_) {
    observer->OnRsidResolved(rsid, ssrc);
  }
}

}  // namespace webrtc
