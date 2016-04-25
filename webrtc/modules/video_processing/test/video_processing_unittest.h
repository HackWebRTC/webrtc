/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_TEST_VIDEO_PROCESSING_UNITTEST_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_TEST_VIDEO_PROCESSING_UNITTEST_H_

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

typedef struct {
  std::string file_name;
  int width;
  int height;
} VideoToTest;

class VideoProcessingTest : public ::testing::TestWithParam<VideoToTest> {
 protected:
  VideoProcessingTest();
  virtual void SetUp();
  virtual void TearDown();
  VideoProcessing* vp_;
  FILE* source_file_;
  VideoFrame video_frame_;
  VideoToTest vtt_;
  int width_;
  int half_width_;
  int height_;
  int size_y_;
  int size_uv_;
  size_t frame_length_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_TEST_VIDEO_PROCESSING_UNITTEST_H_
