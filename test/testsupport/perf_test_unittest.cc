/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/perf_test.h"

#include <string>

#include "test/gtest.h"

namespace webrtc {
namespace test {

TEST(PerfTest, AppendResult) {
  testing::internal::CaptureStdout();
  std::string expected = "RESULT measurementmodifier: trace= 42 units\n";
  PrintResult("measurement", "modifier", "trace", 42, "units", false);

  expected += "*RESULT foobar: baz= 7 widgets\n";
  PrintResult( "foo", "bar", "baz", 7, "widgets", true);

  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(expected, output);
}

}  // namespace test
}  // namespace webrtc
