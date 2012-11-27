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
using webrtc::FrameCutter;
using webrtc::kI420;
using webrtc::scoped_array;
using webrtc::test::OutputPath;
using webrtc::test::ResourcePath;

namespace webrtc {
namespace test {

const int width = 352;
const int height = 288;

const std::string ref_video = ResourcePath("foreman_cif", "yuv");
const std::string test_video = OutputPath() + "testvideo.yuv";

int num_bytes_read;

TEST(FrameCutterUnittest, ValidInPath) {
  const int first_frame_to_cut = 160;
  const int last_frame_to_cut = 240;

  int result = FrameCutter(ref_video, width, height, first_frame_to_cut,
                           last_frame_to_cut, test_video);
  EXPECT_EQ(0, result);

  FILE* ref_video_fid = fopen(ref_video.c_str(), "r");
  ASSERT_TRUE(ref_video_fid != NULL);
  FILE* test_video_fid = fopen(test_video.c_str(), "r");
  ASSERT_TRUE(test_video_fid != NULL);

  const int frame_size =CalcBufferSize(kI420, width, height);

  scoped_array<int> ref_buffer(new int[frame_size]);
  scoped_array<int> test_buffer(new int[frame_size]);

  for (int i = 0; i < first_frame_to_cut; ++i) {
    num_bytes_read = fread(ref_buffer.get(), frame_size, 1, ref_video_fid);
    EXPECT_EQ(frame_size, num_bytes_read);

    num_bytes_read = fread(test_buffer.get(), frame_size, 1, test_video_fid);
    EXPECT_EQ(frame_size, num_bytes_read);

    EXPECT_EQ(0, memcmp(ref_buffer.get(), test_buffer.get(), frame_size));
  }
  // Do not compare the frames that have been cut.
  for (int i = first_frame_to_cut; i <= last_frame_to_cut; ++i) {
    num_bytes_read = fread(&ref_buffer, frame_size, 1, ref_video_fid);
    EXPECT_EQ(frame_size, num_bytes_read);
  }

  while (!feof(test_video_fid)) {
    num_bytes_read = fread(&ref_buffer, frame_size, 1, ref_video_fid);
    EXPECT_EQ(frame_size, num_bytes_read);
    num_bytes_read = fread(&test_buffer, frame_size, 1, test_video_fid);
    EXPECT_EQ(frame_size, num_bytes_read);
    EXPECT_EQ(0, memcmp(ref_buffer.get(), test_buffer.get(), frame_size));
  }
  bool are_both_files_at_the_end =
      (feof(test_video_fid) && feof(test_video_fid));
  EXPECT_TRUE(are_both_files_at_the_end);
}

TEST(FrameCutterUnittest, EmptySetToCut) {
  int first_frame_to_cut = 2;
  int last_frame_to_cut = 1;

  int result = FrameCutter(ref_video, width, height, first_frame_to_cut,
                           last_frame_to_cut, test_video);
  EXPECT_EQ(-10, result);
}

TEST(FrameCutterUnittest, InValidInPath) {
  const std::string ref_video = "PATH/THAT/DOES/NOT/EXIST";

  int first_frame_to_cut = 30;
  int last_frame_to_cut = 120;

  int result = FrameCutter(ref_video, width, height, first_frame_to_cut,
                           last_frame_to_cut, test_video);
  EXPECT_EQ(-11, result);
}
}
}
