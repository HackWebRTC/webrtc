/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/fir.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AllOf;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Eq;
using testing::Field;
using testing::make_tuple;
using webrtc::rtcp::Fir;
using webrtc::RTCPUtility::RtcpCommonHeader;
using webrtc::RTCPUtility::RtcpParseCommonHeader;

namespace webrtc {
namespace {

const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;
const uint8_t kSeqNr = 13;
// Manually created Fir packet matching constants above.
const uint8_t kPacket[] = {0x84, 206,  0x00, 0x04,
                           0x12, 0x34, 0x56, 0x78,
                           0x00, 0x00, 0x00, 0x00,
                           0x23, 0x45, 0x67, 0x89,
                           0x0d, 0x00, 0x00, 0x00};

bool ParseFir(const uint8_t* buffer, size_t length, Fir* fir) {
  RtcpCommonHeader header;
  EXPECT_TRUE(RtcpParseCommonHeader(buffer, length, &header));
  EXPECT_THAT(header.BlockSize(), Eq(length));
  return fir->Parse(header, buffer + RtcpCommonHeader::kHeaderSizeBytes);
}

TEST(RtcpPacketFirTest, Parse) {
  Fir mutable_parsed;
  EXPECT_TRUE(ParseFir(kPacket, sizeof(kPacket), &mutable_parsed));
  const Fir& parsed = mutable_parsed;  // Read values from constant object.

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.requests(),
              ElementsAre(AllOf(Field(&Fir::Request::ssrc, Eq(kRemoteSsrc)),
                                Field(&Fir::Request::seq_nr, Eq(kSeqNr)))));
}

TEST(RtcpPacketFirTest, Create) {
  Fir fir;
  fir.From(kSenderSsrc);
  fir.WithRequestTo(kRemoteSsrc, kSeqNr);

  rtc::Buffer packet = fir.Build();

  EXPECT_THAT(make_tuple(packet.data(), packet.size()),
              ElementsAreArray(kPacket));
}

TEST(RtcpPacketFirTest, TwoFciEntries) {
  Fir fir;
  fir.From(kSenderSsrc);
  fir.WithRequestTo(kRemoteSsrc, kSeqNr);
  fir.WithRequestTo(kRemoteSsrc + 1, kSeqNr + 1);

  rtc::Buffer packet = fir.Build();
  Fir parsed;
  EXPECT_TRUE(ParseFir(packet.data(), packet.size(), &parsed));

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.requests(),
              ElementsAre(AllOf(Field(&Fir::Request::ssrc, Eq(kRemoteSsrc)),
                                Field(&Fir::Request::seq_nr, Eq(kSeqNr))),
                          AllOf(Field(&Fir::Request::ssrc, Eq(kRemoteSsrc + 1)),
                                Field(&Fir::Request::seq_nr, Eq(kSeqNr + 1)))));
}

TEST(RtcpPacketFirTest, ParseFailsOnZeroFciEntries) {
  Fir fir;
  fir.From(kSenderSsrc);
  fir.WithRequestTo(kRemoteSsrc, kSeqNr);

  rtc::Buffer packet = fir.Build();

  RtcpCommonHeader header;
  RtcpParseCommonHeader(packet.data(), packet.size(), &header);
  ASSERT_EQ(16u, header.payload_size_bytes);  // Common: 8, 1xfci: 8.
  header.payload_size_bytes = 8;              // Common: 8, 0xfcis.

  Fir parsed;
  EXPECT_FALSE(parsed.Parse(
      header, packet.data() + RtcpCommonHeader::kHeaderSizeBytes));
}

TEST(RtcpPacketFirTest, ParseFailsOnFractionalFciEntries) {
  Fir fir;
  fir.From(kSenderSsrc);
  fir.WithRequestTo(kRemoteSsrc, kSeqNr);
  fir.WithRequestTo(kRemoteSsrc + 1, kSeqNr + 1);

  rtc::Buffer packet = fir.Build();

  RtcpCommonHeader header;
  RtcpParseCommonHeader(packet.data(), packet.size(), &header);
  ASSERT_EQ(24u, header.payload_size_bytes);  // Common: 8, 2xfcis: 16.

  const uint8_t* payload = packet.data() + RtcpCommonHeader::kHeaderSizeBytes;
  Fir good;
  EXPECT_TRUE(good.Parse(header, payload));
  for (size_t i = 1; i < 8; ++i) {
    header.payload_size_bytes = 16 + i;
    Fir bad;
    EXPECT_FALSE(bad.Parse(header, payload));
  }
}
}  // namespace
}  // namespace webrtc
