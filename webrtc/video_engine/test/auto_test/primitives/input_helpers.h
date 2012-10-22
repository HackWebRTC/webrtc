/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_PRIMITIVES_
#define WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_PRIMITIVES_

#include <cstdlib>

#include <string>

namespace webrtc {

class InputValidator {
 public:
  virtual ~InputValidator() {}

  virtual bool InputOk(const std::string& value) const = 0;
};

class InputBuilder {
 public:
  // The input builder takes ownership of the validator.
  explicit InputBuilder(const InputValidator* input_validator);
  ~InputBuilder();

  // Ask the user for input, reads input from the input source and returns
  // the answer. This method will keep asking the user until a correct answer
  // is returned and is thereby guaranteed to return a response that is
  // acceptable to the input validator.
  std::string AskForInput() const;

  // Replaces the input source where we ask for input. Default is stdin.
  InputBuilder& WithInputSource(FILE* input_source);

  // Sets the input validator. The input builder takes ownership. If a default
  // value has been set, it must be acceptable to this validator.
  InputBuilder& WithInputValidator(const InputValidator* input_validator);
  // Sets a default value if the user doesn't want to give input. This value
  // must be acceptable to the input validator.
  InputBuilder& WithDefault(const std::string& default_value);
  // Prints a title before querying the user.
  InputBuilder& WithTitle(const std::string& title);

 private:
  FILE* input_source_;
  const InputValidator* input_validator_;
  std::string default_value_;
  std::string title_;
};

// Ensures input is an integer between low and high (inclusive).
class IntegerWithinRangeValidator : public InputValidator {
 public:
  IntegerWithinRangeValidator(int low, int high)
      : low_(low), high_(high) {}

  bool InputOk(const std::string& input) const {
    int value = atoi(input.c_str());
    // Note: atoi returns 0 on failure.
    if (value == 0 && input.length() > 0 && input[0] != '0')
      return false;  // Probably bad input.
    return value >= low_ && value <= high_;
  }

 private:
  int low_;
  int high_;
};

// Convenience method for creating an input builder.
InputBuilder TypedInput();

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_PRIMITIVES_
