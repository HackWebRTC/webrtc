/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file contains the "standard" suite of integration tests, implemented
 * as a GUnit test. This file is a part of the effort to try to automate all
 * tests in this section of the code. Currently, this code makes no attempt
 * to verify any video output - it only checks for direct errors.
 */

#include "gtest/gtest.h"
#include "vie_autotest.h"
#include "vie_autotest_window_manager_interface.h"
#include "vie_window_creator.h"

namespace {

class ViEStandardIntegrationTest: public testing::Test {
 public:
  static void SetUpTestCase() {
    window_creator_ = new ViEWindowCreator();

    ViEAutoTestWindowManagerInterface* window_manager =
        window_creator_->CreateTwoWindows();

    // Create the test cases
    tests_ = new ViEAutoTest(window_manager->GetWindow1(),
                             window_manager->GetWindow2());
  }

  static void TearDownTestCase() {
    window_creator_->TerminateWindows();

    delete tests_;
    delete window_creator_;
  }

 protected:
  static ViEWindowCreator* window_creator_;
  static ViEAutoTest* tests_;
};

TEST_F(ViEStandardIntegrationTest, RunsBaseStandardTestWithoutErrors)  {
  ASSERT_EQ(0, tests_->ViEBaseStandardTest());
}

TEST_F(ViEStandardIntegrationTest, RunsCaptureTestWithoutErrors)  {
  ASSERT_EQ(0, tests_->ViECaptureStandardTest());
}

TEST_F(ViEStandardIntegrationTest, RunsCodecTestWithoutErrors)  {
  ASSERT_EQ(0, tests_->ViECodecStandardTest());
}

TEST_F(ViEStandardIntegrationTest, RunsEncryptionTestWithoutErrors)  {
  ASSERT_EQ(0, tests_->ViEEncryptionStandardTest());
}

TEST_F(ViEStandardIntegrationTest, RunsFileTestWithoutErrors)  {
  ASSERT_EQ(0, tests_->ViEFileStandardTest());
}

TEST_F(ViEStandardIntegrationTest, RunsImageProcessTestWithoutErrors)  {
  ASSERT_EQ(0, tests_->ViEImageProcessStandardTest());
}

TEST_F(ViEStandardIntegrationTest, RunsNetworkTestWithoutErrors)  {
  ASSERT_EQ(0, tests_->ViENetworkStandardTest());
}

TEST_F(ViEStandardIntegrationTest, RunsRenderTestWithoutErrors)  {
  ASSERT_EQ(0, tests_->ViERenderStandardTest());
}

TEST_F(ViEStandardIntegrationTest, RunsRtpRctpTestWithoutErrors)  {
  ASSERT_EQ(0, tests_->ViERtpRtcpStandardTest());
}

ViEAutoTest* ViEStandardIntegrationTest::tests_ = NULL;
ViEWindowCreator* ViEStandardIntegrationTest::window_creator_ = NULL;

}
