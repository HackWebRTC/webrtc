/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/remb.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAreArray;
using testing::IsEmpty;
using testing::make_tuple;
using webrtc::rtcp::Remb;
using webrtc::RTCPUtility::RtcpCommonHeader;
using webrtc::RTCPUtility::RtcpParseCommonHeader;

namespace webrtc {
namespace {

const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrcs[] = {0x23456789, 0x2345678a, 0x2345678b};
const uint32_t kBitrateBps = 0x3fb93 * 2;  // 522022;
const uint8_t kPacket[] = {0x8f, 206,  0x00, 0x07, 0x12, 0x34, 0x56, 0x78,
                           0x00, 0x00, 0x00, 0x00, 'R',  'E',  'M',  'B',
                           0x03, 0x07, 0xfb, 0x93, 0x23, 0x45, 0x67, 0x89,
                           0x23, 0x45, 0x67, 0x8a, 0x23, 0x45, 0x67, 0x8b};
const size_t kPacketLength = sizeof(kPacket);

bool ParseRemb(const uint8_t* buffer, size_t length, Remb* remb) {
  RtcpCommonHeader header;
  EXPECT_TRUE(RtcpParseCommonHeader(buffer, length, &header));
  EXPECT_EQ(length, header.BlockSize());
  return remb->Parse(header, buffer + RtcpCommonHeader::kHeaderSizeBytes);
}

TEST(RtcpPacketRembTest, Create) {
  Remb remb;
  remb.From(kSenderSsrc);
  remb.AppliesTo(kRemoteSsrcs[0]);
  remb.AppliesTo(kRemoteSsrcs[1]);
  remb.AppliesTo(kRemoteSsrcs[2]);
  remb.WithBitrateBps(kBitrateBps);

  rtc::Buffer packet = remb.Build();

  EXPECT_THAT(make_tuple(packet.data(), packet.size()),
              ElementsAreArray(kPacket));
}

TEST(RtcpPacketRembTest, Parse) {
  Remb remb;
  EXPECT_TRUE(ParseRemb(kPacket, kPacketLength, &remb));
  const Remb& parsed = remb;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kBitrateBps, parsed.bitrate_bps());
  EXPECT_THAT(parsed.ssrcs(), ElementsAreArray(kRemoteSsrcs));
}

TEST(RtcpPacketRembTest, CreateAndParseWithoutSsrcs) {
  Remb remb;
  remb.From(kSenderSsrc);
  remb.WithBitrateBps(kBitrateBps);
  rtc::Buffer packet = remb.Build();

  Remb parsed;
  EXPECT_TRUE(ParseRemb(packet.data(), packet.size(), &parsed));
  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kBitrateBps, parsed.bitrate_bps());
  EXPECT_THAT(parsed.ssrcs(), IsEmpty());
}

TEST(RtcpPacketRembTest, ParseFailsOnTooSmallPacketToBeRemb) {
  uint8_t packet[kPacketLength];
  memcpy(packet, kPacket, kPacketLength);
  packet[3] = 3;  // Make it too small.

  Remb remb;
  EXPECT_FALSE(ParseRemb(packet, (1 + 3) * 4, &remb));
}

TEST(RtcpPacketRembTest, ParseFailsWhenUniqueIdentifierIsNotRemb) {
  uint8_t packet[kPacketLength];
  memcpy(packet, kPacket, kPacketLength);
  packet[12] = 'N';  // Swap 'R' -> 'N' in the 'REMB' unique identifier.

  Remb remb;
  EXPECT_FALSE(ParseRemb(packet, kPacketLength, &remb));
}

TEST(RtcpPacketRembTest, ParseFailsWhenSsrcCountMismatchLength) {
  uint8_t packet[kPacketLength];
  memcpy(packet, kPacket, kPacketLength);
  packet[16]++;  // Swap 3 -> 4 in the ssrcs count.

  Remb remb;
  EXPECT_FALSE(ParseRemb(packet, kPacketLength, &remb));
}

TEST(RtcpPacketRembTest, TooManySsrcs) {
  const size_t kMax = 0xff;
  Remb remb;
  for (size_t i = 1; i <= kMax; ++i)
    EXPECT_TRUE(remb.AppliesTo(kRemoteSsrcs[0] + i));
  EXPECT_FALSE(remb.AppliesTo(kRemoteSsrcs[0]));
}

TEST(RtcpPacketRembTest, TooManySsrcsForBatchAssign) {
  const uint32_t kRemoteSsrc = kRemoteSsrcs[0];
  const size_t kMax = 0xff;
  const std::vector<uint32_t> kAllButOneSsrc(kMax - 1, kRemoteSsrc);
  const std::vector<uint32_t> kTwoSsrcs(2, kRemoteSsrc);

  Remb remb;
  EXPECT_TRUE(remb.AppliesToMany(kAllButOneSsrc));
  // Should be no place for 2 more.
  EXPECT_FALSE(remb.AppliesToMany(kTwoSsrcs));
  // But enough place for 1 more.
  EXPECT_TRUE(remb.AppliesTo(kRemoteSsrc));
  // But not for another one.
  EXPECT_FALSE(remb.AppliesTo(kRemoteSsrc));
}
}  // namespace
}  // namespace webrtc
