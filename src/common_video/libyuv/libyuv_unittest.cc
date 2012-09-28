/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <string.h>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "gtest/gtest.h"
#include "modules/interface/module_common_types.h"  // VideoFrame
#include "system_wrappers/interface/tick_util.h"
#include "testsupport/fileutils.h"

namespace webrtc {

int PrintBuffer(const uint8_t* buffer, int width, int height) {
  if (buffer == NULL)
    return -1;
  int k = 0;
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      printf("%d ", buffer[k++]);
    }
    printf(" \n");
  }
  printf(" \n");
  return 0;
}


int PrintFrame(const VideoFrame* frame, const char* str) {
  if (frame == NULL)
     return -1;
  printf("%s %dx%d \n", str, frame->Width(), frame->Height());

  int ret = 0;
  int width = frame->Width();
  int height = frame->Height();
  ret += PrintBuffer(frame->Buffer(), width, height);
  int half_width = (frame->Width() + 1) / 2;
  int half_height = (frame->Height() + 1) / 2;
  ret += PrintBuffer(frame->Buffer() + width * height, half_width, half_height);
  ret += PrintBuffer(frame->Buffer() + width * height +
                     half_width * half_height, half_width, half_height);
  return ret;
}


// Create an image from on a YUV frame. Every plane value starts with a start
// value, and will be set to increasing values.
// plane_offset - prep for PlaneType.
void CreateImage(VideoFrame* frame, int plane_offset[3]) {
  if (frame == NULL)
    return;
  int width = frame->Width();
  int height = frame->Height();
  int half_width = (frame->Width() + 1) / 2;
  int half_height = (frame->Height() + 1) / 2;
  uint8_t *data = frame->Buffer();
  // Y plane.
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      *data = static_cast<uint8_t>((i + plane_offset[0]) + j);
      data++;
    }
  }
  // U plane.
  for (int i = 0; i < half_height; i++) {
    for (int j = 0; j < half_width; j++) {
      *data = static_cast<uint8_t>((i + plane_offset[1]) + j);
      data++;
    }
  }
  // V Plane.
  for (int i = 0; i < half_height; i++) {
    for (int j = 0; j < half_width; j++) {
      *data = static_cast<uint8_t>((i + plane_offset[2]) + j);
      data++;
    }
  }
}

class TestLibYuv : public ::testing::Test {
 protected:
  TestLibYuv();
  virtual void SetUp();
  virtual void TearDown();

  FILE* source_file_;
  const int width_;
  const int height_;
  const int frame_length_;
};

// TODO (mikhal): Use scoped_ptr when handling buffers.
TestLibYuv::TestLibYuv()
    : source_file_(NULL),
      width_(352),
      height_(288),
      frame_length_(CalcBufferSize(kI420, 352, 288)) {
}

void TestLibYuv::SetUp() {
  const std::string input_file_name = webrtc::test::ProjectRootPath() +
                                      "resources/foreman_cif.yuv";
  source_file_  = fopen(input_file_name.c_str(), "rb");
  ASSERT_TRUE(source_file_ != NULL) << "Cannot read file: "<<
                                       input_file_name << "\n";
}

void TestLibYuv::TearDown() {
  if (source_file_ != NULL) {
    ASSERT_EQ(0, fclose(source_file_));
  }
  source_file_ = NULL;
}

TEST_F(TestLibYuv, ConvertSanityTest) {
  // TODO(mikhal)
}

