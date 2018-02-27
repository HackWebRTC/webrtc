/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/rtp/network_control/include/network_types.h"

namespace webrtc {

StreamsConfig::StreamsConfig() = default;
StreamsConfig::StreamsConfig(const StreamsConfig&) = default;
StreamsConfig::~StreamsConfig() = default;

PacketResult::PacketResult() = default;
PacketResult::PacketResult(const PacketResult& other) = default;
PacketResult::~PacketResult() = default;

TransportPacketsFeedback::TransportPacketsFeedback() = default;
TransportPacketsFeedback::TransportPacketsFeedback(
    const TransportPacketsFeedback& other) = default;
TransportPacketsFeedback::~TransportPacketsFeedback() = default;

std::vector<PacketResult> TransportPacketsFeedback::ReceivedWithSendInfo()
    const {
  std::vector<PacketResult> res;
  for (const PacketResult& fb : packet_feedbacks) {
    if (fb.receive_time.IsFinite() && fb.sent_packet.has_value()) {
      res.push_back(fb);
    }
  }
  return res;
}

std::vector<PacketResult> TransportPacketsFeedback::LostWithSendInfo() const {
  std::vector<PacketResult> res;
  for (const PacketResult& fb : packet_feedbacks) {
    if (fb.receive_time.IsInfinite() && fb.sent_packet.has_value()) {
      res.push_back(fb);
    }
  }
  return res;
}

std::vector<PacketResult> TransportPacketsFeedback::PacketsWithFeedback()
    const {
  return packet_feedbacks;
}

::std::ostream& operator<<(::std::ostream& os,
                           const ProbeClusterConfig& config) {
  return os << "ProbeClusterConfig(...)";
}

::std::ostream& operator<<(::std::ostream& os, const PacerConfig& config) {
  return os << "PacerConfig(...)";
}

}  // namespace webrtc
