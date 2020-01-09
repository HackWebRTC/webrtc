/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/video_rtp_depacketizer_h264.h"

#include <memory>
#include <vector>

#include "api/array_view.h"
#include "common_video/h264/h264_common.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;

enum Nalu {
  kSlice = 1,
  kIdr = 5,
  kSei = 6,
  kSps = 7,
  kPps = 8,
  kStapA = 24,
  kFuA = 28
};

// Bit masks for FU (A and B) indicators.
enum NalDefs { kFBit = 0x80, kNriMask = 0x60, kTypeMask = 0x1F };

// Bit masks for FU (A and B) headers.
enum FuDefs { kSBit = 0x80, kEBit = 0x40, kRBit = 0x20 };

const uint8_t kOriginalSps[] = {kSps, 0x00, 0x00, 0x03, 0x03,
                                0xF4, 0x05, 0x03, 0xC7, 0xC0};
const uint8_t kRewrittenSps[] = {kSps, 0x00, 0x00, 0x03, 0x03, 0xF4, 0x05, 0x03,
                                 0xC7, 0xE0, 0x1B, 0x41, 0x10, 0x8D, 0x00};
const uint8_t kIdrOne[] = {kIdr, 0xFF, 0x00, 0x00, 0x04};
const uint8_t kIdrTwo[] = {kIdr, 0xFF, 0x00, 0x11};

struct H264ParsedPayload : public RtpDepacketizer::ParsedPayload {
  RTPVideoHeaderH264& h264() {
    return absl::get<RTPVideoHeaderH264>(video.video_type_header);
  }
};

class RtpDepacketizerH264Test : public ::testing::Test {
 protected:
  RtpDepacketizerH264Test()
      : depacketizer_(std::make_unique<RtpDepacketizerH264>()) {}

  void ExpectPacket(H264ParsedPayload* parsed_payload,
                    const uint8_t* data,
                    size_t length) {
    ASSERT_TRUE(parsed_payload != NULL);
    EXPECT_THAT(std::vector<uint8_t>(
                    parsed_payload->payload,
                    parsed_payload->payload + parsed_payload->payload_length),
                ::testing::ElementsAreArray(data, length));
  }

  std::unique_ptr<RtpDepacketizer> depacketizer_;
};

