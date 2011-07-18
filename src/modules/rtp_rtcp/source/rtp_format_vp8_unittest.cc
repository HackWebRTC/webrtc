/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file includes unit tests for the VP8 packetizer.
 */

#include <gtest/gtest.h>

#include "typedefs.h"
#include "rtp_format_vp8.h"

namespace {

using webrtc::RTPFragmentationHeader;
using webrtc::RtpFormatVp8;
using webrtc::RTPVideoHeaderVP8;

const WebRtc_UWord32 kPayloadSize = 30;

class RtpFormatVp8Test : public ::testing::Test {
 protected:
  RtpFormatVp8Test() {};
  virtual void SetUp();
  virtual void TearDown();
  void CheckHeader(bool first_in_frame, bool frag_start, bool frag_end);
  void CheckPayload(int payload_end);
  void CheckLast(bool last) const;
  void CheckPacket(int send_bytes, int expect_bytes, bool last,
              bool first_in_frame, bool frag_start, bool frag_end);
  WebRtc_UWord8 payload_data_[kPayloadSize];
  WebRtc_UWord8 buffer_[kPayloadSize];
  WebRtc_UWord8 *data_ptr_;
  RTPFragmentationHeader* fragmentation_;
  RTPVideoHeaderVP8 hdr_info_;
  int payload_start_;
};

void RtpFormatVp8Test::SetUp() {
    for (int i = 0; i < kPayloadSize; i++)
    {
        payload_data_[i] = i / 10; // integer division
    }
    data_ptr_ = payload_data_;

    fragmentation_ = new RTPFragmentationHeader;
    fragmentation_->VerifyAndAllocateFragmentationHeader(3);
    fragmentation_->fragmentationLength[0] = 10;
    fragmentation_->fragmentationLength[1] = 10;
    fragmentation_->fragmentationLength[2] = 10;
    fragmentation_->fragmentationOffset[0] = 0;
    fragmentation_->fragmentationOffset[1] = 10;
    fragmentation_->fragmentationOffset[2] = 20;

    hdr_info_.pictureId = 0;
    hdr_info_.nonReference = false;
}

void RtpFormatVp8Test::TearDown() {
    delete fragmentation_;
}

#define EXPECT_BIT_EQ(x,n,a) EXPECT_EQ((((x)>>n)&0x1), a)

#define EXPECT_RSV_ZERO(x) EXPECT_EQ(((x)&0xE0), 0)

//#define EXPECT_BIT_I_EQ(x,a) EXPECT_EQ((((x)&0x10) > 0), (a > 0))
#define EXPECT_BIT_I_EQ(x,a) EXPECT_BIT_EQ(x, 4, a)

#define EXPECT_BIT_N_EQ(x,a) EXPECT_EQ((((x)&0x08) > 0), (a > 0))

#define EXPECT_FI_EQ(x,a) EXPECT_EQ((((x)&0x06) >> 1), a)

#define EXPECT_BIT_B_EQ(x,a) EXPECT_EQ((((x)&0x01) > 0), (a > 0))

void RtpFormatVp8Test::CheckHeader(bool first_in_frame, bool frag_start,
                                   bool frag_end)
{
    payload_start_ = 1;
    EXPECT_RSV_ZERO(buffer_[0]);
    if (first_in_frame & hdr_info_.pictureId != webrtc::kNoPictureId)
    {
        EXPECT_BIT_I_EQ(buffer_[0], 1);
        if (hdr_info_.pictureId > 0x7F)
        {
            EXPECT_BIT_EQ(buffer_[1], 7, 1);
            EXPECT_EQ(buffer_[1] & 0x7F,
                      (hdr_info_.pictureId >> 8) & 0x7F);
            EXPECT_EQ(buffer_[2], hdr_info_.pictureId & 0xFF);
            payload_start_ += 2;
        }
        else
        {
            EXPECT_BIT_EQ(buffer_[1], 7, 0);
            EXPECT_EQ(buffer_[1] & 0x7F,
                      (hdr_info_.pictureId) & 0x7F);
            payload_start_ += 1;
        }
    }
    EXPECT_BIT_N_EQ(buffer_[0], 0);
    WebRtc_UWord8 fi = 0x03;
    if (frag_start) fi = fi & 0x01;
    if (frag_end)   fi = fi & 0x02;
    EXPECT_FI_EQ(buffer_[0], fi);
    if (first_in_frame) EXPECT_BIT_B_EQ(buffer_[0], 1);
}

void RtpFormatVp8Test::CheckPayload(int payload_end)
{
    for (int i = payload_start_; i < payload_end; i++, data_ptr_++)
        EXPECT_EQ(buffer_[i], *data_ptr_);
}

void RtpFormatVp8Test::CheckLast(bool last) const
{
    EXPECT_EQ(last, data_ptr_ == payload_data_ + kPayloadSize);
}

void RtpFormatVp8Test::CheckPacket(int send_bytes, int expect_bytes, bool last,
            bool first_in_frame, bool frag_start, bool frag_end)
{
    EXPECT_EQ(send_bytes, expect_bytes);
    CheckHeader(first_in_frame, frag_start, frag_end);
    CheckPayload(send_bytes);
    CheckLast(last);
}

TEST_F(RtpFormatVp8Test, TestStrictMode)
{
    int send_bytes = 0;
    bool last;
    bool first_in_frame = true;

    hdr_info_.pictureId = 200; // > 0x7F should produce 2-byte PictureID
    RtpFormatVp8 packetizer = RtpFormatVp8(payload_data_, kPayloadSize,
        hdr_info_, *fragmentation_, webrtc::kStrict);

    // get first packet, expect balanced size = same as second packet
    EXPECT_EQ(0, packetizer.NextPacket(8, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 7, last,
                first_in_frame,
                /* frag_start */ true,
                /* frag_end */ false);
    first_in_frame = false;

    // get second packet
    EXPECT_EQ(0, packetizer.NextPacket(8, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 7, last,
                first_in_frame,
                /* frag_start */ false,
                /* frag_end */ true);

    // Second partition
    // Get first (and only) packet
    EXPECT_EQ(1, packetizer.NextPacket(20, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 11, last,
                first_in_frame,
                /* frag_start */ true,
                /* frag_end */ true);

    // Third partition
    // Get first packet (of four)
    EXPECT_EQ(2, packetizer.NextPacket(4, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 4, last,
                first_in_frame,
                /* frag_start */ true,
                /* frag_end */ false);

    // Get second packet (of four)
    EXPECT_EQ(2, packetizer.NextPacket(4, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 3, last,
                first_in_frame,
                /* frag_start */ false,
                /* frag_end */ false);

    // Get third packet (of four)
    EXPECT_EQ(2, packetizer.NextPacket(4, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 4, last,
                first_in_frame,
                /* frag_start */ false,
                /* frag_end */ false);

    // Get fourth and last packet
    EXPECT_EQ(2, packetizer.NextPacket(4, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 3, last,
                first_in_frame,
                /* frag_start */ false,
                /* frag_end */ true);
}

TEST_F(RtpFormatVp8Test, TestAggregateMode)
{
    int send_bytes = 0;
    bool last;
    bool first_in_frame = true;

    hdr_info_.pictureId = 20; // <= 0x7F should produce 1-byte PictureID
    RtpFormatVp8 packetizer = RtpFormatVp8(payload_data_, kPayloadSize,
        hdr_info_, *fragmentation_, webrtc::kAggregate);

    // get first packet
    // first half of first partition
    EXPECT_EQ(0, packetizer.NextPacket(6, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 6, last,
                first_in_frame,
                /* frag_start */ true,
                /* frag_end */ false);
    first_in_frame = false;

    // get second packet
    // second half of first partition
    EXPECT_EQ(0, packetizer.NextPacket(10, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 7, last,
                first_in_frame,
                /* frag_start */ false,
                /* frag_end */ true);

    // get third packet
    // last two partitions aggregated
    EXPECT_EQ(1, packetizer.NextPacket(25, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 21, last,
                first_in_frame,
                /* frag_start */ true,
                /* frag_end */ true);
}

TEST_F(RtpFormatVp8Test, TestSloppyMode)
{
    int send_bytes = 0;
    bool last;
    bool first_in_frame = true;

    hdr_info_.pictureId = webrtc::kNoPictureId; // no PictureID
    RtpFormatVp8 packetizer = RtpFormatVp8(payload_data_, kPayloadSize,
        hdr_info_, *fragmentation_, webrtc::kSloppy);

    // get first packet
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 9, last,
                first_in_frame,
                /* frag_start */ true,
                /* frag_end */ false);
    first_in_frame = false;

    // get second packet
    // fragments of first and second partitions
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 9, last,
                first_in_frame,
                /* frag_start */ false,
                /* frag_end */ false);

    // get third packet
    // fragments of second and third partitions
    EXPECT_EQ(1, packetizer.NextPacket(9, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 9, last,
                first_in_frame,
                /* frag_start */ false,
                /* frag_end */ false);

    // get fourth packet
    // second half of last partition
    EXPECT_EQ(2, packetizer.NextPacket(9, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 7, last,
                first_in_frame,
                /* frag_start */ false,
                /* frag_end */ true);
}

// Verify that sloppy mode is forced if fragmentation info is missing.
TEST_F(RtpFormatVp8Test, TestSloppyModeFallback)
{
    int send_bytes = 0;
    bool last;
    bool first_in_frame = true;

    hdr_info_.pictureId = 200; // > 0x7F should produce 2-byte PictureID
    RtpFormatVp8 packetizer = RtpFormatVp8(payload_data_, kPayloadSize,
        hdr_info_);

    // get first packet
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 9, last,
                first_in_frame,
                /* frag_start */ true,
                /* frag_end */ false);
    first_in_frame = false;

    // get second packet
    // fragments of first and second partitions
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 9, last,
                first_in_frame,
                /* frag_start */ false,
                /* frag_end */ false);

    // get third packet
    // fragments of second and third partitions
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 9, last,
                first_in_frame,
                /* frag_start */ false,
                /* frag_end */ false);

    // get fourth packet
    // second half of last partition
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer_, &send_bytes, &last));
    CheckPacket(send_bytes, 9, last,
                first_in_frame,
                /* frag_start */ false,
                /* frag_end */ true);
}

// Verify that non-reference bit is set.
TEST_F(RtpFormatVp8Test, TestNonReferenceBit) {
    int send_bytes = 0;
    bool last;
    bool first_in_frame = true;

    hdr_info_.nonReference = true;
    RtpFormatVp8 packetizer = RtpFormatVp8(payload_data_, kPayloadSize,
        hdr_info_);

    // get first packet
    ASSERT_EQ(0, packetizer.NextPacket(25, buffer_, &send_bytes, &last));
    ASSERT_FALSE(last);
    EXPECT_BIT_N_EQ(buffer_[0], 1);

    // get second packet
    ASSERT_EQ(0, packetizer.NextPacket(25, buffer_, &send_bytes, &last));
    ASSERT_TRUE(last);
    EXPECT_BIT_N_EQ(buffer_[0], 1);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

} // namespace
