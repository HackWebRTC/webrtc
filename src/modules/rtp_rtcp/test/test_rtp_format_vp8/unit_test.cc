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

const WebRtc_UWord32 kPayloadSize = 30;

class RtpFormatVp8Test : public ::testing::Test {
 protected:
  RtpFormatVp8Test() {};
  virtual void SetUp();
  virtual void TearDown();
  WebRtc_UWord8* payload_data;
  RTPFragmentationHeader* fragmentation;
};

void RtpFormatVp8Test::SetUp() {
    payload_data = new WebRtc_UWord8[kPayloadSize];
    for (int i = 0; i < kPayloadSize; i++)
    {
        payload_data[i] = i / 10; // integer division
    }
    fragmentation = new RTPFragmentationHeader;
    fragmentation->VerifyAndAllocateFragmentationHeader(3);
    fragmentation->fragmentationLength[0] = 10;
    fragmentation->fragmentationLength[1] = 10;
    fragmentation->fragmentationLength[2] = 10;
    fragmentation->fragmentationOffset[0] = 0;
    fragmentation->fragmentationOffset[1] = 10;
    fragmentation->fragmentationOffset[2] = 20;
}

void RtpFormatVp8Test::TearDown() {
    delete [] payload_data;
    delete fragmentation;
}

#define EXPECT_BIT_EQ(x,n,a) EXPECT_EQ((((x)>>n)&0x1), a)

#define EXPECT_RSV_ZERO(x) EXPECT_EQ(((x)&0xE0), 0)

//#define EXPECT_BIT_I_EQ(x,a) EXPECT_EQ((((x)&0x10) > 0), (a > 0))
#define EXPECT_BIT_I_EQ(x,a) EXPECT_BIT_EQ(x, 4, a)

#define EXPECT_BIT_N_EQ(x,a) EXPECT_EQ((((x)&0x08) > 0), (a > 0))

#define EXPECT_FI_EQ(x,a) EXPECT_EQ((((x)&0x06) >> 1), a)

#define EXPECT_BIT_B_EQ(x,a) EXPECT_EQ((((x)&0x01) > 0), (a > 0))

TEST_F(RtpFormatVp8Test, TestStrictMode)
{
    WebRtc_UWord8 buffer[20];
    int send_bytes = 0;
    bool last;

    RtpFormatVp8 packetizer = RtpFormatVp8(payload_data, kPayloadSize,
        *fragmentation, webrtc::kStrict);

    // get first packet, expect balanced size = same as second packet
    EXPECT_EQ(0, packetizer.NextPacket(8, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,6);
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 1);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x01);
    EXPECT_BIT_B_EQ(buffer[0], 1);
    for (int i = 1; i < 6; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }

    // get second packet
    EXPECT_EQ(0, packetizer.NextPacket(8, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,6); // 5 remaining from partition, 1 header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x02);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 6; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }

    // Second partition
    // Get first (and only) packet
    EXPECT_EQ(0, packetizer.NextPacket(20, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,11);
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x00);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 11; i++)
    {
        EXPECT_EQ(buffer[i], 1);
    }

    // Third partition
    // Get first packet (of four)
    EXPECT_EQ(0, packetizer.NextPacket(4, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,4);
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x01); // first fragment
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 4; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

    // Get second packet (of four)
    EXPECT_EQ(0, packetizer.NextPacket(4, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,3);
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x03); // middle fragment
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 3; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

    // Get third packet (of four)
    EXPECT_EQ(0, packetizer.NextPacket(4, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,4);
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x03); // middle fragment
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 4; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

    // Get fourth and last packet
    EXPECT_EQ(0, packetizer.NextPacket(4, buffer, &send_bytes, &last));
    EXPECT_TRUE(last); // last packet in frame
    EXPECT_EQ(send_bytes,3); // 2 bytes payload left, 1 header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x02); // last fragment
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 3; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

}