TEST_F(RtpDepacketizerH264Test, TestSingleNalu) {
  uint8_t packet[2] = {0x05, 0xFF};  // F=0, NRI=0, Type=5 (IDR).
  H264ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(VideoFrameType::kVideoFrameKey, payload.video_header().frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_TRUE(payload.video_header().is_first_packet_in_frame);
  EXPECT_EQ(kH264SingleNalu, payload.h264().packetization_type);
  EXPECT_EQ(kIdr, payload.h264().nalu_type);
}

TEST_F(RtpDepacketizerH264Test, TestSingleNaluSpsWithResolution) {
  uint8_t packet[] = {kSps, 0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40, 0x50,
                      0x05, 0xBA, 0x10, 0x00, 0x00, 0x03, 0x00, 0xC0,
                      0x00, 0x00, 0x03, 0x2A, 0xE0, 0xF1, 0x83, 0x25};
  H264ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(VideoFrameType::kVideoFrameKey, payload.video_header().frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_TRUE(payload.video_header().is_first_packet_in_frame);
  EXPECT_EQ(kH264SingleNalu, payload.h264().packetization_type);
  EXPECT_EQ(1280u, payload.video_header().width);
  EXPECT_EQ(720u, payload.video_header().height);
}

TEST_F(RtpDepacketizerH264Test, TestStapAKey) {
  // clang-format off
  const NaluInfo kExpectedNalus[] = { {H264::kSps, 0, -1},
                                      {H264::kPps, 1, 2},
                                      {H264::kIdr, -1, 0} };
  uint8_t packet[] = {kStapA,  // F=0, NRI=0, Type=24.
                      // Length, nal header, payload.
                      0, 0x18, kExpectedNalus[0].type,
                        0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40, 0x50, 0x05, 0xBA,
                        0x10, 0x00, 0x00, 0x03, 0x00, 0xC0, 0x00, 0x00, 0x03,
                        0x2A, 0xE0, 0xF1, 0x83, 0x25,
                      0, 0xD, kExpectedNalus[1].type,
                        0x69, 0xFC, 0x0, 0x0, 0x3, 0x0, 0x7, 0xFF, 0xFF, 0xFF,
                        0xF6, 0x40,
                      0, 0xB, kExpectedNalus[2].type,
                        0x85, 0xB8, 0x0, 0x4, 0x0, 0x0, 0x13, 0x93, 0x12, 0x0};
  // clang-format on

  H264ParsedPayload payload;
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(VideoFrameType::kVideoFrameKey, payload.video_header().frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_TRUE(payload.video_header().is_first_packet_in_frame);
  const RTPVideoHeaderH264& h264 = payload.h264();
  EXPECT_EQ(kH264StapA, h264.packetization_type);
  // NALU type for aggregated packets is the type of the first packet only.
  EXPECT_EQ(kSps, h264.nalu_type);
  ASSERT_EQ(3u, h264.nalus_length);
  for (size_t i = 0; i < h264.nalus_length; ++i) {
    EXPECT_EQ(kExpectedNalus[i].type, h264.nalus[i].type)
        << "Failed parsing nalu " << i;
    EXPECT_EQ(kExpectedNalus[i].sps_id, h264.nalus[i].sps_id)
        << "Failed parsing nalu " << i;
    EXPECT_EQ(kExpectedNalus[i].pps_id, h264.nalus[i].pps_id)
        << "Failed parsing nalu " << i;
  }
}

TEST_F(RtpDepacketizerH264Test, TestStapANaluSpsWithResolution) {
  uint8_t packet[] = {kStapA,  // F=0, NRI=0, Type=24.
                               // Length (2 bytes), nal header, payload.
                      0x00, 0x19, kSps, 0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40,
                      0x50, 0x05, 0xBA, 0x10, 0x00, 0x00, 0x03, 0x00, 0xC0,
                      0x00, 0x00, 0x03, 0x2A, 0xE0, 0xF1, 0x83, 0x25, 0x80,
                      0x00, 0x03, kIdr, 0xFF, 0x00, 0x00, 0x04, kIdr, 0xFF,
                      0x00, 0x11};

  H264ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(VideoFrameType::kVideoFrameKey, payload.video_header().frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_TRUE(payload.video_header().is_first_packet_in_frame);
  EXPECT_EQ(kH264StapA, payload.h264().packetization_type);
  EXPECT_EQ(1280u, payload.video_header().width);
  EXPECT_EQ(720u, payload.video_header().height);
}

TEST_F(RtpDepacketizerH264Test, TestEmptyStapARejected) {
  uint8_t lone_empty_packet[] = {kStapA, 0x00, 0x00};

  uint8_t leading_empty_packet[] = {kStapA, 0x00, 0x00, 0x00, 0x04,
                                    kIdr,   0xFF, 0x00, 0x11};

  uint8_t middle_empty_packet[] = {kStapA, 0x00, 0x03, kIdr, 0xFF, 0x00, 0x00,
                                   0x00,   0x00, 0x04, kIdr, 0xFF, 0x00, 0x11};

  uint8_t trailing_empty_packet[] = {kStapA, 0x00, 0x03, kIdr,
                                     0xFF,   0x00, 0x00, 0x00};

  H264ParsedPayload payload;

  EXPECT_FALSE(depacketizer_->Parse(&payload, lone_empty_packet,
                                    sizeof(lone_empty_packet)));
  EXPECT_FALSE(depacketizer_->Parse(&payload, leading_empty_packet,
                                    sizeof(leading_empty_packet)));
  EXPECT_FALSE(depacketizer_->Parse(&payload, middle_empty_packet,
                                    sizeof(middle_empty_packet)));
  EXPECT_FALSE(depacketizer_->Parse(&payload, trailing_empty_packet,
                                    sizeof(trailing_empty_packet)));
}

TEST_F(RtpDepacketizerH264Test, DepacketizeWithRewriting) {
  rtc::Buffer in_buffer;
  rtc::Buffer out_buffer;

  uint8_t kHeader[2] = {kStapA};
  in_buffer.AppendData(kHeader, 1);
  out_buffer.AppendData(kHeader, 1);

  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kOriginalSps));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kOriginalSps);
  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kRewrittenSps));
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kRewrittenSps);

  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kIdrOne));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kIdrOne);
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kIdrOne);

  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kIdrTwo));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kIdrTwo);
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kIdrTwo);

  H264ParsedPayload payload;
  EXPECT_TRUE(
      depacketizer_->Parse(&payload, in_buffer.data(), in_buffer.size()));

  std::vector<uint8_t> expected_packet_payload(
      out_buffer.data(), &out_buffer.data()[out_buffer.size()]);

  EXPECT_THAT(
      expected_packet_payload,
      ::testing::ElementsAreArray(payload.payload, payload.payload_length));
}

