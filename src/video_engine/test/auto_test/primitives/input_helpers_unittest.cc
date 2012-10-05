/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gtest/gtest.h"
#include "video_engine/test/auto_test/primitives/fake_stdin.h"
#include "video_engine/test/auto_test/primitives/input_helpers.h"

namespace webrtc {

class InputHelpersTest: public testing::Test {
};

TEST_F(InputHelpersTest, AcceptsAnyInputExceptEmptyByDefault) {
  FILE* fake_stdin = FakeStdin("\n\nWhatever\n");
  std::string result = TypedInput().WithInputSource(fake_stdin).AskForInput();
  EXPECT_EQ("Whatever", result);
  fclose(fake_stdin);
}

TEST_F(InputHelpersTest, ReturnsDefaultOnEmptyInputIfDefaultSet) {
  FILE* fake_stdin = FakeStdin("\n\nWhatever\n");
  std::string result = TypedInput()
      .WithInputSource(fake_stdin)
      .WithDefault("MyDefault")
      .AskForInput();
  EXPECT_EQ("MyDefault", result);
  fclose(fake_stdin);
}

TEST_F(InputHelpersTest, CanSetTitle) {
  FILE* fake_stdin = FakeStdin("\n\nWhatever\n");
  std::string result = TypedInput()
      .WithInputSource(fake_stdin)
      .WithTitle("Make a choice!")
      .AskForInput();
  EXPECT_EQ("Whatever", result);
  fclose(fake_stdin);
}

TEST_F(InputHelpersTest, ObeysInputValidator) {
  class ValidatorWhichOnlyAcceptsFooBar : public InputValidator {
   public:
    bool InputOk(const std::string& input) const {
      return input == "FooBar";
    }
  };
  FILE* fake_stdin = FakeStdin("\nFoo\nBar\nFoo Bar\nFooBar\n");
  std::string result = TypedInput()
      .WithInputSource(fake_stdin)
      .WithInputValidator(new ValidatorWhichOnlyAcceptsFooBar())
      .AskForInput();
  EXPECT_EQ("FooBar", result);
  fclose(fake_stdin);
}
};
