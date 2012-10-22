/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/test/auto_test/primitives/input_helpers.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

#include "gflags/gflags.h"

namespace webrtc {

DEFINE_bool(choose_defaults, false,
    "Make the default choice at every choice when running a custom call.");

class AcceptAllNonEmptyValidator : public InputValidator {
 public:
  bool InputOk(const std::string& value) const {
    return value.length() > 0;
  }
};

InputBuilder::InputBuilder(const InputValidator* input_validator)
    : input_source_(stdin), input_validator_(input_validator),
      default_value_("") {
}

InputBuilder::~InputBuilder() {
  delete input_validator_;
}

std::string InputBuilder::AskForInput() const {
  if (FLAGS_choose_defaults && !default_value_.empty())
    return default_value_;
  if (!title_.empty())
    printf("\n%s\n", title_.c_str());

  if (!default_value_.empty())
    printf("Hit enter for default (%s):\n", default_value_.c_str());

  printf("# ");
  char raw_input[128];
  if (!fgets(raw_input, 128, input_source_)) {
    // If we get here the user probably hit CTRL+D.
    exit(1);
  }

  std::string input = raw_input;
  input = input.substr(0, input.size() - 1);  // Strip last \n.

  if (input.empty() && !default_value_.empty())
    return default_value_;

  if (!input_validator_->InputOk(input)) {
    printf("Invalid input. Please try again.\n");
    return AskForInput();
  }
  return input;
}

InputBuilder& InputBuilder::WithInputSource(FILE* input_source) {
  input_source_ = input_source;
  return *this;
}

InputBuilder& InputBuilder::WithInputValidator(
    const InputValidator* input_validator) {
  // If there's a default value, it must be accepted by the input validator.
  assert(default_value_.empty() || input_validator->InputOk(default_value_));
  delete input_validator_;
  input_validator_ = input_validator;
  return *this;
}

InputBuilder& InputBuilder::WithDefault(const std::string& default_value) {
  assert(input_validator_->InputOk(default_value));
  default_value_ = default_value;
  return *this;
}

InputBuilder& InputBuilder::WithTitle(const std::string& title) {
  title_ = title;
  return *this;
}

InputBuilder TypedInput() {
  return InputBuilder(new AcceptAllNonEmptyValidator());
}

}  // namespace webrtc
