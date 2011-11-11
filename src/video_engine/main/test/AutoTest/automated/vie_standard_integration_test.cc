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

namespace {

// We limit the input video size in order to not use to much bandwidth for
// I420 transfer.
const int MAX_INPUT_VIDEO_WIDTH = 200;

// Define flag validators for our flags:
static bool ValidatePath(const char* flag_name, const std::string& value) {
  return !value.empty();
}

static bool ValidateDimension(const char* flag_name, WebRtc_Word32 value) {
  if (value <= 0 || value > MAX_INPUT_VIDEO_WIDTH) {
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

DEFINE_int32(i420_test_video_width, 0, "The width of the provided video."
             " This cannot be larger than 200 due to bandwidth concerns");
static const bool dummy2 =
    google::RegisterFlagValidator(&FLAGS_i420_test_video_width,
                                  &ValidateDimension);
DEFINE_int32(i420_test_video_height, 0, "The height of the provided video."
             " This cannot be larger than 200 due to bandwidth concerns");
static const bool dummy3 =
    google::RegisterFlagValidator(&FLAGS_i420_test_video_height,
                                  &ValidateDimension);

class ViEStandardIntegrationTest: public ViEIntegrationTest {
 public:
  void SetUp() {
    std::string test_case_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();

    std::string local_preview_filename = test_case_name + "-local-preview.yuv";
    std::string remote_filename = test_case_name + "-remote.yuv";

    if (!local_file_renderer_.PrepareForRendering(local_preview_filename)) {
      FAIL() << "Could not open output file " << local_preview_filename <<
          " for writing.";
    }
    if (!remote_file_renderer_.PrepareForRendering(remote_filename)) {
      FAIL() << "Could not open output file " << remote_filename <<
          " for writing.";
    }
  }

  void TearDown() {
    local_file_renderer_.StopRendering();
    remote_file_renderer_.StopRendering();

    bool test_failed = ::testing::UnitTest::GetInstance()->
        current_test_info()->result()->Failed();

    if (test_failed) {
      // Leave the files for analysis if the test failed
      local_file_renderer_.SaveOutputFile("failed-");
      remote_file_renderer_.SaveOutputFile("failed-");
    } else {
      // No reason to keep the files if we succeeded
      local_file_renderer_.DeleteOutputFile();
      remote_file_renderer_.DeleteOutputFile();
    }
  }
 protected:
  ViEToFileRenderer local_file_renderer_;
  ViEToFileRenderer remote_file_renderer_;
};

TEST_F(ViEStandardIntegrationTest, RunsBaseStandardTestWithoutErrors)  {
  tests_->ViEAutomatedBaseStandardTest(FLAGS_i420_test_video_path,
                                       FLAGS_i420_test_video_width,
                                       FLAGS_i420_test_video_height,
                                       &local_file_renderer_,
                                       &remote_file_renderer_);

  QualityMetricsResult psnr_result;
  int psnr_error = PsnrFromFiles(FLAGS_i420_test_video_path.c_str(),
                                 remote_file_renderer_.output_filename().c_str(),
                                 FLAGS_i420_test_video_width,
                                 FLAGS_i420_test_video_height,
                                 &psnr_result);
  ASSERT_EQ(0, psnr_error) << "The PSNR routine failed - output files missing?";
  ASSERT_GT(psnr_result.average, 25);  // That is, we want at least 25 dB

  QualityMetricsResult ssim_result;
  int ssim_error = SsimFromFiles(FLAGS_i420_test_video_path.c_str(),
                                 remote_file_renderer_.output_filename().c_str(),
                                 FLAGS_i420_test_video_width,
                                 FLAGS_i420_test_video_height,
                                 &ssim_result);
  ASSERT_EQ(0, ssim_error) << "The SSIM routine failed - output files missing?";
  ASSERT_GT(ssim_result.average, 0.85f);  // 1 = perfect, -1 = terrible
}

TEST_F(ViEStandardIntegrationTest, RunsCodecTestWithoutErrors)  {
  tests_->ViEAutomatedCodecStandardTest(FLAGS_i420_test_video_path,
                                        FLAGS_i420_test_video_width,
                                        FLAGS_i420_test_video_height,
                                        &local_file_renderer_,
                                        &remote_file_renderer_);

  // We compare the left and right here instead of with the original. The reason
  // is that it is hard to say when the three consecutive tests switch over into
  // each other, at which point we would have to restart the original to get a
  // fair comparison.
  QualityMetricsResult psnr_result;
  int psnr_error = PsnrFromFiles(local_file_renderer_.output_filename().c_str(),
                                 remote_file_renderer_.output_filename().c_str(),
                                 FLAGS_i420_test_video_width,
                                 FLAGS_i420_test_video_height,
                                 &psnr_result);
  ASSERT_EQ(0, psnr_error) << "The PSNR routine failed - output files missing?";
  // This test includes VP8 which is a bit lossy. According to Wikipedia between
  // 20-25 is considered OK for transmission codecs and we seem to be getting
  // like 21 so 20 seems like a good threshold value here.
  EXPECT_GT(psnr_result.average, 20);

  QualityMetricsResult ssim_result;
  int ssim_error = SsimFromFiles(local_file_renderer_.output_filename().c_str(),
                                 remote_file_renderer_.output_filename().c_str(),
                                 FLAGS_i420_test_video_width,
                                 FLAGS_i420_test_video_height,
                                 &ssim_result);
  ASSERT_EQ(0, ssim_error) << "The SSIM routine failed - output files missing?";
  EXPECT_GT(ssim_result.average, 0.8f);  // 1 = perfect, -1 = terrible
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
