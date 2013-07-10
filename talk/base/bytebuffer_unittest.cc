/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/base/bytebuffer.h"
#include "talk/base/byteorder.h"
#include "talk/base/common.h"
#include "talk/base/gunit.h"

namespace talk_base {

TEST(ByteBufferTest, TestByteOrder) {
  uint16 n16 = 1;
  uint32 n32 = 1;
  uint64 n64 = 1;

  EXPECT_EQ(n16, NetworkToHost16(HostToNetwork16(n16)));
  EXPECT_EQ(n32, NetworkToHost32(HostToNetwork32(n32)));
  EXPECT_EQ(n64, NetworkToHost64(HostToNetwork64(n64)));

  if (IsHostBigEndian()) {
    // The host is the network (big) endian.
    EXPECT_EQ(n16, HostToNetwork16(n16));
    EXPECT_EQ(n32, HostToNetwork32(n32));
    EXPECT_EQ(n64, HostToNetwork64(n64));

    // GetBE converts big endian to little endian here.
    EXPECT_EQ(n16 >> 8, GetBE16(&n16));
    EXPECT_EQ(n32 >> 24, GetBE32(&n32));
    EXPECT_EQ(n64 >> 56, GetBE64(&n64));
  } else {
    // The host is little endian.
    EXPECT_NE(n16, HostToNetwork16(n16));
    EXPECT_NE(n32, HostToNetwork32(n32));
    EXPECT_NE(n64, HostToNetwork64(n64));

    // GetBE converts little endian to big endian here.
    EXPECT_EQ(GetBE16(&n16), HostToNetwork16(n16));
    EXPECT_EQ(GetBE32(&n32), HostToNetwork32(n32));
    EXPECT_EQ(GetBE64(&n64), HostToNetwork64(n64));

    // GetBE converts little endian to big endian here.
    EXPECT_EQ(n16 << 8, GetBE16(&n16));
    EXPECT_EQ(n32 << 24, GetBE32(&n32));
    EXPECT_EQ(n64 << 56, GetBE64(&n64));
  }
}

TEST(ByteBufferTest, TestBufferLength) {
  ByteBuffer buffer;
  size_t size = 0;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt8(1);
  ++size;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt16(1);
  size += 2;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt24(1);
  size += 3;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt32(1);
  size += 4;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt64(1);
  size += 8;
  EXPECT_EQ(size, buffer.Length());

  EXPECT_TRUE(buffer.Consume(0));
  EXPECT_EQ(size, buffer.Length());

  EXPECT_TRUE(buffer.Consume(4));
  size -= 4;
  EXPECT_EQ(size, buffer.Length());
}

TEST(ByteBufferTest, TestGetSetReadPosition) {
  ByteBuffer buffer("ABCDEF", 6);
  EXPECT_EQ(6U, buffer.Length());
  ByteBuffer::ReadPosition pos(buffer.GetReadPosition());
  EXPECT_TRUE(buffer.SetReadPosition(pos));
  EXPECT_EQ(6U, buffer.Length());
  std::string read;
  EXPECT_TRUE(buffer.ReadString(&read, 3));
  EXPECT_EQ("ABC", read);
  EXPECT_EQ(3U, buffer.Length());
  EXPECT_TRUE(buffer.SetReadPosition(pos));
  EXPECT_EQ(6U, buffer.Length());
  read.clear();
  EXPECT_TRUE(buffer.ReadString(&read, 3));
  EXPECT_EQ("ABC", read);
  EXPECT_EQ(3U, buffer.Length());
  // For a resize by writing Capacity() number of bytes.
  size_t capacity = buffer.Capacity();
  buffer.ReserveWriteBuffer(buffer.Capacity());
  EXPECT_EQ(capacity + 3U, buffer.Length());
  EXPECT_FALSE(buffer.SetReadPosition(pos));
  read.clear();
  EXPECT_TRUE(buffer.ReadString(&read, 3));
  EXPECT_EQ("DEF", read);
}

TEST(ByteBufferTest, TestReadWriteBuffer) {
  ByteBuffer::ByteOrder orders[2] = { ByteBuffer::ORDER_HOST,
                                      ByteBuffer::ORDER_NETWORK };
  for (size_t i = 0; i < ARRAY_SIZE(orders); i++) {
    ByteBuffer buffer(orders[i]);
    EXPECT_EQ(orders[i], buffer.Order());
    uint8 ru8;
    EXPECT_FALSE(buffer.ReadUInt8(&ru8));

    // Write and read uint8.
    uint8 wu8 = 1;
    buffer.WriteUInt8(wu8);
    EXPECT_TRUE(buffer.ReadUInt8(&ru8));
    EXPECT_EQ(wu8, ru8);
    EXPECT_EQ(0U, buffer.Length());

    // Write and read uint16.
    uint16 wu16 = (1 << 8) + 1;
    buffer.WriteUInt16(wu16);
    uint16 ru16;
    EXPECT_TRUE(buffer.ReadUInt16(&ru16));
    EXPECT_EQ(wu16, ru16);
    EXPECT_EQ(0U, buffer.Length());

    // Write and read uint24.
    uint32 wu24 = (3 << 16) + (2 << 8) + 1;
    buffer.WriteUInt24(wu24);
    uint32 ru24;
    EXPECT_TRUE(buffer.ReadUInt24(&ru24));
    EXPECT_EQ(wu24, ru24);
    EXPECT_EQ(0U, buffer.Length());

    // Write and read uint32.
    uint32 wu32 = (4 << 24) + (3 << 16) + (2 << 8) + 1;
    buffer.WriteUInt32(wu32);
    uint32 ru32;
    EXPECT_TRUE(buffer.ReadUInt32(&ru32));
    EXPECT_EQ(wu32, ru32);
    EXPECT_EQ(0U, buffer.Length());

    // Write and read uint64.
    uint32 another32 = (8 << 24) + (7 << 16) + (6 << 8) + 5;
    uint64 wu64 = (static_cast<uint64>(another32) << 32) + wu32;
    buffer.WriteUInt64(wu64);
    uint64 ru64;
    EXPECT_TRUE(buffer.ReadUInt64(&ru64));
    EXPECT_EQ(wu64, ru64);
    EXPECT_EQ(0U, buffer.Length());

    // Write and read string.
    std::string write_string("hello");
    buffer.WriteString(write_string);
    std::string read_string;
    EXPECT_TRUE(buffer.ReadString(&read_string, write_string.size()));
    EXPECT_EQ(write_string, read_string);
    EXPECT_EQ(0U, buffer.Length());

    // Write and read bytes
    char write_bytes[] = "foo";
    buffer.WriteBytes(write_bytes, 3);
    char read_bytes[3];
    EXPECT_TRUE(buffer.ReadBytes(read_bytes, 3));
    for (int i = 0; i < 3; ++i) {
      EXPECT_EQ(write_bytes[i], read_bytes[i]);
    }
    EXPECT_EQ(0U, buffer.Length());

    // Write and read reserved buffer space
    char* write_dst = buffer.ReserveWriteBuffer(3);
    memcpy(write_dst, write_bytes, 3);
    memset(read_bytes, 0, 3);
    EXPECT_TRUE(buffer.ReadBytes(read_bytes, 3));
    for (int i = 0; i < 3; ++i) {
      EXPECT_EQ(write_bytes[i], read_bytes[i]);
    }
    EXPECT_EQ(0U, buffer.Length());

    // Write and read in order.
    buffer.WriteUInt8(wu8);
    buffer.WriteUInt16(wu16);
    buffer.WriteUInt24(wu24);
    buffer.WriteUInt32(wu32);
    buffer.WriteUInt64(wu64);
    EXPECT_TRUE(buffer.ReadUInt8(&ru8));
    EXPECT_EQ(wu8, ru8);
    EXPECT_TRUE(buffer.ReadUInt16(&ru16));
    EXPECT_EQ(wu16, ru16);
    EXPECT_TRUE(buffer.ReadUInt24(&ru24));
    EXPECT_EQ(wu24, ru24);
    EXPECT_TRUE(buffer.ReadUInt32(&ru32));
    EXPECT_EQ(wu32, ru32);
    EXPECT_TRUE(buffer.ReadUInt64(&ru64));
    EXPECT_EQ(wu64, ru64);
    EXPECT_EQ(0U, buffer.Length());
  }
}

}  // namespace talk_base