TEST_F(TestLibYuv, ConvertTest) {
  // Reading YUV frame - testing on the first frame of the foreman sequence
  int j = 0;
  std::string output_file_name = webrtc::test::OutputPath() +
                                 "LibYuvTest_conversion.yuv";
  FILE*  output_file = fopen(output_file_name.c_str(), "wb");
  ASSERT_TRUE(output_file != NULL);

  double psnr = 0;

  uint8_t* orig_buffer = new uint8_t[frame_length_];
  EXPECT_GT(fread(orig_buffer, 1, frame_length_, source_file_), 0U);

  // printf("\nConvert #%d I420 <-> RGB24\n", j);
  uint8_t* res_rgb_buffer2  = new uint8_t[width_ * height_ * 3];
  VideoFrame res_i420_frame;
  res_i420_frame.VerifyAndAllocate(frame_length_);
  res_i420_frame.SetHeight(height_);
  res_i420_frame.SetWidth(width_);
  EXPECT_EQ(0, ConvertFromI420(orig_buffer, width_, kRGB24, 0,
                               width_, height_, res_rgb_buffer2));

  EXPECT_EQ(0, ConvertToI420(kRGB24, res_rgb_buffer2, 0, 0, width_, height_,
                             0, kRotateNone, &res_i420_frame));

  if (fwrite(res_i420_frame.Buffer(), 1, frame_length_,
             output_file) != static_cast<unsigned int>(frame_length_)) {
    return;
  }
  psnr = I420PSNR(orig_buffer, res_i420_frame.Buffer(), width_, height_);
  // Optimization Speed- quality trade-off => 45 dB only (platform dependant).
  EXPECT_GT(ceil(psnr), 44);
  j++;
  delete [] res_rgb_buffer2;

  // printf("\nConvert #%d I420 <-> UYVY\n", j);
  uint8_t* out_uyvy_buffer = new uint8_t[width_ * height_ * 2];
  EXPECT_EQ(0, ConvertFromI420(orig_buffer, width_,
                               kUYVY, 0, width_, height_, out_uyvy_buffer));
  EXPECT_EQ(0, ConvertToI420(kUYVY, out_uyvy_buffer, 0, 0, width_, height_,
            0, kRotateNone, &res_i420_frame));
  psnr = I420PSNR(orig_buffer, res_i420_frame.Buffer(), width_, height_);
  EXPECT_EQ(48.0, psnr);
  if (fwrite(res_i420_frame.Buffer(), 1, frame_length_,
             output_file) !=  static_cast<unsigned int>(frame_length_)) {
    return;
  }

  j++;
  delete [] out_uyvy_buffer;

  // printf("\nConvert #%d I420 <-> I420 \n", j);
 uint8_t* out_i420_buffer = new uint8_t[width_ * height_ * 3 / 2 ];
  EXPECT_EQ(0, ConvertToI420(kI420, orig_buffer, 0, 0, width_, height_,
                             0, kRotateNone, &res_i420_frame));
  EXPECT_EQ(0, ConvertFromI420(res_i420_frame.Buffer(), width_, kI420, 0,
                               width_, height_, out_i420_buffer));
  if (fwrite(res_i420_frame.Buffer(), 1, frame_length_,
             output_file) != static_cast<unsigned int>(frame_length_)) {
    return;
  }
  psnr = I420PSNR(orig_buffer, out_i420_buffer, width_, height_);
  EXPECT_EQ(48.0, psnr);
  j++;
  delete [] out_i420_buffer;

  // printf("\nConvert #%d I420 <-> YV12\n", j);
  uint8_t* outYV120Buffer = new uint8_t[frame_length_];

  EXPECT_EQ(0, ConvertFromI420(orig_buffer, width_, kYV12, 0,
                               width_, height_, outYV120Buffer));
  EXPECT_EQ(0, ConvertFromYV12(outYV120Buffer, width_,
                               kI420, 0,
                               width_, height_,
                               res_i420_frame.Buffer()));
  if (fwrite(res_i420_frame.Buffer(), 1, frame_length_,
             output_file) !=  static_cast<unsigned int>(frame_length_)) {
    return;
  }

  psnr = I420PSNR(orig_buffer, res_i420_frame.Buffer(), width_, height_);
  EXPECT_EQ(48.0, psnr);
  j++;
  delete [] outYV120Buffer;

  // printf("\nConvert #%d I420 <-> YUY2\n", j);
  uint8_t* out_yuy2_buffer = new uint8_t[width_ * height_ * 2];
  EXPECT_EQ(0, ConvertFromI420(orig_buffer, width_,
                               kYUY2, 0, width_, height_, out_yuy2_buffer));

  EXPECT_EQ(0, ConvertToI420(kYUY2, out_yuy2_buffer, 0, 0, width_, height_,
                             0, kRotateNone, &res_i420_frame));

  if (fwrite(res_i420_frame.Buffer(), 1, frame_length_,
             output_file) !=  static_cast<unsigned int>(frame_length_)) {
    return;
  }
  psnr = I420PSNR(orig_buffer, res_i420_frame.Buffer(), width_, height_);
  EXPECT_EQ(48.0, psnr);

  // printf("\nConvert #%d I420 <-> RGB565\n", j);
  uint8_t* out_rgb565_buffer = new uint8_t[width_ * height_ * 2];
  EXPECT_EQ(0, ConvertFromI420(orig_buffer, width_,
                               kRGB565, 0, width_, height_, out_rgb565_buffer));

  EXPECT_EQ(0, ConvertToI420(kRGB565, out_rgb565_buffer, 0, 0, width_, height_,
                             0, kRotateNone, &res_i420_frame));

  if (fwrite(res_i420_frame.Buffer(), 1, frame_length_,
             output_file) !=  static_cast<unsigned int>(frame_length_)) {
    return;
  }
  psnr = I420PSNR(orig_buffer, res_i420_frame.Buffer(), width_, height_);
  // TODO(leozwang) Investigate the right psnr should be set for I420ToRGB565,
  // Another example is I420ToRGB24, the psnr is 44
  EXPECT_GT(ceil(psnr), 40);

  // printf("\nConvert #%d I420 <-> ARGB8888\n", j);
  uint8_t* out_argb8888_buffer = new uint8_t[width_ * height_ * 4];
  EXPECT_EQ(0, ConvertFromI420(orig_buffer, width_,
                               kARGB, 0, width_, height_, out_argb8888_buffer));

  EXPECT_EQ(0, ConvertToI420(kARGB, out_argb8888_buffer, 0, 0, width_, height_,
                             0, kRotateNone, &res_i420_frame));

  if (fwrite(res_i420_frame.Buffer(), 1, frame_length_,
             output_file) !=  static_cast<unsigned int>(frame_length_)) {
    return;
  }
  psnr = I420PSNR(orig_buffer, res_i420_frame.Buffer(), width_, height_);
  // TODO(leozwang) Investigate the right psnr should be set for I420ToARGB8888,
  EXPECT_GT(ceil(psnr), 42);

  ASSERT_EQ(0, fclose(output_file));

  res_i420_frame.Free();
  delete [] out_argb8888_buffer;
  delete [] out_rgb565_buffer;
  delete [] out_yuy2_buffer;
  delete [] orig_buffer;
}