TEST_F(RtpFormatVp8Test, TestAggregateMode)
{
    WebRtc_UWord8 buffer[kPayloadSize];
    int send_bytes = 0;
    bool last;

    RtpFormatVp8 packetizer = RtpFormatVp8(payload_data, kPayloadSize,
        *fragmentation, webrtc::kAggregate);

    // get first packet
    // first half of first partition
    EXPECT_EQ(0, packetizer.NextPacket(6, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,6); // First 5 from first partition, 1 header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 1);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x01);
    EXPECT_BIT_B_EQ(buffer[0], 1);
    for (int i = 1; i < 6; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }

    // get second packet
    // second half of first partition
    EXPECT_EQ(0, packetizer.NextPacket(10, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,6); // Last 5 from first partition, 1 header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x02);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 6; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }

    // get third packet
    // last two partitions aggregated
    EXPECT_EQ(0, packetizer.NextPacket(25, buffer, &send_bytes, &last));
    EXPECT_TRUE(last); // last packet
    EXPECT_EQ(send_bytes,21); // Two 10-byte partitions and 1 byte header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x00);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 11; i++)
    {
        EXPECT_EQ(buffer[i], 1);
    }
    for (int i = 11; i < 21; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

}

TEST_F(RtpFormatVp8Test, TestSloppyMode)
{
    WebRtc_UWord8 buffer[kPayloadSize];
    int send_bytes = 0;
    bool last;

    RtpFormatVp8 packetizer = RtpFormatVp8(payload_data, kPayloadSize,
        *fragmentation, webrtc::kSloppy);

    // get first packet
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,9); // 8 bytes payload and 1 byte header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 1);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x01);
    EXPECT_BIT_B_EQ(buffer[0], 1);
    for (int i = 1; i < 9; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }

    // get second packet
    // fragments of first and second partitions
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,9); // 8 bytes (2+6) payload and 1 byte header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x03);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i <= 2; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }
    for (int i = 3; i < 9; i++)
    {
        EXPECT_EQ(buffer[i], 1);
    }

    // get third packet
    // fragments of second and third partitions
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,9); // 8 bytes (4+4) payload and 1 byte header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x03);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i <= 4; i++)
    {
        EXPECT_EQ(buffer[i], 1);
    }
    for (int i = 5; i < 9; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

    // get fourth packet
    // second half of last partition
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer, &send_bytes, &last));
    EXPECT_TRUE(last); // last packet
    EXPECT_EQ(send_bytes,7); // Last 6 from last partition, 1 header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x02);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 5; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

}

// Verify that sloppy mode is forced if fragmentation info is missing.
TEST_F(RtpFormatVp8Test, TestSloppyModeFallback)
{
    WebRtc_UWord8 buffer[kPayloadSize];
    int send_bytes = 0;
    bool last;

    RtpFormatVp8 packetizer = RtpFormatVp8(payload_data, kPayloadSize);

    // get first packet
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,9); // 8 bytes payload and 1 byte header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 1);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x01);
    EXPECT_BIT_B_EQ(buffer[0], 1);
    for (int i = 1; i < 9; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }

    // get second packet
    // fragments of first and second partitions
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,9); // 8 bytes (2+6) payload and 1 byte header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x03);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i <= 2; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }
    for (int i = 3; i < 9; i++)
    {
        EXPECT_EQ(buffer[i], 1);
    }

    // get third packet
    // fragments of second and third partitions
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer, &send_bytes, &last));
    EXPECT_FALSE(last);
    EXPECT_EQ(send_bytes,9); // 8 bytes (4+4) payload and 1 byte header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x03);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i <= 4; i++)
    {
        EXPECT_EQ(buffer[i], 1);
    }
    for (int i = 5; i < 9; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

    // get fourth packet
    // second half of last partition
    EXPECT_EQ(0, packetizer.NextPacket(9, buffer, &send_bytes, &last));
    EXPECT_TRUE(last); // last packet
    EXPECT_EQ(send_bytes,7); // Last 6 from last partition, 1 header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x02);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 5; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

} // namespace
