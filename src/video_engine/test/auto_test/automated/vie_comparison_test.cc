/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "testsupport/metrics/video_metrics.h"
#include "vie_autotest.h"
#include "vie_comparison_tests.h"
#include "vie_integration_test_base.h"
#include "vie_to_file_renderer.h"

namespace {

// The input file must be QCIF since I420 gets scaled to that in the tests
// (it is so bandwidth-heavy we have no choice). Our comparison algorithms
// wouldn't like scaling, so this will work when we compare with the original.
const std::string input_file = ViETest::GetResultOutputPath() +
    "resources/paris_qcif.yuv";
const int input_width = 176;
const int input_height = 144;

class ViEComparisonTest: public testing::Test {
 public:
  void SetUp() {
    std::string test_case_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();

    std::string output_path = ViETest::GetResultOutputPath();
    std::string local_preview_filename =
        test_case_name + "-local-preview.yuv";
    std::string remote_filename =
        test_case_name + "-remote.yuv";

    if (!local_file_renderer_.PrepareForRendering(output_path,
                                                  local_preview_filename)) {
      FAIL() << "Could not open output file " << output_path <<
          local_preview_filename << " for writing.";
    }
    if (!remote_file_renderer_.PrepareForRendering(output_path,
                                                   remote_filename)) {
      FAIL() << "Could not open output file " << output_path <<
          remote_filename << " for writing.";
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

  ViEComparisonTests tests_;
};

TEST_F(ViEComparisonTest, RunsBaseStandardTestWithoutErrors)  {
  ASSERT_TRUE(tests_.TestCallSetup(input_file, input_width, input_height,
                                   &local_file_renderer_,
                                   &remote_file_renderer_));

  QualityMetricsResult psnr_result;
  int psnr_error = PsnrFromFiles(
      input_file.c_str(), remote_file_renderer_.GetFullOutputPath().c_str(),
      input_width, input_height, &psnr_result);

  ASSERT_EQ(0, psnr_error) << "The PSNR routine failed - output files missing?";
  ASSERT_GT(psnr_result.average, 28);  // That is, we want at least 28 dB

  QualityMetricsResult ssim_result;
  int ssim_error = SsimFromFiles(
      input_file.c_str(), remote_file_renderer_.GetFullOutputPath().c_str(),
      input_width, input_height, &ssim_result);

  ASSERT_EQ(0, ssim_error) << "The SSIM routine failed - output files missing?";
  ASSERT_GT(ssim_result.average, 0.95f);  // 1 = perfect, -1 = terrible

  ViETest::Log("Results: PSNR %f SSIM %f",
                 psnr_result.average, ssim_result.average);
}

TEST_F(ViEComparisonTest, RunsCodecTestWithoutErrors)  {
  ASSERT_TRUE(tests_.TestCodecs(input_file, input_width, input_height,
                                &local_file_renderer_,
                                &remote_file_renderer_));

  // We compare the local and remote here instead of with the original.
  // The reason is that it is hard to say when the three consecutive tests
  // switch over into each other, at which point we would have to restart the
  // original to get a fair comparison.
  QualityMetricsResult psnr_result;
  int psnr_error = PsnrFromFiles(
      input_file.c_str(),
      remote_file_renderer_.GetFullOutputPath().c_str(),
      input_width, input_height, &psnr_result);

  ASSERT_EQ(0, psnr_error) << "The PSNR routine failed - output files missing?";
  // TODO(phoglund): This value should be higher. Investigate why the remote
  // file turns out 6 seconds shorter than the local file (frame dropping?...)
  EXPECT_GT(psnr_result.average, 20);

  QualityMetricsResult ssim_result;
  int ssim_error = SsimFromFiles(
      local_file_renderer_.GetFullOutputPath().c_str(),
      remote_file_renderer_.GetFullOutputPath().c_str(),
      input_width, input_height, &ssim_result);

  // TODO(phoglund): This value should also be higher.
  ASSERT_EQ(0, ssim_error) << "The SSIM routine failed - output files missing?";
  EXPECT_GT(ssim_result.average, 0.7f);  // 1 = perfect, -1 = terrible

  ViETest::Log("Results: PSNR %f SSIM %f",
               psnr_result.average, ssim_result.average);
}

}  // namespace