// TODO(holmer): Disabled for now due to crashes on Linux 32 bit. The theory
// is that it crashes due to the fact that the buffers are not 16 bit aligned.
// See http://code.google.com/p/webrtc/issues/detail?id=335 for more info.
TEST_F(TestLibYuv, DISABLED_MirrorTest) {
  // TODO (mikhal): Add an automated test to confirm output.
  // TODO(mikhal): Update to new I420VideoFrame and align values. Until then,
  // this test is disabled, only insuring build.
  std::string str;
  int width = 16;
  int height = 8;
  int length = webrtc::CalcBufferSize(kI420, width, height);

  VideoFrame test_frame;
  test_frame.VerifyAndAllocate(length);
  test_frame.SetWidth(width);
  test_frame.SetHeight(height);
  memset(test_frame.Buffer(), 255, length);

  // Create input frame.
  VideoFrame in_frame, test_in_frame;
  in_frame.VerifyAndAllocate(length);
  in_frame.SetWidth(width);
  in_frame.SetHeight(height);
  in_frame.SetLength(length);
  int plane_offset[3];  // prep for kNumPlanes.
  plane_offset[0] = 10;
  plane_offset[1] = 100;
  plane_offset[2] = 200;
  CreateImage(&in_frame, plane_offset);
  test_in_frame.CopyFrame(in_frame);
  EXPECT_EQ(0, PrintFrame(&in_frame, "InputFrame"));

  VideoFrame out_frame, test_out_frame;
  out_frame.VerifyAndAllocate(length);
  out_frame.SetWidth(width);
  out_frame.SetHeight(height);
  out_frame.SetLength(length);
  CreateImage(&out_frame, plane_offset);
  test_out_frame.CopyFrame(out_frame);

  // Left-Right.
  std::cout << "Test Mirror function: LeftRight" << std::endl;
  EXPECT_EQ(0, MirrorI420LeftRight(&in_frame, &out_frame));
  EXPECT_EQ(0, PrintFrame(&out_frame, "OutputFrame"));
  EXPECT_EQ(0, MirrorI420LeftRight(&out_frame, &in_frame));

  EXPECT_EQ(0, memcmp(in_frame.Buffer(), test_in_frame.Buffer(), length));

  // UpDown
  std::cout << "Test Mirror function: UpDown" << std::endl;
  EXPECT_EQ(0, MirrorI420UpDown(&in_frame, &out_frame));
  EXPECT_EQ(0, PrintFrame(&test_out_frame, "OutputFrame"));
  EXPECT_EQ(0, MirrorI420UpDown(&out_frame, &test_frame));
  EXPECT_EQ(0, memcmp(in_frame.Buffer(), test_frame.Buffer(), length));

  // TODO(mikhal): Write to a file, and ask to look at the file.

  std::cout << "Do the mirrored frames look correct?" << std::endl;
  in_frame.Free();
  test_in_frame.Free();
  out_frame.Free();
  test_out_frame.Free();
}

TEST_F(TestLibYuv, alignment) {
  int value = 0x3FF; // 1023
  EXPECT_EQ(0x400, AlignInt(value, 128));  // Low 7 bits are zero.
  EXPECT_EQ(0x400, AlignInt(value, 64));  // Low 6 bits are zero.
  EXPECT_EQ(0x400, AlignInt(value, 32));  // Low 5 bits are zero.
}

}  // namespace
