/*
 * libjingle
 * Copyright 2008, Google Inc.
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

#include "talk/base/gunit.h"
#include "talk/base/optionsfile.h"

namespace talk_base {

#ifdef ANDROID
static const char *kTestFile = "/sdcard/.testfile";
#elif CHROMEOS
static const char *kTestFile = "/tmp/.testfile";
#else
static const char *kTestFile = ".testfile";
#endif

static const std::string kTestOptionA = "test-option-a";
static const std::string kTestOptionB = "test-option-b";
static const std::string kTestString1 = "a string";
static const std::string kTestString2 = "different string";
static const std::string kOptionWithEquals = "foo=bar";
static const std::string kOptionWithNewline = "foo\nbar";
static const std::string kValueWithEquals = "baz=quux";
static const std::string kValueWithNewline = "baz\nquux";
static const std::string kEmptyString = "";
static const char kOptionWithUtf8[] = {'O', 'p', 't', '\302', '\256', 'i', 'o',
    'n', '\342', '\204', '\242', '\0'};  // Opt(R)io(TM).
static const char kValueWithUtf8[] = {'V', 'a', 'l', '\302', '\256', 'v', 'e',
    '\342', '\204', '\242', '\0'};  // Val(R)ue(TM).
static int kTestInt1 = 12345;
static int kTestInt2 = 67890;
static int kNegInt = -634;
static int kZero = 0;

TEST(OptionsFile, GetSetString) {
  OptionsFile store(kTestFile);
  // Clear contents of the file on disk.
  EXPECT_TRUE(store.Save());
  std::string out1, out2;
  EXPECT_FALSE(store.GetStringValue(kTestOptionA, &out1));
  EXPECT_FALSE(store.GetStringValue(kTestOptionB, &out2));
  EXPECT_TRUE(store.SetStringValue(kTestOptionA, kTestString1));
  EXPECT_TRUE(store.Save());
  EXPECT_TRUE(store.Load());
  EXPECT_TRUE(store.SetStringValue(kTestOptionB, kTestString2));
  EXPECT_TRUE(store.Save());
  EXPECT_TRUE(store.Load());
  EXPECT_TRUE(store.GetStringValue(kTestOptionA, &out1));
  EXPECT_TRUE(store.GetStringValue(kTestOptionB, &out2));
  EXPECT_EQ(kTestString1, out1);
  EXPECT_EQ(kTestString2, out2);
  EXPECT_TRUE(store.RemoveValue(kTestOptionA));
  EXPECT_TRUE(store.Save());
  EXPECT_TRUE(store.Load());
  EXPECT_TRUE(store.RemoveValue(kTestOptionB));
  EXPECT_TRUE(store.Save());
  EXPECT_TRUE(store.Load());
  EXPECT_FALSE(store.GetStringValue(kTestOptionA, &out1));
  EXPECT_FALSE(store.GetStringValue(kTestOptionB, &out2));
}

TEST(OptionsFile, GetSetInt) {
  OptionsFile store(kTestFile);
  // Clear contents of the file on disk.
  EXPECT_TRUE(store.Save());
  int out1, out2;
  EXPECT_FALSE(store.GetIntValue(kTestOptionA, &out1));
  EXPECT_FALSE(store.GetIntValue(kTestOptionB, &out2));
  EXPECT_TRUE(store.SetIntValue(kTestOptionA, kTestInt1));
  EXPECT_TRUE(store.Save());
  EXPECT_TRUE(store.Load());
  EXPECT_TRUE(store.SetIntValue(kTestOptionB, kTestInt2));
  EXPECT_TRUE(store.Save());
  EXPECT_TRUE(store.Load());
  EXPECT_TRUE(store.GetIntValue(kTestOptionA, &out1));
  EXPECT_TRUE(store.GetIntValue(kTestOptionB, &out2));
  EXPECT_EQ(kTestInt1, out1);
  EXPECT_EQ(kTestInt2, out2);
  EXPECT_TRUE(store.RemoveValue(kTestOptionA));
  EXPECT_TRUE(store.Save());
  EXPECT_TRUE(store.Load());
  EXPECT_TRUE(store.RemoveValue(kTestOptionB));
  EXPECT_TRUE(store.Save());
  EXPECT_TRUE(store.Load());
  EXPECT_FALSE(store.GetIntValue(kTestOptionA, &out1));
  EXPECT_FALSE(store.GetIntValue(kTestOptionB, &out2));
  EXPECT_TRUE(store.SetIntValue(kTestOptionA, kNegInt));
  EXPECT_TRUE(store.GetIntValue(kTestOptionA, &out1));
  EXPECT_EQ(kNegInt, out1);
  EXPECT_TRUE(store.SetIntValue(kTestOptionA, kZero));
  EXPECT_TRUE(store.GetIntValue(kTestOptionA, &out1));
  EXPECT_EQ(kZero, out1);
}

TEST(OptionsFile, Persist) {
  {
    OptionsFile store(kTestFile);
    // Clear contents of the file on disk.
    EXPECT_TRUE(store.Save());
    EXPECT_TRUE(store.SetStringValue(kTestOptionA, kTestString1));
    EXPECT_TRUE(store.SetIntValue(kTestOptionB, kNegInt));
    EXPECT_TRUE(store.Save());
  }
  {
    OptionsFile store(kTestFile);
    // Load the saved contents from above.
    EXPECT_TRUE(store.Load());
    std::string out1;
    int out2;
    EXPECT_TRUE(store.GetStringValue(kTestOptionA, &out1));
    EXPECT_TRUE(store.GetIntValue(kTestOptionB, &out2));
    EXPECT_EQ(kTestString1, out1);
    EXPECT_EQ(kNegInt, out2);
  }
}

TEST(OptionsFile, SpecialCharacters) {
  OptionsFile store(kTestFile);
  // Clear contents of the file on disk.
  EXPECT_TRUE(store.Save());
  std::string out;
  EXPECT_FALSE(store.SetStringValue(kOptionWithEquals, kTestString1));
  EXPECT_FALSE(store.GetStringValue(kOptionWithEquals, &out));
  EXPECT_FALSE(store.SetStringValue(kOptionWithNewline, kTestString1));
  EXPECT_FALSE(store.GetStringValue(kOptionWithNewline, &out));
  EXPECT_TRUE(store.SetStringValue(kOptionWithUtf8, kValueWithUtf8));
  EXPECT_TRUE(store.SetStringValue(kTestOptionA, kTestString1));
  EXPECT_TRUE(store.Save());
  EXPECT_TRUE(store.Load());
  EXPECT_TRUE(store.GetStringValue(kTestOptionA, &out));
  EXPECT_EQ(kTestString1, out);
  EXPECT_TRUE(store.GetStringValue(kOptionWithUtf8, &out));
  EXPECT_EQ(kValueWithUtf8, out);
  EXPECT_FALSE(store.SetStringValue(kTestOptionA, kValueWithNewline));
  EXPECT_TRUE(store.GetStringValue(kTestOptionA, &out));
  EXPECT_EQ(kTestString1, out);
  EXPECT_TRUE(store.SetStringValue(kTestOptionA, kValueWithEquals));
  EXPECT_TRUE(store.Save());
  EXPECT_TRUE(store.Load());
  EXPECT_TRUE(store.GetStringValue(kTestOptionA, &out));
  EXPECT_EQ(kValueWithEquals, out);
  EXPECT_TRUE(store.SetStringValue(kEmptyString, kTestString2));
  EXPECT_TRUE(store.Save());
  EXPECT_TRUE(store.Load());
  EXPECT_TRUE(store.GetStringValue(kEmptyString, &out));
  EXPECT_EQ(kTestString2, out);
  EXPECT_TRUE(store.SetStringValue(kTestOptionB, kEmptyString));
  EXPECT_TRUE(store.Save());
  EXPECT_TRUE(store.Load());
  EXPECT_TRUE(store.GetStringValue(kTestOptionB, &out));
  EXPECT_EQ(kEmptyString, out);
}

}  // namespace talk_base
