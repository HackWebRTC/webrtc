/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_standard_test.h"

// These symbols clash with gtest, so undef them:
#undef TEST
#undef ASSERT_TRUE
#undef ASSERT_FALSE

#include "gtest/gtest.h"

namespace {

class VoEStandardIntegrationTest: public testing::Test {
 public:
  virtual ~VoEStandardIntegrationTest() {}

  // Initializes the test manager.
  virtual void SetUp() {
    ASSERT_TRUE(test_manager_.Init());
    test_manager_.GetInterfaces();
  }

  // Releases anything allocated by SetUp.
  virtual void TearDown() {
    ASSERT_EQ(0, test_manager_.ReleaseInterfaces());
  }

 protected:
  voetest::VoETestManager test_manager_;
};

TEST_F(VoEStandardIntegrationTest, RunsStandardTestWithoutErrors) {
  ASSERT_EQ(0, test_manager_.DoStandardTest());
}

} // namespace
