/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/call/rtp_demuxer.h"
#include "webrtc/call/rtp_packet_sink_interface.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

namespace {

template <typename Key, typename Value>
bool MultimapAssociationExists(const std::multimap<Key, Value>& multimap,
                               Key key,
                               Value val) {
  auto it_range = multimap.equal_range(key);
  using Reference = typename std::multimap<Key, Value>::const_reference;
  return std::any_of(it_range.first, it_range.second,
                     [val](Reference elem) { return elem.second == val; });
}

}  // namespace

RtpDemuxer::RtpDemuxer() {}

RtpDemuxer::~RtpDemuxer() {
  RTC_DCHECK(sinks_.empty());
}

void RtpDemuxer::AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  RTC_DCHECK(!MultimapAssociationExists(sinks_, ssrc, sink));
  sinks_.emplace(ssrc, sink);
}

size_t RtpDemuxer::RemoveSink(const RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  size_t count = 0;
  for (auto it = sinks_.begin(); it != sinks_.end(); ) {
    if (it->second == sink) {
      it = sinks_.erase(it);
      ++count;
    } else {
      ++it;
    }
  }
  return count;
}

bool RtpDemuxer::OnRtpPacket(const RtpPacketReceived& packet) {
  bool found = false;
  auto it_range = sinks_.equal_range(packet.Ssrc());
  for (auto it = it_range.first; it != it_range.second; ++it) {
    found = true;
    it->second->OnRtpPacket(packet);
  }
  return found;
}

}  // namespace webrtc
