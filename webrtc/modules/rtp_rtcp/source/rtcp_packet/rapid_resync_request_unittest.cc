/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rapid_resync_request.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAreArray;
using testing::make_tuple;
using webrtc::rtcp::RapidResyncRequest;
using webrtc::RTCPUtility::RtcpCommonHeader;
using webrtc::RTCPUtility::RtcpParseCommonHeader;

namespace webrtc {
namespace {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;
// Manually created packet matching constants above.
const uint8_t kPacket[] = {0x85, 205,  0x00, 0x02,
                           0x12, 0x34, 0x56, 0x78,
                           0x23, 0x45, 0x67, 0x89};
const size_t kPacketLength = sizeof(kPacket);
}  // namespace

TEST(RtcpPacketRapidResyncRequestTest, Parse) {
  RtcpCommonHeader header;
  ASSERT_TRUE(RtcpParseCommonHeader(kPacket, kPacketLength, &header));
  RapidResyncRequest mutable_parsed;
  EXPECT_TRUE(mutable_parsed.Parse(
      header, kPacket + RtcpCommonHeader::kHeaderSizeBytes));
  const RapidResyncRequest& parsed = mutable_parsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, parsed.media_ssrc());
}

TEST(RtcpPacketRapidResyncRequestTest, Create) {
  RapidResyncRequest rrr;
  rrr.From(kSenderSsrc);
  rrr.To(kRemoteSsrc);

  rtc::Buffer packet = rrr.Build();

  EXPECT_THAT(make_tuple(packet.data(), packet.size()),
              ElementsAreArray(kPacket));
}

TEST(RtcpPacketRapidResyncRequestTest, ParseFailsOnWrongSizePacket) {
  RapidResyncRequest parsed;
  RtcpCommonHeader header;
  ASSERT_TRUE(RtcpParseCommonHeader(kPacket, kPacketLength, &header));
  const size_t kCorrectPayloadSize = header.payload_size_bytes;
  const uint8_t* payload = kPacket + RtcpCommonHeader::kHeaderSizeBytes;

  header.payload_size_bytes = kCorrectPayloadSize - 1;
  EXPECT_FALSE(parsed.Parse(header, payload));

  header.payload_size_bytes = kCorrectPayloadSize + 1;
  EXPECT_FALSE(parsed.Parse(header, payload));
}
}  // namespace webrtc
