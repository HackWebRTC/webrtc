/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/reliable_message_framer.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "modules/roq/roq_wire.h"

namespace webrtc::roq {

std::vector<uint8_t> FrameReliableMessage(std::span<const uint8_t> message) {
  if (message.size() > kMaximumReliableMessageSize)
    return {};
  std::vector<uint8_t> framed;
  if (!EncodeQuicVarInt(message.size(), &framed))
    return {};
  framed.insert(framed.end(), message.begin(), message.end());
  return framed;
}

bool ReliableMessageDecoder::Push(std::span<const uint8_t> bytes) {
  buffered_.insert(buffered_.end(), bytes.begin(), bytes.end());
  while (!buffered_.empty()) {
    uint64_t length = 0;
    size_t consumed = 0;
    if (!DecodeQuicVarInt(buffered_, &length, &consumed))
      return buffered_.size() <= 8;
    if (length > kMaximumReliableMessageSize)
      return false;
    if (buffered_.size() < consumed + length)
      return true;
    messages_.emplace_back(buffered_.begin() + consumed,
                           buffered_.begin() + consumed + length);
    buffered_.erase(buffered_.begin(), buffered_.begin() + consumed + length);
  }
  return true;
}

std::vector<std::vector<uint8_t>> ReliableMessageDecoder::TakeMessages() {
  return std::exchange(messages_, {});
}

}  // namespace webrtc::roq
