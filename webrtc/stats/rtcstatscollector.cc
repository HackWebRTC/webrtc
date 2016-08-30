/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/stats/rtcstatscollector.h"

#include <memory>
#include <utility>
#include <vector>

#include "webrtc/api/peerconnection.h"
#include "webrtc/base/checks.h"

namespace webrtc {

RTCStatsCollector::RTCStatsCollector(
    PeerConnection* pc,
    double cache_lifetime,
    std::unique_ptr<rtc::Timing> timing)
    : pc_(pc),
      timing_(std::move(timing)),
      cache_timestamp_(0.0),
      cache_lifetime_(cache_lifetime) {
  RTC_DCHECK(pc_);
  RTC_DCHECK(timing_);
  RTC_DCHECK(IsOnSignalingThread());
  RTC_DCHECK_GE(cache_lifetime_, 0.0);
}

rtc::scoped_refptr<const RTCStatsReport> RTCStatsCollector::GetStatsReport() {
  RTC_DCHECK(IsOnSignalingThread());
  double now = timing_->TimerNow();
  if (cached_report_ && now - cache_timestamp_ <= cache_lifetime_)
    return cached_report_;
  cache_timestamp_ = now;

  rtc::scoped_refptr<RTCStatsReport> report = RTCStatsReport::Create();
  report->AddStats(ProducePeerConnectionStats());

  cached_report_ = report;
  return cached_report_;
}

void RTCStatsCollector::ClearCachedStatsReport() {
  RTC_DCHECK(IsOnSignalingThread());
  cached_report_ = nullptr;
}

bool RTCStatsCollector::IsOnSignalingThread() const {
  return pc_->session()->signaling_thread()->IsCurrent();
}

std::unique_ptr<RTCPeerConnectionStats>
RTCStatsCollector::ProducePeerConnectionStats() const {
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
    new RTCPeerConnectionStats("RTCPeerConnection", cache_timestamp_));
  stats->data_channels_opened = data_channels_opened;
  stats->data_channels_closed = static_cast<uint32_t>(data_channels.size()) -
                                data_channels_opened;
  return stats;
}

}  // namespace webrtc
