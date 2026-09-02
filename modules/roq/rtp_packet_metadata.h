/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef MODULES_ROQ_RTP_PACKET_METADATA_H_
#define MODULES_ROQ_RTP_PACKET_METADATA_H_

#include <cstdint>
#include <span>

namespace webrtc::roq {

struct RtpPacketMetadata {
  uint16_t sequence_number = 0;
  uint32_t timestamp = 0;
  uint32_t ssrc = 0;
  uint8_t payload_type = 0;
  bool marker = false;
  bool contains_idr = false;
  bool contains_sps = false;
  bool contains_pps = false;
};

bool ParseH264RtpPacketMetadata(std::span<const uint8_t> packet,
                                RtpPacketMetadata* metadata);

}  // namespace webrtc::roq

#endif  // MODULES_ROQ_RTP_PACKET_METADATA_H_
