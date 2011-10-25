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

#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "vie_autotest.h"
#include "vie_autotest_window_manager_interface.h"
#include "vie_integration_test_base.h"
#include "vie_window_creator.h"

namespace {

// Define flag validators for our flags:
static bool ValidatePath(const char* flag_name, const std::string& value) {
  return !value.empty();
}

static bool ValidateDimension(const char* flag_name, WebRtc_Word32 value) {
  if (value <= 0) {
    return false;
  }
  return true;
}

// Define the flags themselves:
DEFINE_string(i420_test_video_path, "", "Path to an i420-coded raw video file"
              " to use for the test. This file is fed into the fake camera"
              " and will therefore be what the camera 'sees'.");
static const bool dummy1 =
    google::RegisterFlagValidator(&FLAGS_i420_test_video_path,
                                  &ValidatePath);

DEFINE_int32(i420_test_video_width, 0, "The width of the provided video.");
static const bool dummy2 =
    google::RegisterFlagValidator(&FLAGS_i420_test_video_width,
                                  &ValidateDimension);
DEFINE_int32(i420_test_video_height, 0, "The height of the provided video.");
static const bool dummy3 =
    google::RegisterFlagValidator(&FLAGS_i420_test_video_height,
                                  &ValidateDimension);

class ViEStandardIntegrationTest: public ViEIntegrationTest {
};

TEST_F(ViEStandardIntegrationTest, RunsBaseTestWithoutErrors)  {
  tests_->ViEAutomatedBaseStandardTest(FLAGS_i420_test_video_path,
                                       FLAGS_i420_test_video_width,
                                       FLAGS_i420_test_video_height);
}

TEST_F(ViEStandardIntegrationTest, RunsCodecTestWithoutErrors)  {
  tests_->ViEAutomatedCodecStandardTest(FLAGS_i420_test_video_path,
                                        FLAGS_i420_test_video_width,
                                        FLAGS_i420_test_video_height);
}

// These tests still require a physical camera:
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
