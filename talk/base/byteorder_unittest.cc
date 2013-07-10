/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#include "talk/base/byteorder.h"

#include "talk/base/basictypes.h"
#include "talk/base/gunit.h"

namespace talk_base {

// Test memory set functions put values into memory in expected order.
TEST(ByteOrderTest, TestSet) {
  uint8 buf[8] = { 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
  Set8(buf, 0, 0xfb);
  Set8(buf, 1, 0x12);
  EXPECT_EQ(0xfb, buf[0]);
  EXPECT_EQ(0x12, buf[1]);
  SetBE16(buf, 0x1234);
  EXPECT_EQ(0x12, buf[0]);
  EXPECT_EQ(0x34, buf[1]);
  SetLE16(buf, 0x1234);
  EXPECT_EQ(0x34, buf[0]);
  EXPECT_EQ(0x12, buf[1]);
  SetBE32(buf, 0x12345678);
  EXPECT_EQ(0x12, buf[0]);
  EXPECT_EQ(0x34, buf[1]);
  EXPECT_EQ(0x56, buf[2]);
  EXPECT_EQ(0x78, buf[3]);
  SetLE32(buf, 0x12345678);
  EXPECT_EQ(0x78, buf[0]);
  EXPECT_EQ(0x56, buf[1]);
  EXPECT_EQ(0x34, buf[2]);
  EXPECT_EQ(0x12, buf[3]);
  SetBE64(buf, UINT64_C(0x0123456789abcdef));
  EXPECT_EQ(0x01, buf[0]);
  EXPECT_EQ(0x23, buf[1]);
  EXPECT_EQ(0x45, buf[2]);
  EXPECT_EQ(0x67, buf[3]);
  EXPECT_EQ(0x89, buf[4]);
  EXPECT_EQ(0xab, buf[5]);
  EXPECT_EQ(0xcd, buf[6]);
  EXPECT_EQ(0xef, buf[7]);
  SetLE64(buf, UINT64_C(0x0123456789abcdef));
  EXPECT_EQ(0xef, buf[0]);
  EXPECT_EQ(0xcd, buf[1]);
  EXPECT_EQ(0xab, buf[2]);
  EXPECT_EQ(0x89, buf[3]);
  EXPECT_EQ(0x67, buf[4]);
  EXPECT_EQ(0x45, buf[5]);
  EXPECT_EQ(0x23, buf[6]);
  EXPECT_EQ(0x01, buf[7]);
}

// Test memory get functions get values from memory in expected order.
TEST(ByteOrderTest, TestGet) {
  uint8 buf[8];
  buf[0] = 0x01u;
  buf[1] = 0x23u;
  buf[2] = 0x45u;
  buf[3] = 0x67u;
  buf[4] = 0x89u;
  buf[5] = 0xabu;
  buf[6] = 0xcdu;
  buf[7] = 0xefu;
  EXPECT_EQ(0x01u, Get8(buf, 0));
  EXPECT_EQ(0x23u, Get8(buf, 1));
  EXPECT_EQ(0x0123u, GetBE16(buf));
  EXPECT_EQ(0x2301u, GetLE16(buf));
  EXPECT_EQ(0x01234567u, GetBE32(buf));
  EXPECT_EQ(0x67452301u, GetLE32(buf));
  EXPECT_EQ(UINT64_C(0x0123456789abcdef), GetBE64(buf));
  EXPECT_EQ(UINT64_C(0xefcdab8967452301), GetLE64(buf));
}

}  // namespace talk_base

