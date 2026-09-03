/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef MODULES_ROQ_RELIABLE_MESSAGE_FRAMER_H_
#define MODULES_ROQ_RELIABLE_MESSAGE_FRAMER_H_

#include <cstdint>
#include <span>
#include <vector>

namespace webrtc::roq {

constexpr size_t kMaximumReliableMessageSize = 65535;

std::vector<uint8_t> FrameReliableMessage(std::span<const uint8_t> message);

class ReliableMessageDecoder {
 public:
  bool Push(std::span<const uint8_t> bytes);
  std::vector<std::vector<uint8_t>> TakeMessages();

 private:
  std::vector<uint8_t> buffered_;
  std::vector<std::vector<uint8_t>> messages_;
};

}  // namespace webrtc::roq

#endif  // MODULES_ROQ_RELIABLE_MESSAGE_FRAMER_H_
