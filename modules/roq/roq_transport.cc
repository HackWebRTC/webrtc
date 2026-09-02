/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/roq_transport.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace webrtc::roq {

RoqPacketTransport::RoqPacketTransport(RoqSession* session,
                                       uint64_t rtp_flow_id,
                                       uint64_t rtcp_flow_id,
                                       PacketAcceptedCallback packet_accepted)
    : session_(session),
      rtp_flow_id_(rtp_flow_id),
      rtcp_flow_id_(rtcp_flow_id),
      packet_accepted_(std::move(packet_accepted)) {}

bool RoqPacketTransport::SendRtp(std::span<const uint8_t> packet,
                                 const PacketOptions& options) {
  return Send(rtp_flow_id_, packet, options);
}

bool RoqPacketTransport::SendRtcp(std::span<const uint8_t> packet,
                                  const PacketOptions& options) {
  return Send(rtcp_flow_id_, packet, options);
}

bool RoqPacketTransport::Send(uint64_t flow_id,
                              std::span<const uint8_t> packet,
                              const PacketOptions& options) {
  if (session_->Send(flow_id, packet) != RoqSendResult::kQueued)
    return false;
  if (packet_accepted_)
    packet_accepted_(flow_id, packet, options);
  return true;
}

}  // namespace webrtc::roq
