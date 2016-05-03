/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/send_delay_stats.h"

#include "webrtc/base/logging.h"
#include "webrtc/system_wrappers/include/metrics.h"

namespace webrtc {
namespace {
// Packet with a larger delay are removed and excluded from the delay stats.
// Set to larger than max histogram delay which is 10000.
const int64_t kMaxSentPacketDelayMs = 11000;
const size_t kMaxPacketMapSize = 2000;

// Limit for the maximum number of streams to calculate stats for.
const size_t kMaxSsrcMapSize = 50;
const int kMinRequiredSamples = 200;
}  // namespace

SendDelayStats::SendDelayStats(Clock* clock)
    : clock_(clock), num_old_packets_(0), num_skipped_packets_(0) {}

SendDelayStats::~SendDelayStats() {
  if (num_old_packets_ > 0 || num_skipped_packets_ > 0) {
    LOG(LS_WARNING) << "Delay stats: number of old packets " << num_old_packets_
                    << ", skipped packets " << num_skipped_packets_
                    << ". Number of streams " << send_delay_counters_.size();
  }
  UpdateHistograms();
}

void SendDelayStats::UpdateHistograms() {
  rtc::CritScope lock(&crit_);
  for (const auto& it : send_delay_counters_) {
    int send_delay_ms = it.second.Avg(kMinRequiredSamples);
    if (send_delay_ms != -1) {
      RTC_LOGGED_HISTOGRAM_COUNTS_10000("WebRTC.Video.SendDelayInMs",
                                        send_delay_ms);
    }
  }
}

void SendDelayStats::AddSsrcs(const VideoSendStream::Config& config) {
  rtc::CritScope lock(&crit_);
  if (ssrcs_.size() > kMaxSsrcMapSize)
    return;
  for (const auto& ssrc : config.rtp.ssrcs)
    ssrcs_.insert(ssrc);
}

void SendDelayStats::OnSendPacket(uint16_t packet_id,
                                  int64_t capture_time_ms,
                                  uint32_t ssrc) {
  // Packet sent to transport.
  rtc::CritScope lock(&crit_);
  if (ssrcs_.find(ssrc) == ssrcs_.end())
    return;

  int64_t now = clock_->TimeInMilliseconds();
  RemoveOld(now, &packets_);

  if (packets_.size() > kMaxPacketMapSize) {
    ++num_skipped_packets_;
    return;
  }
  packets_.insert(
      std::make_pair(packet_id, Packet(ssrc, capture_time_ms, now)));
}

bool SendDelayStats::OnSentPacket(int packet_id, int64_t time_ms) {
  // Packet leaving socket.
  if (packet_id == -1)
    return false;

  rtc::CritScope lock(&crit_);
  auto it = packets_.find(packet_id);
  if (it == packets_.end())
    return false;

  // TODO(asapersson): Remove SendSideDelayUpdated(), use capture -> sent.
  // Elapsed time from send (to transport) -> sent (leaving socket).
  int diff_ms = time_ms - it->second.send_time_ms;
  send_delay_counters_[it->second.ssrc].Add(diff_ms);
  packets_.erase(it);
  return true;
}

void SendDelayStats::RemoveOld(int64_t now, PacketMap* packets) {
  while (!packets->empty()) {
    auto it = packets->begin();
    if (now - it->second.capture_time_ms < kMaxSentPacketDelayMs)
      break;

    packets->erase(it);
    ++num_old_packets_;
  }
}

void SendDelayStats::SampleCounter::Add(int sample) {
  sum += sample;
  ++num_samples;
}

int SendDelayStats::SampleCounter::Avg(int min_required_samples) const {
  if (num_samples < min_required_samples || num_samples == 0)
    return -1;
  return (sum + (num_samples / 2)) / num_samples;
}

}  // namespace webrtc
