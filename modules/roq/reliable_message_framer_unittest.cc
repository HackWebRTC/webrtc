/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/reliable_message_framer.h"

#include <cstdint>
#include <vector>

#include "test/gtest.h"

namespace webrtc::roq {
namespace {

TEST(ReliableMessageFramerTest, DecodesFragmentedAndConsecutiveMessages) {
  const std::vector<uint8_t> first = {1, 2, 3};
  const std::vector<uint8_t> second(100, 4);
  std::vector<uint8_t> wire = FrameReliableMessage(first);
  const std::vector<uint8_t> second_wire = FrameReliableMessage(second);
  wire.insert(wire.end(), second_wire.begin(), second_wire.end());
  ReliableMessageDecoder decoder;

  EXPECT_TRUE(decoder.Push(std::span<const uint8_t>(wire).first(2)));
  EXPECT_TRUE(decoder.TakeMessages().empty());
  EXPECT_TRUE(decoder.Push(std::span<const uint8_t>(wire).subspan(2)));

  const std::vector<std::vector<uint8_t>> messages = decoder.TakeMessages();
  ASSERT_EQ(messages.size(), 2u);
  EXPECT_EQ(messages[0], first);
  EXPECT_EQ(messages[1], second);
}

TEST(ReliableMessageFramerTest, RejectsMessageLargerThan64KiB) {
  EXPECT_TRUE(FrameReliableMessage(std::vector<uint8_t>(65536)).empty());
  EXPECT_FALSE(FrameReliableMessage(std::vector<uint8_t>(65535)).empty());
}

}  // namespace
}  // namespace webrtc::roq
