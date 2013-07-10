/*
 * libjingle
 * Copyright 2010, Google Inc.
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

#include "talk/base/versionparsing.h"

#include "talk/base/gunit.h"

namespace talk_base {

static const int kExampleSegments = 4;

typedef int ExampleVersion[kExampleSegments];

TEST(VersionParsing, TestGoodParse) {
  ExampleVersion ver;
  std::string str1("1.1.2.0");
  static const ExampleVersion expect1 = {1, 1, 2, 0};
  EXPECT_TRUE(ParseVersionString(str1, kExampleSegments, ver));
  EXPECT_EQ(0, CompareVersions(ver, expect1, kExampleSegments));
  std::string str2("2.0.0.1");
  static const ExampleVersion expect2 = {2, 0, 0, 1};
  EXPECT_TRUE(ParseVersionString(str2, kExampleSegments, ver));
  EXPECT_EQ(0, CompareVersions(ver, expect2, kExampleSegments));
}

TEST(VersionParsing, TestBadParse) {
  ExampleVersion ver;
  std::string str1("1.1.2");
  EXPECT_FALSE(ParseVersionString(str1, kExampleSegments, ver));
  std::string str2("");
  EXPECT_FALSE(ParseVersionString(str2, kExampleSegments, ver));
  std::string str3("garbarge");
  EXPECT_FALSE(ParseVersionString(str3, kExampleSegments, ver));
}

TEST(VersionParsing, TestCompare) {
  static const ExampleVersion ver1 = {1, 0, 21, 0};
  static const ExampleVersion ver2 = {1, 1, 2, 0};
  static const ExampleVersion ver3 = {1, 1, 3, 0};
  static const ExampleVersion ver4 = {1, 1, 3, 9861};

  // Test that every combination of comparisons has the expected outcome.
  EXPECT_EQ(0, CompareVersions(ver1, ver1, kExampleSegments));
  EXPECT_EQ(0, CompareVersions(ver2, ver2, kExampleSegments));
  EXPECT_EQ(0, CompareVersions(ver3, ver3, kExampleSegments));
  EXPECT_EQ(0, CompareVersions(ver4, ver4, kExampleSegments));

  EXPECT_GT(0, CompareVersions(ver1, ver2, kExampleSegments));
  EXPECT_LT(0, CompareVersions(ver2, ver1, kExampleSegments));

  EXPECT_GT(0, CompareVersions(ver1, ver3, kExampleSegments));
  EXPECT_LT(0, CompareVersions(ver3, ver1, kExampleSegments));

  EXPECT_GT(0, CompareVersions(ver1, ver4, kExampleSegments));
  EXPECT_LT(0, CompareVersions(ver4, ver1, kExampleSegments));

  EXPECT_GT(0, CompareVersions(ver2, ver3, kExampleSegments));
  EXPECT_LT(0, CompareVersions(ver3, ver2, kExampleSegments));

  EXPECT_GT(0, CompareVersions(ver2, ver4, kExampleSegments));
  EXPECT_LT(0, CompareVersions(ver4, ver2, kExampleSegments));

  EXPECT_GT(0, CompareVersions(ver3, ver4, kExampleSegments));
  EXPECT_LT(0, CompareVersions(ver4, ver3, kExampleSegments));
}

}  // namespace talk_base
