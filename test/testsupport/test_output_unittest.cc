/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/test_output.h"

#include <string.h>

#include <string>

#include "rtc_base/file.h"
#include "rtc_base/flags.h"
#include "rtc_base/pathutils.h"
#include "rtc_base/platform_file.h"
#include "test/gtest.h"

DECLARE_string(test_output_dir);

namespace webrtc {
namespace test {

TEST(IsolatedOutputTest, ShouldRejectInvalidIsolatedOutDir) {
  const char* backup = FLAG_test_output_dir;
  FLAG_test_output_dir = "";
  ASSERT_FALSE(WriteToTestOutput("a-file", "some-contents"));
  FLAG_test_output_dir = backup;
}

TEST(IsolatedOutputTest, ShouldRejectInvalidFileName) {
  ASSERT_FALSE(WriteToTestOutput(nullptr, "some-contents"));
  ASSERT_FALSE(WriteToTestOutput("", "some-contents"));
}

// Sets isolated_out_dir=<a-writable-path> to execute this test.
TEST(IsolatedOutputTest, ShouldBeAbleToWriteContent) {
  const char* filename = "a-file";
  const char* content = "some-contents";
  if (WriteToTestOutput(filename, content)) {
    rtc::Pathname out_file(FLAG_test_output_dir, filename);
    rtc::File input = rtc::File::Open(out_file);
    EXPECT_TRUE(input.IsOpen());
    EXPECT_TRUE(input.Seek(0));
    uint8_t buffer[32];
    EXPECT_EQ(input.Read(buffer, strlen(content)), strlen(content));
    buffer[strlen(content)] = 0;
    EXPECT_EQ(std::string(content),
              std::string(reinterpret_cast<char*>(buffer)));
    input.Close();

    EXPECT_TRUE(rtc::File::Remove(out_file));
  }
}

}  // namespace test
}  // namespace webrtc