TEST_F(RtpDepacketizerH264Test, DepacketizeWithDoubleRewriting) {
  rtc::Buffer in_buffer;
  rtc::Buffer out_buffer;

  uint8_t kHeader[2] = {kStapA};
  in_buffer.AppendData(kHeader, 1);
  out_buffer.AppendData(kHeader, 1);

  // First SPS will be kept...
  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kOriginalSps));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kOriginalSps);
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kOriginalSps);

  // ...only the second one will be rewritten.
  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kOriginalSps));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kOriginalSps);
  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kRewrittenSps));
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kRewrittenSps);

  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kIdrOne));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kIdrOne);
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kIdrOne);

  ByteWriter<uint16_t>::WriteBigEndian(kHeader, sizeof(kIdrTwo));
  in_buffer.AppendData(kHeader, 2);
  in_buffer.AppendData(kIdrTwo);
  out_buffer.AppendData(kHeader, 2);
  out_buffer.AppendData(kIdrTwo);

  H264ParsedPayload payload;
  EXPECT_TRUE(
      depacketizer_->Parse(&payload, in_buffer.data(), in_buffer.size()));

  std::vector<uint8_t> expected_packet_payload(
      out_buffer.data(), &out_buffer.data()[out_buffer.size()]);

  EXPECT_THAT(
      expected_packet_payload,
      ::testing::ElementsAreArray(payload.payload, payload.payload_length));
}

TEST_F(RtpDepacketizerH264Test, TestStapADelta) {
  uint8_t packet[16] = {kStapA,  // F=0, NRI=0, Type=24.
                                 // Length, nal header, payload.
                        0, 0x02, kSlice, 0xFF, 0, 0x03, kSlice, 0xFF, 0x00, 0,
                        0x04, kSlice, 0xFF, 0x00, 0x11};
  H264ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(VideoFrameType::kVideoFrameDelta,
            payload.video_header().frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_TRUE(payload.video_header().is_first_packet_in_frame);
  EXPECT_EQ(kH264StapA, payload.h264().packetization_type);
  // NALU type for aggregated packets is the type of the first packet only.
  EXPECT_EQ(kSlice, payload.h264().nalu_type);
}

TEST_F(RtpDepacketizerH264Test, TestFuA) {
  // clang-format off
  uint8_t packet1[] = {
      kFuA,          // F=0, NRI=0, Type=28.
      kSBit | kIdr,  // FU header.
      0x85, 0xB8, 0x0, 0x4, 0x0, 0x0, 0x13, 0x93, 0x12, 0x0  // Payload.
  };
  // clang-format on
  const uint8_t kExpected1[] = {kIdr, 0x85, 0xB8, 0x0,  0x4, 0x0,
                                0x0,  0x13, 0x93, 0x12, 0x0};

  uint8_t packet2[] = {
      kFuA,  // F=0, NRI=0, Type=28.
      kIdr,  // FU header.
      0x02   // Payload.
  };
  const uint8_t kExpected2[] = {0x02};

  uint8_t packet3[] = {
      kFuA,          // F=0, NRI=0, Type=28.
      kEBit | kIdr,  // FU header.
      0x03           // Payload.
  };
  const uint8_t kExpected3[] = {0x03};

  H264ParsedPayload payload;

  // We expect that the first packet is one byte shorter since the FU-A header
  // has been replaced by the original nal header.
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet1, sizeof(packet1)));
  ExpectPacket(&payload, kExpected1, sizeof(kExpected1));
  EXPECT_EQ(VideoFrameType::kVideoFrameKey, payload.video_header().frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_TRUE(payload.video_header().is_first_packet_in_frame);
  const RTPVideoHeaderH264& h264 = payload.h264();
  EXPECT_EQ(kH264FuA, h264.packetization_type);
  EXPECT_EQ(kIdr, h264.nalu_type);
  ASSERT_EQ(1u, h264.nalus_length);
  EXPECT_EQ(static_cast<H264::NaluType>(kIdr), h264.nalus[0].type);
  EXPECT_EQ(-1, h264.nalus[0].sps_id);
  EXPECT_EQ(0, h264.nalus[0].pps_id);

  // Following packets will be 2 bytes shorter since they will only be appended
  // onto the first packet.
  payload = H264ParsedPayload();
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet2, sizeof(packet2)));
  ExpectPacket(&payload, kExpected2, sizeof(kExpected2));
  EXPECT_EQ(VideoFrameType::kVideoFrameKey, payload.video_header().frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_FALSE(payload.video_header().is_first_packet_in_frame);
  {
    const RTPVideoHeaderH264& h264 = payload.h264();
    EXPECT_EQ(kH264FuA, h264.packetization_type);
    EXPECT_EQ(kIdr, h264.nalu_type);
    // NALU info is only expected for the first FU-A packet.
    EXPECT_EQ(0u, h264.nalus_length);
  }

  payload = H264ParsedPayload();
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet3, sizeof(packet3)));
  ExpectPacket(&payload, kExpected3, sizeof(kExpected3));
  EXPECT_EQ(VideoFrameType::kVideoFrameKey, payload.video_header().frame_type);
  EXPECT_EQ(kVideoCodecH264, payload.video_header().codec);
  EXPECT_FALSE(payload.video_header().is_first_packet_in_frame);
  {
    const RTPVideoHeaderH264& h264 = payload.h264();
    EXPECT_EQ(kH264FuA, h264.packetization_type);
    EXPECT_EQ(kIdr, h264.nalu_type);
    // NALU info is only expected for the first FU-A packet.
    ASSERT_EQ(0u, h264.nalus_length);
  }
}

