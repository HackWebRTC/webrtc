/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdio>
#include <memory>

#include "webrtc/modules/audio_processing/test/conversational_speech/config.h"
#include "webrtc/modules/audio_processing/test/conversational_speech/timing.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace test {
namespace {

using conversational_speech::LoadTiming;
using conversational_speech::SaveTiming;
using conversational_speech::Turn;

const char* const audiotracks_path = "/path/to/audiotracks";
const char* const timing_filepath = "/path/to/timing_file.txt";
const char* const output_path = "/path/to/output_dir";

const std::vector<Turn> expected_timing = {
    {"A", "a1", 0},
    {"B", "b1", 0},
    {"A", "a2", 100},
    {"B", "b2", -200},
    {"A", "a3", 0},
    {"A", "a4", 0},
};
const std::size_t kNumberOfTurns = expected_timing.size();

}  // namespace

TEST(ConversationalSpeechTest, Settings) {
  const conversational_speech::Config config(
      audiotracks_path, timing_filepath, output_path);

  // Test getters.
  EXPECT_EQ(audiotracks_path, config.audiotracks_path());
  EXPECT_EQ(timing_filepath, config.timing_filepath());
  EXPECT_EQ(output_path, config.output_path());
}

TEST(ConversationalSpeechTest, ExpectedTimingSize) {
  // Check the expected timing size.
  EXPECT_EQ(kNumberOfTurns, 6u);
}

TEST(ConversationalSpeechTest, TimingSaveLoad) {
  // Save test timing.
  const std::string temporary_filepath = webrtc::test::TempFilename(
      webrtc::test::OutputPath(), "TempTimingTestFile");
  SaveTiming(temporary_filepath, expected_timing);

  // Create a std::vector<Turn> instance by loading from file.
  std::vector<Turn> actual_timing = LoadTiming(temporary_filepath);
  std::remove(temporary_filepath.c_str());

  // Check size.
  EXPECT_EQ(expected_timing.size(), actual_timing.size());

  // Check Turn instances.
  for (size_t index = 0; index < expected_timing.size(); ++index) {
    EXPECT_EQ(expected_timing[index], actual_timing[index])
        << "turn #" << index << " not matching";
  }
}

}  // namespace test
}  // namespace webrtc
