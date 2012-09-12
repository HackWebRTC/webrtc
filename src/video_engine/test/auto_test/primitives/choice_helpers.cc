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

static const int kNoDefault = 0;

ChoiceBuilder::ChoiceBuilder(const Choices& choices)
    : choices_(choices), default_choice_(kNoDefault), input_source_(stdin) {
}

int ChoiceBuilder::Choose() {
  if (!title_.empty()) {
    printf("\n%s\n", title_.c_str());
  }

  Choices::const_iterator iterator = choices_.begin();
  for (int number = 1; iterator != choices_.end(); ++iterator, ++number)
    printf("  %d. %s\n", number, (*iterator).c_str());

  if (default_choice_ != kNoDefault)
    printf("  Hit enter for default (%s):\n", default_choice_text_.c_str());
  printf("# ");
  char input[8];
  fgets(input, 8, input_source_);
  int selection;
  if (input[0] == '\n')
    selection = default_choice_;
  else
    selection = atoi(input);

  if (selection < 1 || selection > static_cast<int>(choices_.size())) {
    printf("Please select one of the given options.\n");
    return Choose();
  }

  return selection;
}

ChoiceBuilder& ChoiceBuilder::WithDefault(const std::string& default_choice) {
  Choices::const_iterator iterator = std::find(
      choices_.begin(), choices_.end(), default_choice);
  assert(iterator != choices_.end() && "No such choice.");

  // Store the value as the choice number, e.g. its index + 1.
  default_choice_ = (iterator - choices_.begin()) + 1;
  default_choice_text_ = default_choice;
  return *this;
}

ChoiceBuilder& ChoiceBuilder::WithInputSource(FILE* input_source) {
  input_source_ = input_source;
  return *this;
}

ChoiceBuilder& ChoiceBuilder::WithTitle(const std::string& title) {
  title_ = title;
  return *this;
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
