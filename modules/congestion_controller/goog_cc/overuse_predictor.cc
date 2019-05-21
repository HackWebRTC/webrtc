/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/overuse_predictor.h"

#include <algorithm>

namespace webrtc {
namespace {
constexpr int kMaxPendingPackets = 100;

DataRate GetAvailableCapacity(const NetworkStateEstimate& est,
                              double deviation) {
  double capacity_bps = est.link_capacity.bps();
  double min_capacity_bps = est.link_capacity_min.bps();
  double deviation_bps = est.link_capacity_std_dev.bps();
  return DataRate::bps(
      std::max(capacity_bps + deviation_bps * deviation, min_capacity_bps));
}
}  // namespace

OverusePredictorConfig::OverusePredictorConfig(const std::string& config) {
  ParseFieldTrial({&enabled, &capacity_dev_ratio_threshold, &capacity_deviation,
                   &delay_threshold},
                  config);
}

OverusePredictor::OverusePredictor(const WebRtcKeyValueConfig* config)
    : conf_(config->Lookup("WebRTC-Bwe-OverusePredictor")) {}

bool OverusePredictor::Enabled() const {
  return conf_.enabled;
}

void OverusePredictor::OnSentPacket(SentPacket sent_packet) {
  pending_.push_back(SentPacketInfo{sent_packet.send_time, sent_packet.size});
  if (pending_.size() > kMaxPendingPackets)
    pending_.pop_front();
}

bool OverusePredictor::PredictOveruse(const NetworkStateEstimate& est) {
  while (!pending_.empty() && pending_.front().send_time < est.last_send_time) {
    pending_.pop_front();
  }
  double deviation_ratio = est.link_capacity_std_dev / est.link_capacity;
  if (deviation_ratio > conf_.capacity_dev_ratio_threshold)
    return false;
  TimeDelta buffer_delay = PredictDelay(est) - est.propagation_delay;
  return buffer_delay > conf_.delay_threshold;
}

TimeDelta OverusePredictor::PredictDelay(const NetworkStateEstimate& est) {
  auto safe_capacity = GetAvailableCapacity(est, conf_.capacity_deviation);
  Timestamp last_send_time = est.last_send_time;
  TimeDelta link_delay = est.pre_link_buffer_delay;
  for (const auto& packet : pending_) {
    auto delta = packet.send_time - last_send_time;
    last_send_time = packet.send_time;
    link_delay = std::max(link_delay - delta, est.propagation_delay);
    link_delay += packet.size / safe_capacity;
  }
  return link_delay;
}

}  // namespace webrtc
