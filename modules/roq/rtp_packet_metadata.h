/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef MODULES_ROQ_RTP_PACKET_METADATA_H_
#define MODULES_ROQ_RTP_PACKET_METADATA_H_

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "api/video/video_codec_type.h"

namespace webrtc::roq {

struct RtpPacketMetadata {
  uint16_t sequence_number = 0;
  uint32_t timestamp = 0;
  uint32_t ssrc = 0;
  uint8_t payload_type = 0;
  bool marker = false;
  bool contains_vps = false;
  bool is_key = false;
  bool contains_sps = false;
  bool contains_pps = false;
};

bool ParseH264RtpPacketMetadata(std::span<const uint8_t> packet,
                                RtpPacketMetadata* metadata);
bool ParseVideoRtpPacketMetadata(std::span<const uint8_t> packet,
                                 VideoCodecType codec,
                                 RtpPacketMetadata* metadata);
bool ParseRtpPacketMetadata(std::span<const uint8_t> packet,
                            RtpPacketMetadata* metadata);
bool ParseRtxOriginalSequenceNumber(std::span<const uint8_t> packet,
                                    uint16_t* sequence_number);
// Returns the primary codec payload length, excluding RTP and RED redundancy.
size_t PrimaryPayloadSize(std::span<const uint8_t> packet, bool red);
struct RtpPayloadCost {
  size_t primary_bytes = 0;
  size_t redundant_bytes = 0;
  size_t redundant_blocks = 0;
};
std::optional<RtpPayloadCost> ParseRtpPayloadCost(
    std::span<const uint8_t> packet,
    bool red);
// Exact report coverage + exact SDES. Empty means unsafe to coalesce.
std::vector<uint8_t> PeriodicRtcpKey(std::span<const uint8_t> packet);
bool ClassifyRtcpPacket(std::span<const uint8_t> packet, bool* critical);

}  // namespace webrtc::roq

#endif  // MODULES_ROQ_RTP_PACKET_METADATA_H_
