/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/roq_transport.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "api/units/time_delta.h"
#include "modules/roq/rtp_packet_metadata.h"

namespace webrtc::roq {

RoqPacketTransport::RoqPacketTransport(RoqPacketTransportConfig config,
                                       PacketAcceptedCallback packet_accepted)
    : packet_accepted_(std::move(packet_accepted)),
      config_(std::move(config)) {}

bool RoqPacketTransport::SendRtp(std::span<const uint8_t> packet,
                                 const PacketOptions& options) {
  return Send(config_.rtp_flow_id, packet, options);
}

bool RoqPacketTransport::SendRtcp(std::span<const uint8_t> packet,
                                  const PacketOptions& options) {
  return Send(config_.rtcp_flow_id, packet, options);
}

bool RoqPacketTransport::Send(uint64_t flow_id,
                              std::span<const uint8_t> packet,
                              const PacketOptions& options) {
  const Timestamp now = config_.clock->CurrentTime();
  RoqDatagramMetadata datagram = {.media_type = config_.media_type,
                                  .enqueue_time = now};
  if (flow_id == config_.rtcp_flow_id) {
    bool critical = false;
    if (!ClassifyRtcpPacket(packet, &critical)) {
      config_.session->RecordInvalidDatagram();
      return false;
    }
    datagram.media_type = RoqMediaType::kRtcp;
    datagram.priority = critical ? RoqDatagramPriority::kRtcp
                                 : RoqDatagramPriority::kPeriodicRtcp;
    datagram.deadline = now + (critical ? config_.critical_rtcp_deadline
                                        : config_.periodic_rtcp_deadline);
  } else {
    RtpPacketMetadata metadata;
    if (config_.media_type == RoqMediaType::kAudio) {
      if (!ParseRtpPacketMetadata(packet, &metadata) ||
          metadata.payload_type != config_.primary_payload_type ||
          metadata.ssrc != config_.primary_ssrc) {
        config_.session->RecordInvalidDatagram();
        return false;
      }
      datagram.priority = RoqDatagramPriority::kAudio;
      datagram.deadline = now + config_.audio_deadline;
      datagram.packet_id = RoqPacketId{metadata.ssrc, metadata.sequence_number};
    } else if (ParseH264RtpPacketMetadata(packet, &metadata) &&
               metadata.payload_type == config_.primary_payload_type &&
               metadata.ssrc == config_.primary_ssrc) {
      const bool key = metadata.contains_idr || metadata.contains_sps ||
                       metadata.contains_pps;
      datagram.priority = key ? RoqDatagramPriority::kVideoKey
                              : RoqDatagramPriority::kVideoDelta;
      datagram.deadline = now + (key ? config_.video_key_deadline
                                     : config_.video_delta_deadline);
      datagram.frame_id = RoqFrameId{metadata.ssrc, metadata.timestamp};
      datagram.packet_id = RoqPacketId{metadata.ssrc, metadata.sequence_number};
    } else if (ParseRtpPacketMetadata(packet, &metadata) &&
               metadata.payload_type == config_.rtx_payload_type &&
               metadata.ssrc == config_.rtx_ssrc) {
      uint16_t original_sequence_number = 0;
      if (!ParseRtxOriginalSequenceNumber(packet, &original_sequence_number)) {
        config_.session->RecordInvalidDatagram();
        return false;
      }
      datagram.original_packet_id =
          RoqPacketId{config_.primary_ssrc, original_sequence_number};
    } else {
      config_.session->RecordInvalidDatagram();
      return false;
    }
  }
  PacketAcceptedCallback accepted = packet_accepted_;
  std::vector<uint8_t> accepted_packet(packet.begin(), packet.end());
  if (config_.session->SendMedia(
          flow_id, packet, std::move(datagram),
          [accepted = std::move(accepted), flow_id,
           packet = std::move(accepted_packet), options] {
            if (accepted)
              accepted(flow_id, packet, options);
          }) != RoqSendResult::kQueued) {
    return false;
  }
  return true;
}

}  // namespace webrtc::roq
