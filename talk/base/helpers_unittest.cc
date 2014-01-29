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

#include <string>

#include "talk/base/gunit.h"
#include "talk/base/helpers.h"
#include "talk/base/ssladapter.h"

namespace talk_base {

class RandomTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    talk_base::InitializeSSL();
  }

  static void TearDownTestCase() {
    talk_base::CleanupSSL();
  }
};

TEST_F(RandomTest, TestCreateRandomId) {
  CreateRandomId();
}

TEST_F(RandomTest, TestCreateRandomDouble) {
  for (int i = 0; i < 100; ++i) {
    double r = CreateRandomDouble();
    EXPECT_GE(r, 0.0);
    EXPECT_LT(r, 1.0);
  }
}

TEST_F(RandomTest, TestCreateNonZeroRandomId) {
  EXPECT_NE(0U, CreateRandomNonZeroId());
}

TEST_F(RandomTest, TestCreateRandomString) {
  std::string random = CreateRandomString(256);
  EXPECT_EQ(256U, random.size());
  std::string random2;
  EXPECT_TRUE(CreateRandomString(256, &random2));
  EXPECT_NE(random, random2);
  EXPECT_EQ(256U, random2.size());
}

TEST_F(RandomTest, TestCreateRandomForTest) {
  // Make sure we get the output we expect.
  SetRandomTestMode(true);
  EXPECT_EQ(2154761789U, CreateRandomId());
  EXPECT_EQ("h0ISP4S5SJKH/9EY", CreateRandomString(16));

  // Reset and make sure we get the same output.
  SetRandomTestMode(true);
  EXPECT_EQ(2154761789U, CreateRandomId());
  EXPECT_EQ("h0ISP4S5SJKH/9EY", CreateRandomString(16));

  // Test different character sets.
  SetRandomTestMode(true);
  std::string str;
  EXPECT_TRUE(CreateRandomString(16, "a", &str));
  EXPECT_EQ("aaaaaaaaaaaaaaaa", str);
  EXPECT_TRUE(CreateRandomString(16, "abc", &str));
  EXPECT_EQ("acbccaaaabbaacbb", str);

  // Turn off test mode for other tests.
  SetRandomTestMode(false);
}

}  // namespace talk_base
