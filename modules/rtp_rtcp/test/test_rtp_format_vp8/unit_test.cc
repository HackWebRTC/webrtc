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

#include "unit_test.h"
#include "rtp_format_vp8.h"

namespace webrtc {

RTPFormatVP8Test::RTPFormatVP8Test()
{
}

void RTPFormatVP8Test::SetUp() {
    payload_data = new WebRtc_UWord8[30];
    payload_size = 30;
    for (int i = 0; i < payload_size; i++)
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

void RTPFormatVP8Test::TearDown() {
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

TEST_F(RTPFormatVP8Test, TestStrictMode)
{
    WebRtc_UWord8 buffer[20];
    int send_bytes = 0;

    RTPFormatVP8 packetizer = RTPFormatVP8(payload_data, payload_size, fragmentation, kStrict);

    // get first packet
    EXPECT_FALSE(packetizer.NextPacket(8, buffer, &send_bytes));
    EXPECT_EQ(send_bytes,8);
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 1);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x01);
    EXPECT_BIT_B_EQ(buffer[0], 1);
    for (int i = 1; i < 8; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }

    // get second packet
    EXPECT_FALSE(packetizer.NextPacket(8, buffer, &send_bytes));
    EXPECT_EQ(send_bytes,4); // 3 remaining from partition, 1 header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x02);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 4; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }

    // Second partition
    // Get first (and only) packet
    EXPECT_FALSE(packetizer.NextPacket(20, buffer, &send_bytes));
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
    // Get first packet (of three)
    EXPECT_FALSE(packetizer.NextPacket(5, buffer, &send_bytes));
    EXPECT_EQ(send_bytes,5);
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x01); // first fragment
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 5; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

    // Get second packet (of three)
    EXPECT_FALSE(packetizer.NextPacket(5, buffer, &send_bytes));
    EXPECT_EQ(send_bytes,5);
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x03); // middle fragment
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 5; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

    // Get third and last packet
    EXPECT_TRUE(packetizer.NextPacket(5, buffer, &send_bytes)); // last packet in frame
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

TEST_F(RTPFormatVP8Test, TestAggregateMode)
{
    WebRtc_UWord8 buffer[30];
    int send_bytes = 0;

    RTPFormatVP8 packetizer = RTPFormatVP8(payload_data, payload_size, fragmentation,
        kAggregate);

    // get first packet
    // first two partitions aggregated
    EXPECT_FALSE(packetizer.NextPacket(25, buffer, &send_bytes));
    EXPECT_EQ(send_bytes,21); // Two 10-byte partitions and 1 byte header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 1);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x00);
    EXPECT_BIT_B_EQ(buffer[0], 1);
    for (int i = 1; i < 11; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }
    for (int i = 11; i < 21; i++)
    {
        EXPECT_EQ(buffer[i], 1);
    }

    // get second packet
    // first half of last partition
    EXPECT_FALSE(packetizer.NextPacket(6, buffer, &send_bytes));
    EXPECT_EQ(send_bytes,6); // First 5 from last partition, 1 header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x01);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 6; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

    // get third packet
    // second half of last partition
    EXPECT_TRUE(packetizer.NextPacket(6, buffer, &send_bytes)); // last packet
    EXPECT_EQ(send_bytes,6); // Last 5 from last partition, 1 header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x02);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 6; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

}

