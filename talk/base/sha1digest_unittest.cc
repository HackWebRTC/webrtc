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

#include "talk/base/sha1digest.h"
#include "talk/base/gunit.h"
#include "talk/base/stringencode.h"

namespace talk_base {

std::string Sha1(const std::string& input) {
  Sha1Digest sha1;
  return ComputeDigest(&sha1, input);
}

TEST(Sha1DigestTest, TestSize) {
  Sha1Digest sha1;
  EXPECT_EQ(20U, Sha1Digest::kSize);
  EXPECT_EQ(20U, sha1.Size());
}

TEST(Sha1DigestTest, TestBasic) {
  // Test vectors from sha1.c.
  EXPECT_EQ("da39a3ee5e6b4b0d3255bfef95601890afd80709", Sha1(""));
  EXPECT_EQ("a9993e364706816aba3e25717850c26c9cd0d89d", Sha1("abc"));
  EXPECT_EQ("84983e441c3bd26ebaae4aa1f95129e5e54670f1",
            Sha1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
  std::string a_million_as(1000000, 'a');
  EXPECT_EQ("34aa973cd4c4daa4f61eeb2bdbad27316534016f", Sha1(a_million_as));
}

TEST(Sha1DigestTest, TestMultipleUpdates) {
  Sha1Digest sha1;
  std::string input =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  char output[Sha1Digest::kSize];
  for (size_t i = 0; i < input.size(); ++i) {
    sha1.Update(&input[i], 1);
  }
  EXPECT_EQ(sha1.Size(), sha1.Finish(output, sizeof(output)));
  EXPECT_EQ("84983e441c3bd26ebaae4aa1f95129e5e54670f1",
            hex_encode(output, sizeof(output)));
}

TEST(Sha1DigestTest, TestReuse) {
  Sha1Digest sha1;
  std::string input = "abc";
  EXPECT_EQ("a9993e364706816aba3e25717850c26c9cd0d89d",
            ComputeDigest(&sha1, input));
  input = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  EXPECT_EQ("84983e441c3bd26ebaae4aa1f95129e5e54670f1",
            ComputeDigest(&sha1, input));
}

TEST(Sha1DigestTest, TestBufferTooSmall) {
  Sha1Digest sha1;
  std::string input = "abcdefghijklmnopqrstuvwxyz";
  char output[Sha1Digest::kSize - 1];
  sha1.Update(input.c_str(), input.size());
  EXPECT_EQ(0U, sha1.Finish(output, sizeof(output)));
}

TEST(Sha1DigestTest, TestBufferConst) {
  Sha1Digest sha1;
  const int kLongSize = 1000000;
  std::string input(kLongSize, '\0');
  for (int i = 0; i < kLongSize; ++i) {
    input[i] = static_cast<char>(i);
  }
  sha1.Update(input.c_str(), input.size());
  for (int i = 0; i < kLongSize; ++i) {
    EXPECT_EQ(static_cast<char>(i), input[i]);
  }
}

}  // namespace talk_base
