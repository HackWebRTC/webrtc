/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/roq/roq_wire.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace webrtc::roq {
namespace {

size_t VarIntLength(uint64_t value) {
  if (value < (1ULL << 6)) {
    return 1;
  }
  if (value < (1ULL << 14)) {
    return 2;
  }
  if (value < (1ULL << 30)) {
    return 4;
  }
  if (value < (1ULL << 62)) {
    return 8;
  }
  return 0;
}

}  // namespace

bool EncodeQuicVarInt(uint64_t value, std::vector<uint8_t>* output) {
  const size_t length = VarIntLength(value);
  if (length == 0) {
    return false;
  }
  const size_t offset = output->size();
  output->resize(offset + length);
  for (size_t i = 0; i < length; ++i) {
    (*output)[offset + length - i - 1] = static_cast<uint8_t>(value & 0xff);
    value >>= 8;
  }
  (*output)[offset] |= static_cast<uint8_t>((length == 1   ? 0
                                             : length == 2 ? 1
                                             : length == 4 ? 2
                                                           : 3)
                                            << 6);
  return true;
}

bool DecodeQuicVarInt(std::span<const uint8_t> input,
                      uint64_t* value,
                      size_t* consumed) {
  if (input.empty()) {
    return false;
  }
  const size_t length = size_t{1} << (input.front() >> 6);
  if (input.size() < length) {
    return false;
  }
  uint64_t decoded = input.front() & 0x3f;
  for (size_t i = 1; i < length; ++i) {
    decoded = (decoded << 8) | input[i];
  }
  *value = decoded;
  *consumed = length;
  return true;
}

std::vector<uint8_t> EncodeRoqDatagram(uint64_t flow_id,
                                       std::span<const uint8_t> packet) {
  std::vector<uint8_t> output;
  output.reserve(8 + packet.size());
  if (!EncodeQuicVarInt(flow_id, &output)) {
    return {};
  }
  output.insert(output.end(), packet.begin(), packet.end());
  return output;
}

bool DecodeRoqDatagram(std::span<const uint8_t> payload,
                       RoqDatagram* datagram,
                       std::string* error) {
  size_t consumed = 0;
  if (!DecodeQuicVarInt(payload, &datagram->flow_id, &consumed)) {
    *error = "invalid RoQ flow ID varint";
    return false;
  }
  if (payload.size() == consumed) {
    *error = "RoQ DATAGRAM has no RTP or RTCP packet";
    return false;
  }
  datagram->packet = payload.subspan(consumed);
  return true;
}

}  // namespace webrtc::roq
