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

TEST(RtpPacketMetadataTest, ParsesGenericAudioRtpHeader) {
  constexpr std::array<uint8_t, 13> packet = {0x80, 0xef, 0xff, 0xff, 0x00,
                                              0x00, 0x01, 0xe0, 0x00, 0xa9,
                                              0x8a, 0xc7, 0xaa};
  RtpPacketMetadata metadata;
  ASSERT_TRUE(ParseRtpPacketMetadata(packet, &metadata));
  EXPECT_EQ(metadata.payload_type, 111);
  EXPECT_EQ(metadata.sequence_number, 65535);
  EXPECT_EQ(metadata.timestamp, 480u);
  EXPECT_EQ(metadata.ssrc, 11111111u);
  EXPECT_TRUE(metadata.marker);
}

TEST(RtpPacketMetadataTest, ParsesRtxOriginalSequenceNumber) {
  constexpr std::array<uint8_t, 15> packet = {0x80, 0x61, 0x00, 0x2a, 0x00,
                                              0x01, 0x5f, 0x90, 0x01, 0x53,
                                              0x15, 0x90, 0xff, 0xff, 0xaa};
  uint16_t sequence_number = 0;
  ASSERT_TRUE(ParseRtxOriginalSequenceNumber(packet, &sequence_number));
  EXPECT_EQ(sequence_number, 65535);
}

TEST(RtpPacketMetadataTest, RejectsMalformedRtpAndRtx) {
  RtpPacketMetadata metadata;
  uint16_t sequence_number = 0;
  constexpr std::array<uint8_t, 2> malformed = {0x40, 0x60};
  EXPECT_FALSE(ParseRtpPacketMetadata(malformed, &metadata));
  constexpr std::array<uint8_t, 13> no_osn = {0x80, 0x61, 0x00, 0x2a, 0x00,
                                              0x01, 0x5f, 0x90, 0x01, 0x53,
                                              0x15, 0x90, 0xaa};
  EXPECT_FALSE(ParseRtxOriginalSequenceNumber(no_osn, &sequence_number));
}

TEST(RtpPacketMetadataTest, ClassifiesPureReportsAsPeriodicRtcp) {
  constexpr std::array<uint8_t, 16> compound = {
      0x80, 200, 0x00, 0x01, 0, 0, 0, 1, 0x80, 201, 0x00, 0x01, 0, 0, 0, 2};
  bool critical = true;
  ASSERT_TRUE(ClassifyRtcpPacket(compound, &critical));
  EXPECT_FALSE(critical);
}

TEST(RtpPacketMetadataTest, ClassifiesReportsWithSdesAsPeriodicRtcp) {
  constexpr std::array<uint8_t, 20> compound = {
      0x80, 200,  0x00, 0x01, 0, 0, 0, 1, 0x81, 202,
      0x00, 0x02, 0,    0,    0, 1, 0, 0, 0,    0};
  bool critical = true;
  ASSERT_TRUE(ClassifyRtcpPacket(compound, &critical));
  EXPECT_FALSE(critical);
}

TEST(RtpPacketMetadataTest, PromotesCompoundRtcpContainingFeedback) {
  constexpr std::array<uint8_t, 20> compound = {
      0x80, 201,  0x00, 0x01, 0, 0, 0, 2, 0x81, 206,
      0x00, 0x02, 0,    0,    0, 1, 0, 0, 0,    2};
  bool critical = false;
  ASSERT_TRUE(ClassifyRtcpPacket(compound, &critical));
  EXPECT_TRUE(critical);
}

TEST(RtpPacketMetadataTest, TreatsUnknownValidRtcpAsCritical) {
  constexpr std::array<uint8_t, 4> packet = {0x80, 204, 0x00, 0x00};
  bool critical = false;
  ASSERT_TRUE(ClassifyRtcpPacket(packet, &critical));
  EXPECT_TRUE(critical);
}

TEST(RtpPacketMetadataTest, RejectsMalformedCompoundRtcp) {
  constexpr std::array<uint8_t, 8> packet = {0x80, 200, 0x00, 0x02, 0, 0, 0, 1};
  bool critical = false;
  EXPECT_FALSE(ClassifyRtcpPacket(packet, &critical));
}

}  // namespace
}  // namespace webrtc::roq
