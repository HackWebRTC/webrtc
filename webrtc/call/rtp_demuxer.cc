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
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

RtpDemuxer::RtpDemuxer() {}

RtpDemuxer::~RtpDemuxer() {
  RTC_DCHECK(sinks_.empty());
}

void RtpDemuxer::AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
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
