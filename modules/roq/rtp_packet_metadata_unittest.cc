/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/rtp_packet_metadata.h"

#include <array>
#include <cstdint>

#include "test/gtest.h"

namespace webrtc::roq {
namespace {

TEST(RtpPacketMetadataTest, ParsesSingleNalIdrAndRtpHeader) {
  constexpr std::array<uint8_t, 14> packet = {0x80, 0xe0, 0x00, 0x2a, 0x00,
                                              0x01, 0x5f, 0x90, 0x01, 0x53,
                                              0x15, 0x8e, 0x65, 0xaa};
  RtpPacketMetadata metadata;
  ASSERT_TRUE(ParseH264RtpPacketMetadata(packet, &metadata));
  EXPECT_EQ(metadata.sequence_number, 42);
  EXPECT_EQ(metadata.timestamp, 90000u);
  EXPECT_TRUE(metadata.marker);
  EXPECT_TRUE(metadata.contains_idr);
  EXPECT_FALSE(metadata.contains_sps);
}

TEST(RtpPacketMetadataTest, ParsesFuAStartForIdr) {
  constexpr std::array<uint8_t, 15> packet = {0x80, 0x60, 0x00, 0x2b, 0x00,
                                              0x01, 0x5f, 0x90, 0x01, 0x53,
                                              0x15, 0x8e, 0x7c, 0x85, 0xbb};
  RtpPacketMetadata metadata;
  ASSERT_TRUE(ParseH264RtpPacketMetadata(packet, &metadata));
  EXPECT_TRUE(metadata.contains_idr);
}

TEST(RtpPacketMetadataTest, ParsesStapAParameterSets) {
  constexpr std::array<uint8_t, 21> packet = {
      0x80, 0x60, 0x00, 0x2c, 0x00, 0x01, 0x5f, 0x90, 0x01, 0x53, 0x15,
      0x8e, 0x78, 0x00, 0x02, 0x67, 0x01, 0x00, 0x02, 0x68, 0x02};
  RtpPacketMetadata metadata;
  ASSERT_TRUE(ParseH264RtpPacketMetadata(packet, &metadata));
  EXPECT_TRUE(metadata.contains_sps);
  EXPECT_TRUE(metadata.contains_pps);
  EXPECT_FALSE(metadata.contains_idr);
}

TEST(RtpPacketMetadataTest, RejectsTruncatedStapA) {
  constexpr std::array<uint8_t, 15> packet = {0x80, 0x60, 0x00, 0x2d, 0x00,
                                              0x01, 0x5f, 0x90, 0x01, 0x53,
                                              0x15, 0x8e, 0x78, 0x00, 0x05};
  RtpPacketMetadata metadata;
  EXPECT_FALSE(ParseH264RtpPacketMetadata(packet, &metadata));
}

}  // namespace
}  // namespace webrtc::roq