TEST_F(RtpDepacketizerH264Test, TestEmptyPayload) {
  // Using a wild pointer to crash on accesses from inside the depacketizer.
  uint8_t* garbage_ptr = reinterpret_cast<uint8_t*>(0x4711);
  H264ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, garbage_ptr, 0));
}

TEST_F(RtpDepacketizerH264Test, TestTruncatedFuaNalu) {
  const uint8_t kPayload[] = {0x9c};
  H264ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestTruncatedSingleStapANalu) {
  const uint8_t kPayload[] = {0xd8, 0x27};
  H264ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestStapAPacketWithTruncatedNalUnits) {
  const uint8_t kPayload[] = {0x58, 0xCB, 0xED, 0xDF};
  H264ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestTruncationJustAfterSingleStapANalu) {
  const uint8_t kPayload[] = {0x38, 0x27, 0x27};
  H264ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestShortSpsPacket) {
  const uint8_t kPayload[] = {0x27, 0x80, 0x00};
  H264ParsedPayload payload;
  EXPECT_TRUE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
}

TEST_F(RtpDepacketizerH264Test, TestSeiPacket) {
  const uint8_t kPayload[] = {
      kSei,                   // F=0, NRI=0, Type=6.
      0x03, 0x03, 0x03, 0x03  // Payload.
  };
  H264ParsedPayload payload;
  ASSERT_TRUE(depacketizer_->Parse(&payload, kPayload, sizeof(kPayload)));
  const RTPVideoHeaderH264& h264 = payload.h264();
  EXPECT_EQ(VideoFrameType::kVideoFrameDelta,
            payload.video_header().frame_type);
  EXPECT_EQ(kH264SingleNalu, h264.packetization_type);
  EXPECT_EQ(kSei, h264.nalu_type);
  ASSERT_EQ(1u, h264.nalus_length);
  EXPECT_EQ(static_cast<H264::NaluType>(kSei), h264.nalus[0].type);
  EXPECT_EQ(-1, h264.nalus[0].sps_id);
  EXPECT_EQ(-1, h264.nalus[0].pps_id);
}

}  // namespace
}  // namespace webrtc
