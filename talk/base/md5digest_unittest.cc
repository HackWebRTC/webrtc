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

#include "talk/base/md5digest.h"
#include "talk/base/gunit.h"
#include "talk/base/stringencode.h"

namespace talk_base {

std::string Md5(const std::string& input) {
  Md5Digest md5;
  return ComputeDigest(&md5, input);
}

TEST(Md5DigestTest, TestSize) {
  Md5Digest md5;
  EXPECT_EQ(16U, Md5Digest::kSize);
  EXPECT_EQ(16U, md5.Size());
}

TEST(Md5DigestTest, TestBasic) {
  // These are the standard MD5 test vectors from RFC 1321.
  EXPECT_EQ("d41d8cd98f00b204e9800998ecf8427e", Md5(""));
  EXPECT_EQ("0cc175b9c0f1b6a831c399e269772661", Md5("a"));
  EXPECT_EQ("900150983cd24fb0d6963f7d28e17f72", Md5("abc"));
  EXPECT_EQ("f96b697d7cb7938d525a2f31aaf161d0", Md5("message digest"));
  EXPECT_EQ("c3fcd3d76192e4007dfb496cca67e13b",
            Md5("abcdefghijklmnopqrstuvwxyz"));
}

TEST(Md5DigestTest, TestMultipleUpdates) {
  Md5Digest md5;
  std::string input = "abcdefghijklmnopqrstuvwxyz";
  char output[Md5Digest::kSize];
  for (size_t i = 0; i < input.size(); ++i) {
    md5.Update(&input[i], 1);
  }
  EXPECT_EQ(md5.Size(), md5.Finish(output, sizeof(output)));
  EXPECT_EQ("c3fcd3d76192e4007dfb496cca67e13b",
            hex_encode(output, sizeof(output)));
}

TEST(Md5DigestTest, TestReuse) {
  Md5Digest md5;
  std::string input = "message digest";
  EXPECT_EQ("f96b697d7cb7938d525a2f31aaf161d0", ComputeDigest(&md5, input));
  input = "abcdefghijklmnopqrstuvwxyz";
  EXPECT_EQ("c3fcd3d76192e4007dfb496cca67e13b", ComputeDigest(&md5, input));
}

TEST(Md5DigestTest, TestBufferTooSmall) {
  Md5Digest md5;
  std::string input = "abcdefghijklmnopqrstuvwxyz";
  char output[Md5Digest::kSize - 1];
  md5.Update(input.c_str(), input.size());
  EXPECT_EQ(0U, md5.Finish(output, sizeof(output)));
}

TEST(Md5DigestTest, TestBufferConst) {
  Md5Digest md5;
  const int kLongSize = 1000000;
  std::string input(kLongSize, '\0');
  for (int i = 0; i < kLongSize; ++i) {
    input[i] = static_cast<char>(i);
  }
  md5.Update(input.c_str(), input.size());
  for (int i = 0; i < kLongSize; ++i) {
    EXPECT_EQ(static_cast<char>(i), input[i]);
  }
}

}  // namespace talk_base
