/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/rtp_packet_metadata.h"

#include <cstddef>
#include <cstdint>
#include <span>

#include "common_video/h265/h265_common.h"
#include "modules/rtp_rtcp/source/create_video_rtp_depacketizer.h"
#include "rtc_base/copy_on_write_buffer.h"

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
  metadata->is_key |= nalu_type == 5;
  metadata->contains_sps |= nalu_type == 7;
  metadata->contains_pps |= nalu_type == 8;
}

bool ParseRtpHeader(std::span<const uint8_t> packet,
                    RtpPacketMetadata* metadata,
                    size_t* payload_offset,
                    size_t* payload_end) {
  if (!metadata || !payload_offset || !payload_end || packet.size() < 12 ||
      (packet[0] >> 6) != 2) {
    return false;
  }
  *payload_offset = 12 + static_cast<size_t>(packet[0] & 0x0f) * 4;
  if (*payload_offset > packet.size())
    return false;
  if ((packet[0] & 0x10) != 0) {
    if (*payload_offset + 4 > packet.size())
      return false;
    const size_t extension_size =
        4 + static_cast<size_t>(ReadU16(packet, *payload_offset + 2)) * 4;
    if (*payload_offset + extension_size > packet.size())
      return false;
    *payload_offset += extension_size;
  }
  *payload_end = packet.size();
  if ((packet[0] & 0x20) != 0) {
    const size_t padding = packet.back();
    if (padding == 0 || padding > *payload_end - *payload_offset)
      return false;
    *payload_end -= padding;
  }
  *metadata = {};
  metadata->marker = (packet[1] & 0x80) != 0;
  metadata->payload_type = packet[1] & 0x7f;
  metadata->sequence_number = ReadU16(packet, 2);
  metadata->timestamp = ReadU32(packet, 4);
  metadata->ssrc = ReadU32(packet, 8);
  return true;
}

}  // namespace

bool ParseH264RtpPacketMetadata(std::span<const uint8_t> packet,
                                RtpPacketMetadata* metadata) {
  size_t payload_offset = 0;
  size_t payload_end = 0;
  if (!ParseRtpHeader(packet, metadata, &payload_offset, &payload_end) ||
      payload_offset >= payload_end) {
    return false;
  }

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

bool ParseVideoRtpPacketMetadata(std::span<const uint8_t> packet,
                                 VideoCodecType codec,
                                 RtpPacketMetadata* metadata) {
  if (codec == kVideoCodecH264)
    return ParseH264RtpPacketMetadata(packet, metadata);
#if defined(RTC_ENABLE_H265)
  if (codec == kVideoCodecH265) {
    size_t payload_offset = 0;
    size_t payload_end = 0;
    if (!ParseRtpHeader(packet, metadata, &payload_offset, &payload_end) ||
        payload_offset >= payload_end) {
      return false;
    }
    std::unique_ptr<VideoRtpDepacketizer> depacketizer =
        CreateVideoRtpDepacketizer(kVideoCodecH265);
    std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
        depacketizer->Parse(
            CopyOnWriteBuffer(packet.subspan(payload_offset,
                                              payload_end - payload_offset)));
    if (!parsed)
      return false;
    metadata->is_key =
        parsed->video_header.frame_type == VideoFrameType::kVideoFrameKey;
    for (const H265::NaluIndex& index :
         H265::FindNaluIndices(parsed->video_payload)) {
      const H265::NaluType type = H265::ParseNaluType(
          parsed->video_payload[index.payload_start_offset]);
      metadata->contains_vps |= type == H265::NaluType::kVps;
      metadata->contains_sps |= type == H265::NaluType::kSps;
      metadata->contains_pps |= type == H265::NaluType::kPps;
    }
    return true;
  }
#endif
  return false;
}

bool ParseRtpPacketMetadata(std::span<const uint8_t> packet,
                            RtpPacketMetadata* metadata) {
  size_t payload_offset = 0;
  size_t payload_end = 0;
  return ParseRtpHeader(packet, metadata, &payload_offset, &payload_end) &&
         payload_offset < payload_end;
}

bool ParseRtxOriginalSequenceNumber(std::span<const uint8_t> packet,
                                    uint16_t* sequence_number) {
  RtpPacketMetadata metadata;
  size_t payload_offset = 0;
  size_t payload_end = 0;
  if (!sequence_number ||
      !ParseRtpHeader(packet, &metadata, &payload_offset, &payload_end) ||
      payload_end - payload_offset < 2) {
    return false;
  }
  *sequence_number = ReadU16(packet, payload_offset);
  return true;
}

bool ClassifyRtcpPacket(std::span<const uint8_t> packet, bool* critical) {
  if (!critical || packet.empty())
    return false;
  *critical = false;
  while (!packet.empty()) {
    if (packet.size() < 4 || (packet[0] >> 6) != 2)
      return false;
    const size_t block_size = (static_cast<size_t>(ReadU16(packet, 2)) + 1) * 4;
    if (block_size > packet.size())
      return false;
    const uint8_t packet_type = packet[1];
    if (packet_type != 200 && packet_type != 201 && packet_type != 202)
      *critical = true;
    packet = packet.subspan(block_size);
  }
  return true;
}

}  // namespace webrtc::roq
