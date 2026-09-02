/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/rtp_packet_metadata.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace webrtc::roq {
namespace {

uint16_t ReadU16(std::span<const uint8_t> bytes, size_t offset) {
  return static_cast<uint16_t>((bytes[offset] << 8) | bytes[offset + 1]);
}

uint32_t ReadU32(std::span<const uint8_t> bytes, size_t offset) {
  return (static_cast<uint32_t>(bytes[offset]) << 24) |
         (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
         (static_cast<uint32_t>(bytes[offset + 2]) << 8) | bytes[offset + 3];
}

void RecordNaluType(uint8_t nalu_type, RtpPacketMetadata* metadata) {
  metadata->contains_idr |= nalu_type == 5;
  metadata->contains_sps |= nalu_type == 7;
  metadata->contains_pps |= nalu_type == 8;
}

}  // namespace

bool ParseH264RtpPacketMetadata(std::span<const uint8_t> packet,
                                RtpPacketMetadata* metadata) {
  if (!metadata || packet.size() < 13 || (packet[0] >> 6) != 2)
    return false;
  size_t payload_offset = 12 + static_cast<size_t>(packet[0] & 0x0f) * 4;
  if (payload_offset >= packet.size())
    return false;
  if ((packet[0] & 0x10) != 0) {
    if (payload_offset + 4 > packet.size())
      return false;
    const size_t extension_size =
        4 + static_cast<size_t>(ReadU16(packet, payload_offset + 2)) * 4;
    if (payload_offset + extension_size >= packet.size())
      return false;
    payload_offset += extension_size;
  }
  size_t payload_end = packet.size();
  if ((packet[0] & 0x20) != 0) {
    const size_t padding = packet.back();
    if (padding == 0 || padding > payload_end - payload_offset)
      return false;
    payload_end -= padding;
  }
  if (payload_offset >= payload_end)
    return false;

  *metadata = {};
  metadata->marker = (packet[1] & 0x80) != 0;
  metadata->payload_type = packet[1] & 0x7f;
  metadata->sequence_number = ReadU16(packet, 2);
  metadata->timestamp = ReadU32(packet, 4);
  metadata->ssrc = ReadU32(packet, 8);

  const uint8_t type = packet[payload_offset] & 0x1f;
  if (type >= 1 && type <= 23) {
    RecordNaluType(type, metadata);
    return true;
  }
  if (type == 24) {
    size_t offset = payload_offset + 1;
    while (offset < payload_end) {
      if (offset + 2 > payload_end)
        return false;
      const size_t length = ReadU16(packet, offset);
      offset += 2;
      if (length == 0 || offset + length > payload_end)
        return false;
      RecordNaluType(packet[offset] & 0x1f, metadata);
      offset += length;
    }
    return true;
  }
  if (type == 28) {
    if (payload_offset + 2 > payload_end)
      return false;
    RecordNaluType(packet[payload_offset + 1] & 0x1f, metadata);
    return true;
  }
  return true;
}

}  // namespace webrtc::roq
