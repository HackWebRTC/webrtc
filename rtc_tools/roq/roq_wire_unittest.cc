/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/roq/roq_wire.h"

#include <array>
#include <cstdint>
#include <vector>

#include "test/gtest.h"

namespace webrtc::roq {
namespace {

TEST(RoqWireTest, EncodesQuicVarIntBoundaries) {
  struct TestCase {
    uint64_t value;
    std::vector<uint8_t> bytes;
  };
  const std::array<TestCase, 8> cases = {{
      {0, {0x00}},
      {63, {0x3f}},
      {64, {0x40, 0x40}},
      {16383, {0x7f, 0xff}},
      {16384, {0x80, 0x00, 0x40, 0x00}},
      {(1ULL << 30) - 1, {0xbf, 0xff, 0xff, 0xff}},
      {1ULL << 30, {0xc0, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00}},
      {(1ULL << 62) - 1, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
  }};
  for (const TestCase& test : cases) {
    std::vector<uint8_t> encoded;
    ASSERT_TRUE(EncodeQuicVarInt(test.value, &encoded));
    EXPECT_EQ(encoded, test.bytes);
    uint64_t decoded = 0;
    size_t consumed = 0;
    ASSERT_TRUE(DecodeQuicVarInt(encoded, &decoded, &consumed));
    EXPECT_EQ(decoded, test.value);
    EXPECT_EQ(consumed, test.bytes.size());
  }
}

TEST(RoqWireTest, RejectsOutOfRangeAndTruncatedVarInts) {
  std::vector<uint8_t> encoded;
  EXPECT_FALSE(EncodeQuicVarInt(1ULL << 62, &encoded));
  uint64_t value = 0;
  size_t consumed = 0;
  EXPECT_FALSE(DecodeQuicVarInt({}, &value, &consumed));
  constexpr std::array<uint8_t, 1> kTruncatedTwoByte = {0x40};
  EXPECT_FALSE(DecodeQuicVarInt(kTruncatedTwoByte, &value, &consumed));
}

TEST(RoqWireTest, PreservesPacketBoundaryAfterFlowId) {
  constexpr std::array<uint8_t, 4> kPacket = {0x80, 0x60, 0x00, 0x01};
  const std::vector<uint8_t> encoded = EncodeRoqDatagram(64, kPacket);
  EXPECT_EQ(encoded,
            (std::vector<uint8_t>{0x40, 0x40, 0x80, 0x60, 0x00, 0x01}));

  RoqDatagram decoded;
  std::string error;
  ASSERT_TRUE(DecodeRoqDatagram(encoded, &decoded, &error));
  EXPECT_EQ(decoded.flow_id, 64u);
  EXPECT_TRUE(std::equal(decoded.packet.begin(), decoded.packet.end(),
                         kPacket.begin(), kPacket.end()));
}

TEST(RoqWireTest, RejectsDatagramWithoutRtpOrRtcpPacket) {
  constexpr std::array<uint8_t, 1> kOnlyFlowId = {0x02};
  RoqDatagram decoded;
  std::string error;
  EXPECT_FALSE(DecodeRoqDatagram(kOnlyFlowId, &decoded, &error));
  EXPECT_EQ(error, "RoQ DATAGRAM has no RTP or RTCP packet");
}

}  // namespace
}  // namespace webrtc::roq
