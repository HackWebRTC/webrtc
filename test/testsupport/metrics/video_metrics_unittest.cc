/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_metrics.h"

#include <cstdio>

#include "gtest/gtest.h"
#include "testsupport/fileutils.h"

namespace webrtc {

static const char* kEmptyFileName = "video_metrics_unittest_empty_file.tmp";
static const char* kNonExistingFileName = "video_metrics_unittest_non_existing";
static const int kWidth = 352;
static const int kHeight = 288;

static const int kMissingReferenceFileReturnCode = -1;
static const int kMissingTestFileReturnCode = -2;
static const int kEmptyFileReturnCode = -3;
static const double kPsnrPerfectResult =  std::numeric_limits<double>::max();
static const double kSsimPerfectResult = 1.0;

class VideoMetricsTest: public testing::Test {
 protected:
  VideoMetricsTest() {
    video_file = webrtc::test::ProjectRootPath() + "resources/foreman_cif.yuv";
  }
  virtual ~VideoMetricsTest() {}
  void SetUp() {
    // Create an empty file:
    FILE* dummy = fopen(kEmptyFileName, "wb");
    fclose(dummy);
  }
  void TearDown() {
    std::remove(kEmptyFileName);
  }
  QualityMetricsResult result_;
  std::string video_file;
};

// Tests that it is possible to run with the same reference as test file
TEST_F(VideoMetricsTest, ReturnsPerfectResultForIdenticalFiles) {
  EXPECT_EQ(0, PsnrFromFiles(video_file.c_str(), video_file.c_str(),
                             kWidth, kHeight, &result_));
  EXPECT_EQ(kPsnrPerfectResult, result_.average);
  EXPECT_EQ(SsimFromFiles(video_file.c_str(), video_file.c_str(), kWidth, kHeight,
                          &result_), 0);
  EXPECT_EQ(kSsimPerfectResult, result_.average);
}

// Tests that the right return code is given when the reference file is missing.
TEST_F(VideoMetricsTest, MissingReferenceFile) {
  EXPECT_EQ(kMissingReferenceFileReturnCode,
            PsnrFromFiles(kNonExistingFileName, video_file.c_str(), kWidth,
                          kHeight, &result_));
  EXPECT_EQ(kMissingReferenceFileReturnCode,
            SsimFromFiles(kNonExistingFileName, video_file.c_str(), kWidth,
                          kHeight, &result_));
}

// Tests that the right return code is given when the test file is missing.
TEST_F(VideoMetricsTest, MissingTestFile) {
  EXPECT_EQ(kMissingTestFileReturnCode,
            PsnrFromFiles(video_file.c_str(), kNonExistingFileName, kWidth,
                          kHeight, &result_));
  EXPECT_EQ(kMissingTestFileReturnCode,
            SsimFromFiles(video_file.c_str(), kNonExistingFileName, kWidth,
                          kHeight, &result_));
}

// Tests that the method can be executed with empty files.
TEST_F(VideoMetricsTest, EmptyFiles) {
  EXPECT_EQ(kEmptyFileReturnCode,
            PsnrFromFiles(kEmptyFileName, video_file.c_str(), kWidth, kHeight,
                          &result_));
  EXPECT_EQ(kEmptyFileReturnCode,
            SsimFromFiles(kEmptyFileName, video_file.c_str(), kWidth, kHeight,
                          &result_));
  // Run the same again with the empty file switched.
  EXPECT_EQ(kEmptyFileReturnCode,
            PsnrFromFiles(video_file.c_str(), kEmptyFileName, kWidth, kHeight,
                          &result_));
  EXPECT_EQ(kEmptyFileReturnCode,
            SsimFromFiles(video_file.c_str(), kEmptyFileName, kWidth, kHeight,
                          &result_));
}

}  // namespace webrtc

