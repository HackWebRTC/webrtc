/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_generic_ack_received.h"

#include <vector>

namespace webrtc {
std::vector<std::unique_ptr<RtcEventGenericAckReceived>>
RtcEventGenericAckReceived::CreateLogs(
    int64_t packet_number,
    const std::vector<AckedPacket> acked_packets) {
  std::vector<std::unique_ptr<RtcEventGenericAckReceived>> result;
  int64_t time_us = rtc::TimeMicros();
  for (const AckedPacket& packet : acked_packets) {
    result.push_back(absl::WrapUnique(new RtcEventGenericAckReceived(
        time_us, packet_number, packet.packet_number,
        packet.receive_timestamp_ms)));
  }
  return result;
}

RtcEventGenericAckReceived::RtcEventGenericAckReceived(
    int64_t timestamp_us,
    int64_t packet_number,
    int64_t acked_packet_number,
    absl::optional<int64_t> receive_timestamp_ms)
    : RtcEvent(timestamp_us),
      packet_number_(packet_number),
      acked_packet_number_(acked_packet_number),
      receive_timestamp_ms_(receive_timestamp_ms) {}

RtcEventGenericAckReceived::~RtcEventGenericAckReceived() = default;

RtcEvent::Type RtcEventGenericAckReceived::GetType() const {
  return RtcEvent::Type::GenericAckReceived;
}

bool RtcEventGenericAckReceived::IsConfigEvent() const {
  return false;
}

}  // namespace webrtc