TEST_F(RTPFormatVP8Test, TestSloppyMode)
{
    WebRtc_UWord8 buffer[30];
    int send_bytes = 0;

    RTPFormatVP8 packetizer = RTPFormatVP8(payload_data, payload_size, fragmentation,
        kSloppy);

    // get first packet
    EXPECT_FALSE(packetizer.NextPacket(9, buffer, &send_bytes));
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
    EXPECT_FALSE(packetizer.NextPacket(9, buffer, &send_bytes));
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
    EXPECT_FALSE(packetizer.NextPacket(9, buffer, &send_bytes));
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
    EXPECT_TRUE(packetizer.NextPacket(9, buffer, &send_bytes)); // last packet
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
TEST_F(RTPFormatVP8Test, TestSloppyModeFallback)
{
    WebRtc_UWord8 buffer[30];
    int send_bytes = 0;

    RTPFormatVP8 packetizer = RTPFormatVP8(payload_data, payload_size, NULL /*fragmentation*/,
        kStrict); // should be changed to kSlopy

    // get first packet
    EXPECT_FALSE(packetizer.NextPacket(9, buffer, &send_bytes));
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
    EXPECT_FALSE(packetizer.NextPacket(9, buffer, &send_bytes));
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
    EXPECT_FALSE(packetizer.NextPacket(9, buffer, &send_bytes));
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
    EXPECT_TRUE(packetizer.NextPacket(9, buffer, &send_bytes)); // last packet
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

}
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

#include "unit_test.h"
#include "rtp_format_vp8.h"

namespace webrtc {

RTPFormatVP8Test::RTPFormatVP8Test()
{
}

void RTPFormatVP8Test::SetUp() {
    payload_data = new WebRtc_UWord8[30];
    payload_size = 30;
    for (int i = 0; i < payload_size; i++)
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

void RTPFormatVP8Test::TearDown() {
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

TEST_F(RTPFormatVP8Test, TestStrictMode)
{
    WebRtc_UWord8 buffer[20];
    int send_bytes = 0;

    RTPFormatVP8 packetizer = RTPFormatVP8(payload_data, payload_size, fragmentation, kStrict);

    // get first packet
    EXPECT_FALSE(packetizer.NextPacket(8, buffer, &send_bytes));
    EXPECT_EQ(send_bytes,8);
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 1);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x01);
    EXPECT_BIT_B_EQ(buffer[0], 1);
    for (int i = 1; i < 8; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }

    // get second packet
    EXPECT_FALSE(packetizer.NextPacket(8, buffer, &send_bytes));
    EXPECT_EQ(send_bytes,4); // 3 remaining from partition, 1 header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x02);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 4; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }

    // Second partition
    // Get first (and only) packet
    EXPECT_FALSE(packetizer.NextPacket(20, buffer, &send_bytes));
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
    // Get first packet (of three)
    EXPECT_FALSE(packetizer.NextPacket(5, buffer, &send_bytes));
    EXPECT_EQ(send_bytes,5);
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x01); // first fragment
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 5; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

    // Get second packet (of three)
    EXPECT_FALSE(packetizer.NextPacket(5, buffer, &send_bytes));
    EXPECT_EQ(send_bytes,5);
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x03); // middle fragment
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 5; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

    // Get third and last packet
    EXPECT_TRUE(packetizer.NextPacket(5, buffer, &send_bytes)); // last packet in frame
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

TEST_F(RTPFormatVP8Test, TestAggregateMode)
{
    WebRtc_UWord8 buffer[30];
    int send_bytes = 0;

    RTPFormatVP8 packetizer = RTPFormatVP8(payload_data, payload_size, fragmentation,
        kAggregate);

    // get first packet
    // first two partitions aggregated
    EXPECT_FALSE(packetizer.NextPacket(25, buffer, &send_bytes));
    EXPECT_EQ(send_bytes,21); // Two 10-byte partitions and 1 byte header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 1);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x00);
    EXPECT_BIT_B_EQ(buffer[0], 1);
    for (int i = 1; i < 11; i++)
    {
        EXPECT_EQ(buffer[i], 0);
    }
    for (int i = 11; i < 21; i++)
    {
        EXPECT_EQ(buffer[i], 1);
    }

    // get second packet
    // first half of last partition
    EXPECT_FALSE(packetizer.NextPacket(6, buffer, &send_bytes));
    EXPECT_EQ(send_bytes,6); // First 5 from last partition, 1 header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x01);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 6; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

    // get third packet
    // second half of last partition
    EXPECT_TRUE(packetizer.NextPacket(6, buffer, &send_bytes)); // last packet
    EXPECT_EQ(send_bytes,6); // Last 5 from last partition, 1 header
    EXPECT_RSV_ZERO(buffer[0]);
    EXPECT_BIT_I_EQ(buffer[0], 0);
    EXPECT_BIT_N_EQ(buffer[0], 0);
    EXPECT_FI_EQ(buffer[0], 0x02);
    EXPECT_BIT_B_EQ(buffer[0], 0);
    for (int i = 1; i < 6; i++)
    {
        EXPECT_EQ(buffer[i], 2);
    }

}

TEST_F(RTPFormatVP8Test, TestSloppyMode)
{
    WebRtc_UWord8 buffer[30];
    int send_bytes = 0;

    RTPFormatVP8 packetizer = RTPFormatVP8(payload_data, payload_size, fragmentation,
        kSloppy);

    // get first packet
    EXPECT_FALSE(packetizer.NextPacket(9, buffer, &send_bytes));
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
    EXPECT_FALSE(packetizer.NextPacket(9, buffer, &send_bytes));
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
    EXPECT_FALSE(packetizer.NextPacket(9, buffer, &send_bytes));
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
    EXPECT_TRUE(packetizer.NextPacket(9, buffer, &send_bytes)); // last packet
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
TEST_F(RTPFormatVP8Test, TestSloppyModeFallback)
{
    WebRtc_UWord8 buffer[30];
    int send_bytes = 0;

    RTPFormatVP8 packetizer = RTPFormatVP8(payload_data, payload_size, NULL /*fragmentation*/,
        kStrict); // should be changed to kSlopy

    // get first packet
    EXPECT_FALSE(packetizer.NextPacket(9, buffer, &send_bytes));
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
    EXPECT_FALSE(packetizer.NextPacket(9, buffer, &send_bytes));
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
    EXPECT_FALSE(packetizer.NextPacket(9, buffer, &send_bytes));
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
    EXPECT_TRUE(packetizer.NextPacket(9, buffer, &send_bytes)); // last packet
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

}
