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

#include "talk/base/buffer.h"
#include "talk/base/gunit.h"

namespace talk_base {

static const char kTestData[] = {
  0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF
};

TEST(BufferTest, TestConstructDefault) {
  Buffer buf;
  EXPECT_EQ(0U, buf.length());
  EXPECT_EQ(0U, buf.capacity());
  EXPECT_EQ(Buffer(), buf);
}

TEST(BufferTest, TestConstructEmptyWithCapacity) {
  Buffer buf(NULL, 0, 256U);
  EXPECT_EQ(0U, buf.length());
  EXPECT_EQ(256U, buf.capacity());
  EXPECT_EQ(Buffer(), buf);
}

TEST(BufferTest, TestConstructData) {
  Buffer buf(kTestData, sizeof(kTestData));
  EXPECT_EQ(sizeof(kTestData), buf.length());
  EXPECT_EQ(sizeof(kTestData), buf.capacity());
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, sizeof(kTestData)));
  EXPECT_EQ(Buffer(kTestData, sizeof(kTestData)), buf);
}

TEST(BufferTest, TestConstructDataWithCapacity) {
  Buffer buf(kTestData, sizeof(kTestData), 256U);
  EXPECT_EQ(sizeof(kTestData), buf.length());
  EXPECT_EQ(256U, buf.capacity());
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, sizeof(kTestData)));
  EXPECT_EQ(Buffer(kTestData, sizeof(kTestData)), buf);
}

TEST(BufferTest, TestConstructCopy) {
  Buffer buf1(kTestData, sizeof(kTestData), 256), buf2(buf1);
  EXPECT_EQ(sizeof(kTestData), buf2.length());
  EXPECT_EQ(sizeof(kTestData), buf2.capacity());  // capacity isn't copied
  EXPECT_EQ(0, memcmp(buf2.data(), kTestData, sizeof(kTestData)));
  EXPECT_EQ(buf1, buf2);
}

TEST(BufferTest, TestAssign) {
  Buffer buf1, buf2(kTestData, sizeof(kTestData), 256);
  EXPECT_NE(buf1, buf2);
  buf1 = buf2;
  EXPECT_EQ(sizeof(kTestData), buf1.length());
  EXPECT_EQ(sizeof(kTestData), buf1.capacity());  // capacity isn't copied
  EXPECT_EQ(0, memcmp(buf1.data(), kTestData, sizeof(kTestData)));
  EXPECT_EQ(buf1, buf2);
}

TEST(BufferTest, TestSetData) {
  Buffer buf;
  buf.SetData(kTestData, sizeof(kTestData));
  EXPECT_EQ(sizeof(kTestData), buf.length());
  EXPECT_EQ(sizeof(kTestData), buf.capacity());
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, sizeof(kTestData)));
}

TEST(BufferTest, TestAppendData) {
  Buffer buf(kTestData, sizeof(kTestData));
  buf.AppendData(kTestData, sizeof(kTestData));
  EXPECT_EQ(2 * sizeof(kTestData), buf.length());
  EXPECT_EQ(2 * sizeof(kTestData), buf.capacity());
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, sizeof(kTestData)));
  EXPECT_EQ(0, memcmp(buf.data() + sizeof(kTestData),
                      kTestData, sizeof(kTestData)));
}

TEST(BufferTest, TestSetLengthSmaller) {
  Buffer buf;
  buf.SetData(kTestData, sizeof(kTestData));
  buf.SetLength(sizeof(kTestData) / 2);
  EXPECT_EQ(sizeof(kTestData) / 2, buf.length());
  EXPECT_EQ(sizeof(kTestData), buf.capacity());
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, sizeof(kTestData) / 2));
}

TEST(BufferTest, TestSetLengthLarger) {
  Buffer buf;
  buf.SetData(kTestData, sizeof(kTestData));
  buf.SetLength(sizeof(kTestData) * 2);
  EXPECT_EQ(sizeof(kTestData) * 2, buf.length());
  EXPECT_EQ(sizeof(kTestData) * 2, buf.capacity());
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, sizeof(kTestData)));
}

TEST(BufferTest, TestSetCapacitySmaller) {
  Buffer buf;
  buf.SetData(kTestData, sizeof(kTestData));
  buf.SetCapacity(sizeof(kTestData) / 2);  // should be ignored
  EXPECT_EQ(sizeof(kTestData), buf.length());
  EXPECT_EQ(sizeof(kTestData), buf.capacity());
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, sizeof(kTestData)));
}

TEST(BufferTest, TestSetCapacityLarger) {
  Buffer buf(kTestData, sizeof(kTestData));
  buf.SetCapacity(sizeof(kTestData) * 2);
  EXPECT_EQ(sizeof(kTestData), buf.length());
  EXPECT_EQ(sizeof(kTestData) * 2, buf.capacity());
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, sizeof(kTestData)));
}

TEST(BufferTest, TestSetCapacityThenSetLength) {
  Buffer buf(kTestData, sizeof(kTestData));
  buf.SetCapacity(sizeof(kTestData) * 4);
  memcpy(buf.data() + sizeof(kTestData), kTestData, sizeof(kTestData));
  buf.SetLength(sizeof(kTestData) * 2);
  EXPECT_EQ(sizeof(kTestData) * 2, buf.length());
  EXPECT_EQ(sizeof(kTestData) * 4, buf.capacity());
  EXPECT_EQ(0, memcmp(buf.data(), kTestData, sizeof(kTestData)));
  EXPECT_EQ(0, memcmp(buf.data() + sizeof(kTestData),
                      kTestData, sizeof(kTestData)));
}

TEST(BufferTest, TestTransfer) {
  Buffer buf1(kTestData, sizeof(kTestData), 256U), buf2;
  buf1.TransferTo(&buf2);
  EXPECT_EQ(0U, buf1.length());
  EXPECT_EQ(0U, buf1.capacity());
  EXPECT_EQ(sizeof(kTestData), buf2.length());
  EXPECT_EQ(256U, buf2.capacity());  // capacity does transfer
  EXPECT_EQ(0, memcmp(buf2.data(), kTestData, sizeof(kTestData)));
}

}  // namespace talk_base
