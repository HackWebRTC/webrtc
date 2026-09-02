/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef MODULES_ROQ_ROQ_WIRE_H_
#define MODULES_ROQ_ROQ_WIRE_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace webrtc::roq {

enum class RoqError : uint64_t {
  kNoError = 0x00,
  kGeneralError = 0x01,
  kInternalError = 0x02,
  kPacketError = 0x03,
  kStreamCreationError = 0x04,
  kFrameCancelled = 0x05,
  kUnknownFlowId = 0x06,
  kExpectationUnmet = 0x07,
};

struct RoqDatagram {
  uint64_t flow_id = 0;
  std::span<const uint8_t> packet;
};

bool EncodeQuicVarInt(uint64_t value, std::vector<uint8_t>* output);
bool DecodeQuicVarInt(std::span<const uint8_t> input,
                      uint64_t* value,
                      size_t* consumed);
std::vector<uint8_t> EncodeRoqDatagram(uint64_t flow_id,
                                       std::span<const uint8_t> packet);
bool DecodeRoqDatagram(std::span<const uint8_t> payload,
                       RoqDatagram* datagram,
                       std::string* error);

}  // namespace webrtc::roq

#endif  // MODULES_ROQ_ROQ_WIRE_H_
