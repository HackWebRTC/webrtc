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

#include <cstdio>

#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "testsupport/metrics/video_metrics.h"
#include "vie_autotest.h"
#include "vie_autotest_window_manager_interface.h"
#include "vie_integration_test_base.h"
#include "vie_to_file_renderer.h"
#include "vie_window_creator.h"
#include "testsupport/metrics/video_metrics.h"

namespace {

class ViEStandardIntegrationTest: public ViEIntegrationTest {
};

TEST_F(ViEStandardIntegrationTest, RunsBaseTestWithoutErrors)  {
  ASSERT_EQ(0, tests_->ViEBaseStandardTest());
}

TEST_F(ViEStandardIntegrationTest, RunsCodecTestWithoutErrors)  {
  ASSERT_EQ(0, tests_->ViECodecStandardTest());
}

TEST_F(ViEStandardIntegrationTest, RunsCaptureTestWithoutErrors)  {
  ASSERT_EQ(0, tests_->ViECaptureStandardTest());
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

} // namespace
