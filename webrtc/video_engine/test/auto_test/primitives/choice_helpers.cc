/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/test/auto_test/primitives/choice_helpers.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

namespace webrtc {

ChoiceBuilder::ChoiceBuilder(const Choices& choices)
    : choices_(choices),
      input_helper_(new IntegerWithinRangeValidator(1, choices.size())) {
}

int ChoiceBuilder::Choose() {
  std::string input = input_helper_
      .WithTitle(MakeTitleWithOptions())
      .AskForInput();
  return atoi(input.c_str());
}

ChoiceBuilder& ChoiceBuilder::WithDefault(const std::string& default_choice) {
  Choices::const_iterator iterator = std::find(
      choices_.begin(), choices_.end(), default_choice);
  assert(iterator != choices_.end() && "No such choice.");

  // Store the value as the choice number, e.g. its index + 1.
  int choice_index = (iterator - choices_.begin()) + 1;
  char number[16];
  sprintf(number, "%d", choice_index);

  input_helper_.WithDefault(number);
  return *this;
}

ChoiceBuilder& ChoiceBuilder::WithInputSource(FILE* input_source) {
  input_helper_.WithInputSource(input_source);
  return *this;
}

ChoiceBuilder& ChoiceBuilder::WithTitle(const std::string& title) {
  title_ = title;
  return *this;
}

std::string ChoiceBuilder::MakeTitleWithOptions() {
  std::string title_with_options = title_;
  Choices::const_iterator iterator = choices_.begin();
  for (int number = 1; iterator != choices_.end(); ++iterator, ++number) {
    char buffer[128];
    sprintf(buffer, "\n  %d. %s", number, (*iterator).c_str());
    title_with_options += buffer;
  }
  return title_with_options;
}

Choices SplitChoices(const std::string& raw_choices) {
  Choices result;
  size_t current_pos = 0;
  size_t next_newline = 0;
  while ((next_newline = raw_choices.find('\n', current_pos)) !=
      std::string::npos) {
    std::string choice = raw_choices.substr(
        current_pos, next_newline - current_pos);
    result.push_back(choice);
    current_pos = next_newline + 1;
  }
  std::string last_choice = raw_choices.substr(current_pos);
  if (!last_choice.empty())
    result.push_back(last_choice);

  return result;
}

ChoiceBuilder FromChoices(const std::string& raw_choices) {
  return ChoiceBuilder(SplitChoices(raw_choices));
}

}  // namespace webrtc
