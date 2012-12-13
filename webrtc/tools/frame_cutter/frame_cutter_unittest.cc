/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <cstdlib>

#include "gtest/gtest.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/tools/frame_cutter/frame_cutter_lib.h"

using webrtc::CalcBufferSize;
using webrtc::CutFrames;
using webrtc::kI420;
using webrtc::scoped_array;
using webrtc::test::OutputPath;
using webrtc::test::ResourcePath;

namespace webrtc {
namespace test {

const int kWidth = 352;
const int kHeight = 288;

const std::string kRefVideo = ResourcePath("foreman_cif", "yuv");
const std::string kTestVideo = OutputPath() + "testvideo.yuv";

int num_bytes_read;

TEST(CutFramesUnittest, ValidInPath) {
  const int kFirstFrameToCut = 160;
  const int kInterval = 1;
  const int kLastFrameToCut = 240;

  int result = CutFrames(kRefVideo, kWidth, kHeight, kFirstFrameToCut,
                           kInterval, kLastFrameToCut, kTestVideo);
  EXPECT_EQ(0, result);

  FILE* ref_video_fid = fopen(kRefVideo.c_str(), "rb");
  ASSERT_TRUE(ref_video_fid != NULL);
  FILE* test_video_fid = fopen(kTestVideo.c_str(), "rb");
  ASSERT_TRUE(test_video_fid != NULL);

  const int kFrameSize = CalcBufferSize(kI420, kWidth, kHeight);

  scoped_array<int> ref_buffer(new int[kFrameSize]);
  scoped_array<int> test_buffer(new int[kFrameSize]);

  for (int i = 1; i < kFirstFrameToCut; ++i) {
    num_bytes_read = fread(ref_buffer.get(), 1, kFrameSize, ref_video_fid);
    EXPECT_EQ(kFrameSize, num_bytes_read);

    num_bytes_read = fread(test_buffer.get(), 1, kFrameSize, test_video_fid);
    EXPECT_EQ(kFrameSize, num_bytes_read);

    EXPECT_EQ(0, memcmp(ref_buffer.get(), test_buffer.get(), kFrameSize));
  }
  // Do not compare the frames that have been cut.
  for (int i = kFirstFrameToCut; i <= kLastFrameToCut; ++i) {
    num_bytes_read = fread(ref_buffer.get(), 1, kFrameSize, ref_video_fid);
    EXPECT_EQ(kFrameSize, num_bytes_read);
  }

  while (!feof(test_video_fid) && !feof(ref_video_fid)) {
    num_bytes_read = fread(ref_buffer.get(), 1, kFrameSize, ref_video_fid);
    if (!feof(ref_video_fid)) {
      EXPECT_EQ(kFrameSize, num_bytes_read);
    }
    num_bytes_read = fread(test_buffer.get(), 1, kFrameSize, test_video_fid);
    if (!feof(test_video_fid)) {
      EXPECT_EQ(kFrameSize, num_bytes_read);
    }
    if (!feof(test_video_fid) && !feof(test_video_fid)) {
      EXPECT_EQ(0, memcmp(ref_buffer.get(), test_buffer.get(), kFrameSize));
    }
  }
  fclose(ref_video_fid);
  fclose(test_video_fid);
}

TEST(CutFramesUnittest, EmptySetToCut) {
  const int kFirstFrameToCut = 2;
  const int kInterval = 1;
  const int kLastFrameToCut = 1;

  int result = CutFrames(kRefVideo, kWidth, kHeight, kFirstFrameToCut,
                           kInterval, kLastFrameToCut, kTestVideo);
  EXPECT_EQ(-10, result);
}

TEST(CutFramesUnittest, InValidInPath) {
  const std::string kRefVideo = "PATH/THAT/DOES/NOT/EXIST";

  const int kFirstFrameToCut = 30;
  const int kInterval = 1;
  const int kLastFrameToCut = 120;

  int result = CutFrames(kRefVideo, kWidth, kHeight, kFirstFrameToCut,
                           kInterval, kLastFrameToCut, kTestVideo);
  EXPECT_EQ(-11, result);
}

TEST(CutFramesUnitTest, DeletingEverySecondFrame) {
  const int kFirstFrameToCut = 1;
  const int kInterval = 2;
  const int kLastFrameToCut = 10000;
  // Set kLastFrameToCut to a large value so that all frame are processed.
  int result = CutFrames(kRefVideo, kWidth, kHeight, kFirstFrameToCut,
                           kInterval, kLastFrameToCut, kTestVideo);
  EXPECT_EQ(0, result);

  FILE* original_fid = fopen(kRefVideo.c_str(), "rb");
  ASSERT_TRUE(original_fid != NULL);
  FILE* edited_fid = fopen(kTestVideo.c_str(), "rb");
  ASSERT_TRUE(edited_fid != NULL);

  const int kFrameSize = CalcBufferSize(kI420, kWidth, kHeight);

  scoped_array<int> original_buffer(new int[kFrameSize]);
  scoped_array<int> edited_buffer(new int[kFrameSize]);

  int num_frames_read = 0;

  while (!feof(original_fid) && !feof(edited_fid)) {
    num_bytes_read =
        fread(original_buffer.get(), 1, kFrameSize, original_fid);
    if (!feof(original_fid)) {
      EXPECT_EQ(kFrameSize, num_bytes_read);
      num_frames_read++;
    }
    if (num_frames_read % kInterval != 0) {
      num_bytes_read = fread(edited_buffer.get(), 1, kFrameSize, edited_fid);
      if (!feof(edited_fid)) {
        EXPECT_EQ(kFrameSize, num_bytes_read);
      }
      if (!feof(original_fid) && !feof(edited_fid)) {
        EXPECT_EQ(0, memcmp(original_buffer.get(),
                            edited_buffer.get(), kFrameSize));
      }
    }
  }
}
}
}
