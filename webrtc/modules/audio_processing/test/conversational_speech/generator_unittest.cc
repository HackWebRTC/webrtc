/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/test/conversational_speech/config.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace test {
namespace {

const char* const audiotracks_path = "/path/to/audiotracks";
const char* const timing_filepath = "/path/to/timing_file.txt";
const char* const output_path = "/path/to/output_dir";

}  // namespace

TEST(ConversationalSpeechTest, Settings) {
  conversational_speech::Config config(
      audiotracks_path, timing_filepath, output_path);

  // Test getters.
  EXPECT_EQ(config.audiotracks_path(), audiotracks_path);
  EXPECT_EQ(config.timing_filepath(), timing_filepath);
  EXPECT_EQ(config.output_path(), output_path);
}

}  // namespace test
}  // namespace webrtc
