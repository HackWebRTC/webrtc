/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/y4m_file_reader.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

class Y4mFileReaderTest : public ::testing::Test {
 public:
  void SetUp() override {
    const std::string filename =
        TempFilename(webrtc::test::OutputPath(), "test_video_file.y4m");

    // Create simple test video of size 6x4.
    FILE* file = fopen(filename.c_str(), "wb");
    ASSERT_TRUE(file != nullptr);
    fprintf(file, "YUV4MPEG2 W6 H4 F57:10 C420 dummyParam\n");
    fprintf(file, "FRAME\n");

    const int width = 6;
    const int height = 4;
    const int i40_size = width * height * 3 / 2;
    for (int i = 0; i < i40_size; ++i)
      fputc(static_cast<char>(i), file);
    fprintf(file, "FRAME\n");
    for (int i = 0; i < i40_size; ++i)
      fputc(static_cast<char>(i + i40_size), file);
    fclose(file);

    // Open the newly created file.
    video = webrtc::test::Y4mFile::Open(filename);
    ASSERT_TRUE(video);
  }

  rtc::scoped_refptr<webrtc::test::Y4mFile> video;
};

TEST_F(Y4mFileReaderTest, TestParsingFileHeader) {
  EXPECT_EQ(6, video->width());
  EXPECT_EQ(4, video->height());
  EXPECT_EQ(5.7f, video->fps());
}

TEST_F(Y4mFileReaderTest, TestParsingNumberOfFrames) {
  EXPECT_EQ(2u, video->number_of_frames());
}

TEST_F(Y4mFileReaderTest, TestPixelContent) {
  int cnt = 0;
  for (const rtc::scoped_refptr<I420BufferInterface> frame : *video) {
    for (int i = 0; i < 6 * 4; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataY()[i]);
    for (int i = 0; i < 3 * 2; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataU()[i]);
    for (int i = 0; i < 3 * 2; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataV()[i]);
  }
}

}  // namespace test
}  // namespace webrtc
