/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/bitbuffer.h"
#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/common.h"
#include "webrtc/base/gunit.h"

namespace rtc {

TEST(BitBufferTest, ConsumeBits) {
  const uint8 bytes[64] = {0};
  BitBuffer buffer(bytes, 32);
  uint64 total_bits = 32 * 8;
  EXPECT_EQ(total_bits, buffer.RemainingBitCount());
  EXPECT_TRUE(buffer.ConsumeBits(3));
  total_bits -= 3;
  EXPECT_EQ(total_bits, buffer.RemainingBitCount());
  EXPECT_TRUE(buffer.ConsumeBits(3));
  total_bits -= 3;
  EXPECT_EQ(total_bits, buffer.RemainingBitCount());
  EXPECT_TRUE(buffer.ConsumeBits(15));
  total_bits -= 15;
  EXPECT_EQ(total_bits, buffer.RemainingBitCount());
  EXPECT_TRUE(buffer.ConsumeBits(37));
  total_bits -= 37;
  EXPECT_EQ(total_bits, buffer.RemainingBitCount());

  EXPECT_FALSE(buffer.ConsumeBits(32 * 8));
  EXPECT_EQ(total_bits, buffer.RemainingBitCount());
}

TEST(BitBufferTest, ReadBytesAligned) {
  const uint8 bytes[] = {0x0A, 0xBC, 0xDE, 0xF1, 0x23, 0x45, 0x67, 0x89};
  uint8 val8;
  uint16 val16;
  uint32 val32;
  BitBuffer buffer(bytes, 8);
  EXPECT_TRUE(buffer.ReadUInt8(&val8));
  EXPECT_EQ(0x0Au, val8);
  EXPECT_TRUE(buffer.ReadUInt8(&val8));
  EXPECT_EQ(0xBCu, val8);
  EXPECT_TRUE(buffer.ReadUInt16(&val16));
  EXPECT_EQ(0xDEF1u, val16);
  EXPECT_TRUE(buffer.ReadUInt32(&val32));
  EXPECT_EQ(0x23456789u, val32);
}

TEST(BitBufferTest, ReadBytesOffset4) {
  const uint8 bytes[] = {0x0A, 0xBC, 0xDE, 0xF1, 0x23, 0x45, 0x67, 0x89, 0x0A};
  uint8 val8;
  uint16 val16;
  uint32 val32;
  BitBuffer buffer(bytes, 9);
  EXPECT_TRUE(buffer.ConsumeBits(4));

  EXPECT_TRUE(buffer.ReadUInt8(&val8));
  EXPECT_EQ(0xABu, val8);
  EXPECT_TRUE(buffer.ReadUInt8(&val8));
  EXPECT_EQ(0xCDu, val8);
  EXPECT_TRUE(buffer.ReadUInt16(&val16));
  EXPECT_EQ(0xEF12u, val16);
  EXPECT_TRUE(buffer.ReadUInt32(&val32));
  EXPECT_EQ(0x34567890u, val32);
}

TEST(BitBufferTest, ReadBytesOffset3) {
  // The pattern we'll check against is counting down from 0b1111. It looks
  // weird here because it's all offset by 3.
  // Byte pattern is:
  //    56701234
  //  0b00011111,
  //  0b11011011,
  //  0b10010111,
  //  0b01010011,
  //  0b00001110,
  //  0b11001010,
  //  0b10000110,
  //  0b01000010
  //       xxxxx <-- last 5 bits unused.

  // The bytes. It almost looks like counting down by two at a time, except the
  // jump at 5->3->0, since that's when the high bit is turned off.
  const uint8 bytes[] = {0x1F, 0xDB, 0x97, 0x53, 0x0E, 0xCA, 0x86, 0x42};

  uint8 val8;
  uint16 val16;
  uint32 val32;
  BitBuffer buffer(bytes, 8);
  EXPECT_TRUE(buffer.ConsumeBits(3));
  EXPECT_TRUE(buffer.ReadUInt8(&val8));
  EXPECT_EQ(0xFEu, val8);
  EXPECT_TRUE(buffer.ReadUInt16(&val16));
  EXPECT_EQ(0xDCBAu, val16);
  EXPECT_TRUE(buffer.ReadUInt32(&val32));
  EXPECT_EQ(0x98765432u, val32);
  // 5 bits left unread. Not enough to read a uint8.
  EXPECT_EQ(5u, buffer.RemainingBitCount());
  EXPECT_FALSE(buffer.ReadUInt8(&val8));
}

TEST(BitBufferTest, ReadBits) {
  // Bit values are:
  //  0b01001101,
  //  0b00110010
  const uint8 bytes[] = {0x4D, 0x32};
  uint32_t val;
  BitBuffer buffer(bytes, 2);
  EXPECT_TRUE(buffer.ReadBits(&val, 3));
  // 0b010
  EXPECT_EQ(0x2u, val);
  EXPECT_TRUE(buffer.ReadBits(&val, 2));
  // 0b01
  EXPECT_EQ(0x1u, val);
  EXPECT_TRUE(buffer.ReadBits(&val, 7));
  // 0b1010011
  EXPECT_EQ(0x53u, val);
  EXPECT_TRUE(buffer.ReadBits(&val, 2));
  // 0b00
  EXPECT_EQ(0x0u, val);
  EXPECT_TRUE(buffer.ReadBits(&val, 1));
  // 0b1
  EXPECT_EQ(0x1u, val);
  EXPECT_TRUE(buffer.ReadBits(&val, 1));
  // 0b0
  EXPECT_EQ(0x0u, val);

  EXPECT_FALSE(buffer.ReadBits(&val, 1));
}

uint64 GolombEncoded(uint32 val) {
  val++;
  uint32 bit_counter = val;
  uint64 bit_count = 0;
  while (bit_counter > 0) {
    bit_count++;
    bit_counter >>= 1;
  }
  return static_cast<uint64>(val) << (64 - (bit_count * 2 - 1));
}

TEST(BitBufferTest, GolombString) {
  char test_string[] = "my precious";
  for (size_t i = 0; i < ARRAY_SIZE(test_string); ++i) {
    uint64 encoded_val = GolombEncoded(test_string[i]);
    // Use ByteBuffer to convert to bytes, to account for endianness (BitBuffer
    // requires network order).
    ByteBuffer byteBuffer;
    byteBuffer.WriteUInt64(encoded_val);
    BitBuffer buffer(reinterpret_cast<const uint8*>(byteBuffer.Data()),
                     byteBuffer.Length());
    uint32 decoded_val;
    EXPECT_TRUE(buffer.ReadExponentialGolomb(&decoded_val));
    EXPECT_EQ(test_string[i], static_cast<char>(decoded_val));
  }
}

TEST(BitBufferTest, NoGolombOverread) {
  const uint8 bytes[] = {0x00, 0xFF, 0xFF};
  // Make sure the bit buffer correctly enforces byte length on golomb reads.
  // If it didn't, the above buffer would be valid at 3 bytes.
  BitBuffer buffer(bytes, 1);
  uint32 decoded_val;
  EXPECT_FALSE(buffer.ReadExponentialGolomb(&decoded_val));

  BitBuffer longer_buffer(bytes, 2);
  EXPECT_FALSE(longer_buffer.ReadExponentialGolomb(&decoded_val));

  BitBuffer longest_buffer(bytes, 3);
  EXPECT_TRUE(longest_buffer.ReadExponentialGolomb(&decoded_val));
  // Golomb should have read 9 bits, so 0x01FF, and since it is golomb, the
  // result is 0x01FF - 1 = 0x01FE.
  EXPECT_EQ(0x01FEu, decoded_val);
}

}  // namespace rtc
