/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_depacketizer_av1.h"

#include "test/gtest.h"

namespace webrtc {
namespace {
// Signals number of the OBU (fragments) in the packet.
constexpr uint8_t kObuCountAny = 0b0000'0000;
constexpr uint8_t kObuCountOne = 0b0001'0000;
constexpr uint8_t kObuCountTwo = 0b0010'0000;

constexpr uint8_t kObuHeaderSequenceHeader = 0b0'0001'000;
constexpr uint8_t kObuHeaderTemporalDelimiter = 0b0'0010'000;
constexpr uint8_t kObuHeaderFrame = 0b0'0110'000;

TEST(RtpDepacketizerAv1Test, ParsePassFullRtpPayloadAsCodecPayload) {
  const uint8_t packet[] = {(uint8_t{1} << 7) | kObuCountOne, 1, 2, 3, 4};
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_EQ(parsed.payload_length, sizeof(packet));
  EXPECT_TRUE(parsed.payload == packet);
}

TEST(RtpDepacketizerAv1Test, ParseTreatsContinuationFlagAsNotBeginningOfFrame) {
  const uint8_t packet[] = {
      (uint8_t{1} << 7) | kObuCountOne,
      kObuHeaderFrame};  // Value doesn't matter since it is a
                         // continuation of the OBU from previous packet.
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_FALSE(parsed.video.is_first_packet_in_frame);
}

TEST(RtpDepacketizerAv1Test, ParseTreatsNoContinuationFlagAsBeginningOfFrame) {
  const uint8_t packet[] = {(uint8_t{0} << 7) | kObuCountOne, kObuHeaderFrame};
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_TRUE(parsed.video.is_first_packet_in_frame);
}

TEST(RtpDepacketizerAv1Test, ParseTreatsWillContinueFlagAsNotEndOfFrame) {
  const uint8_t packet[] = {(uint8_t{1} << 6) | kObuCountOne, kObuHeaderFrame};
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_FALSE(parsed.video.is_last_packet_in_frame);
}

TEST(RtpDepacketizerAv1Test, ParseTreatsNoWillContinueFlagAsEndOfFrame) {
  const uint8_t packet[] = {(uint8_t{0} << 6) | kObuCountOne, kObuHeaderFrame};
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_TRUE(parsed.video.is_last_packet_in_frame);
}

TEST(RtpDepacketizerAv1Test, ParseTreatsStartOfSequenceHeaderAsKeyFrame) {
  const uint8_t packet[] = {kObuCountOne, kObuHeaderSequenceHeader};
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_TRUE(parsed.video.is_first_packet_in_frame);
  EXPECT_TRUE(parsed.video.frame_type == VideoFrameType::kVideoFrameKey);
}

TEST(RtpDepacketizerAv1Test, ParseTreatsNotStartOfFrameAsDeltaFrame) {
  const uint8_t packet[] = {
      (uint8_t{1} << 7) | kObuCountOne,
      // Byte that look like start of sequence header, but since it is not start
      // of an OBU, it is actually not a start of sequence header.
      kObuHeaderSequenceHeader};
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_FALSE(parsed.video.is_first_packet_in_frame);
  EXPECT_TRUE(parsed.video.frame_type == VideoFrameType::kVideoFrameDelta);
}

TEST(RtpDepacketizerAv1Test,
     ParseTreatsStartOfFrameWithoutSequenceHeaderAsDeltaFrame) {
  const uint8_t packet[] = {kObuCountOne, kObuHeaderFrame};
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_TRUE(parsed.video.is_first_packet_in_frame);
  EXPECT_TRUE(parsed.video.frame_type == VideoFrameType::kVideoFrameDelta);
}

TEST(RtpDepacketizerAv1Test, ParseFindsSequenceHeaderBehindFragmentSize1) {
  const uint8_t packet[] = {kObuCountAny,
                            1,  // size of the next fragment
                            kObuHeaderSequenceHeader};
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_TRUE(parsed.video.frame_type == VideoFrameType::kVideoFrameKey);
}

TEST(RtpDepacketizerAv1Test, ParseFindsSequenceHeaderBehindFragmentSize2) {
  const uint8_t packet[] = {kObuCountTwo,
                            2,  // size of the next fragment
                            kObuHeaderSequenceHeader,
                            42,  // SH payload.
                            kObuHeaderFrame};
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_TRUE(parsed.video.frame_type == VideoFrameType::kVideoFrameKey);
}

TEST(RtpDepacketizerAv1Test,
     ParseFindsSequenceHeaderBehindMultiByteFragmentSize) {
  const uint8_t packet[] = {kObuCountTwo,
                            0b1000'0101,  // leb128 encoded value of 5
                            0b1000'0000,  // using 3 bytes
                            0b0000'0000,  // to encode the value.
                            kObuHeaderSequenceHeader,
                            8,  // 4 bytes of SH payload.
                            0,
                            0,
                            0,
                            kObuHeaderFrame};
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_TRUE(parsed.video.frame_type == VideoFrameType::kVideoFrameKey);
}

TEST(RtpDepacketizerAv1Test, ParseFindsSequenceHeaderBehindTemporalDelimiter) {
  const uint8_t packet[] = {kObuCountTwo,
                            1,  // size of the next fragment
                            kObuHeaderTemporalDelimiter,
                            kObuHeaderSequenceHeader,
                            8,  // 4 bytes of SH payload.
                            0,
                            0,
                            0};
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_TRUE(parsed.video.frame_type == VideoFrameType::kVideoFrameKey);
}

TEST(RtpDepacketizerAv1Test,
     ParseFindsSequenceHeaderBehindTemporalDelimiterAndSize) {
  const uint8_t packet[] = {kObuCountAny,
                            1,  // size of the next fragment
                            kObuHeaderTemporalDelimiter,
                            5,  // size of the next fragment
                            kObuHeaderSequenceHeader,
                            8,  // 4 bytes of SH payload.
                            0,
                            0,
                            0,
                            1,  // size of the next fragment
                            kObuHeaderFrame};
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_TRUE(parsed.video.frame_type == VideoFrameType::kVideoFrameKey);
}

TEST(RtpDepacketizerAv1Test, ParseSkipsEmptyFragments) {
  static_assert(kObuHeaderSequenceHeader == 8, "");
  const uint8_t packet[] = {kObuCountAny,
                            0,  // size of the next fragment
                            8,  // size of the next fragment that look like SH
                            kObuHeaderFrame,
                            1,
                            2,
                            3,
                            4,
                            5,
                            6,
                            7};
  RtpDepacketizerAv1 depacketizer;
  RtpDepacketizer::ParsedPayload parsed;
  ASSERT_TRUE(depacketizer.Parse(&parsed, packet, sizeof(packet)));
  EXPECT_TRUE(parsed.video.frame_type == VideoFrameType::kVideoFrameDelta);
}

}  // namespace
}  // namespace webrtc
