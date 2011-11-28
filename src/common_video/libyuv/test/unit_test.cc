/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/libyuv/test/unit_test.h"

#include <math.h>
#include <string.h>

#include "gtest/gtest.h"
#include "common_video/libyuv/include/libyuv.h"
#include "common_video/libyuv/include/scaler.h"
#include "common_video/libyuv/test/test_util.h"
#include "system_wrappers/interface/tick_util.h"

namespace webrtc {

class LibYuvTest : public ::testing::Test {
 protected:
  LibYuvTest();
  virtual void SetUp();
  virtual void TearDown();

  FILE* source_file_;
  std::string inname_;
  const int width_;
  const int height_;
  const int frame_length_;
};

void ScaleSequence(ScaleMethod method,
                   FILE* source_file, std::string out_name,
                   int src_width, int src_height,
                   int dst_width, int dst_height);

// TODO (mikhal): Update to new test file scheme when available.
// TODO (mikhal): Use scoped_ptr when handling buffers.
LibYuvTest::LibYuvTest()
    : source_file_(NULL),
      inname_("testFiles/foreman_cif.yuv"),
      width_(352),
      height_(288),
      frame_length_(CalcBufferSize(kI420, 352, 288)) {
}

void LibYuvTest::SetUp() {
  source_file_  = fopen(inname_.c_str(), "rb");
  ASSERT_TRUE(source_file_ != NULL) << "Cannot read file: "<< inname_ << "\n";
}

void LibYuvTest::TearDown() {
  if (source_file_ != NULL) {
    ASSERT_EQ(0, fclose(source_file_));
  }
  source_file_ = NULL;
}

TEST_F(LibYuvTest, ConvertSanityTest) {
  // TODO(mikhal)
}

TEST_F(LibYuvTest, ScaleSanityTest) {
  Scaler test_scaler;
  uint8_t* test_buffer = new uint8_t[frame_length_];
  // Scaling without setting values
  int size = 100;
  EXPECT_EQ(-2, test_scaler.Scale(test_buffer, test_buffer, size));

  // Setting bad initial values
  EXPECT_EQ(-1, test_scaler.Set(0, 288, 352, 288, kI420, kI420, kScalePoint));
  EXPECT_EQ(-1, test_scaler.Set(704, 0, 352, 288, kI420, kI420, kScaleBox));
  EXPECT_EQ(-1, test_scaler.Set(704, 576, 352, 0, kI420, kI420,
                                 kScaleBilinear));
  EXPECT_EQ(-1, test_scaler.Set(704, 576, 0, 288, kI420, kI420, kScalePoint));

  // Sending NULL pointer
  size = 0;
  EXPECT_EQ(-1, test_scaler.Scale(NULL, test_buffer, size));

  // Sending a buffer which is too small (should reallocate and update size)
  EXPECT_EQ(0, test_scaler.Set(352, 288, 144, 288, kI420, kI420, kScalePoint));
  uint8_t* test_buffer2 = NULL;
  size = 0;
  fread(test_buffer, 1, frame_length_, source_file_);
  EXPECT_EQ(0, test_scaler.Scale(test_buffer, test_buffer2, size));
  EXPECT_EQ(144 * 288 * 3 / 2, size);

  delete [] test_buffer;
}

TEST_F(LibYuvTest, MirrorSanityTest) {
  // TODO (mikhal): look into scoped_ptr for implementation
  // Sending NULL pointers
  uint8_t* test_buffer1 = new uint8_t[frame_length_];
  uint8_t* test_buffer2 = new uint8_t[frame_length_];
  // Setting bad initial values
  EXPECT_EQ(-1, MirrorI420LeftRight(test_buffer1, test_buffer2, width_, -30));
  EXPECT_EQ(-1, MirrorI420LeftRight(test_buffer1, test_buffer2, -352, height_));
  EXPECT_EQ(-1, MirrorI420LeftRight(NULL, test_buffer2, width_, height_));
  EXPECT_EQ(-1, MirrorI420LeftRight(test_buffer1, NULL, width_, height_));

  delete [] test_buffer1;
  delete [] test_buffer2;
}

TEST_F(LibYuvTest, ConvertTest) {
  // Reading YUV frame - testing on the first frame of the foreman sequence
  int j = 0;
  // TODO (mikhal): move to correct output path.
  std::string out_name = "conversionTest_out.yuv";
  FILE* output_file;
  double psnr = 0;

  output_file = fopen(out_name.c_str(), "wb");
  ASSERT_TRUE(output_file != NULL);

  uint8_t* orig_buffer = new uint8_t[frame_length_];
  fread(orig_buffer, 1, frame_length_, source_file_);

  // printf("\nConvert #%d I420 <-> RGB24\n", j);
  uint8_t* res_rgb_buffer2  = new uint8_t[width_ * height_ * 3];
  uint8_t* res_i420_buffer = new uint8_t[frame_length_];
  EXPECT_EQ(0, ConvertFromI420(kRGB24, orig_buffer, width_, height_,
                               res_rgb_buffer2, false, kRotateNone));
  EXPECT_EQ(0, ConvertToI420(kRGB24, res_rgb_buffer2, width_, height_,
                             res_i420_buffer, false, kRotateNone));

  fwrite(res_i420_buffer, frame_length_, 1, output_file);
  ImagePSNRfromBuffer(orig_buffer, res_i420_buffer, width_, height_, &psnr);
  // Optimization Speed- quality trade-off => 45 dB only.
  EXPECT_EQ(45.0, ceil(psnr));
  j++;
  delete [] res_rgb_buffer2;

  // printf("\nConvert #%d I420 <-> UYVY\n", j);
  uint8_t* out_uyvy_buffer = new uint8_t[width_ * height_ * 2];
  EXPECT_EQ(0, ConvertFromI420(kUYVY, orig_buffer, width_,
                            height_, out_uyvy_buffer, false, kRotateNone));

  EXPECT_EQ(0, ConvertToI420(kUYVY, out_uyvy_buffer, width_, height_,
                             res_i420_buffer, false, kRotateNone));
  ImagePSNRfromBuffer(orig_buffer, res_i420_buffer, width_, height_, &psnr);
  EXPECT_EQ(48.0, psnr);
  fwrite(res_i420_buffer, frame_length_, 1, output_file);

  j++;
  delete [] out_uyvy_buffer;

  // printf("\nConvert #%d I420 <-> I420 \n", j);
  uint8_t* out_i420_buffer = new uint8_t[width_ * height_ * 2];
  EXPECT_EQ(0, ConvertToI420(kI420, orig_buffer, width_, height_,
                             out_i420_buffer, false, kRotateNone));
  EXPECT_EQ(0, ConvertToI420(kI420 , out_i420_buffer, width_, height_,
                             res_i420_buffer, false, kRotateNone));
  fwrite(res_i420_buffer, frame_length_, 1, output_file);
  ImagePSNRfromBuffer(orig_buffer, res_i420_buffer, width_, height_, &psnr);
  EXPECT_EQ(48.0, psnr);
  j++;
  delete [] out_i420_buffer;

  // printf("\nConvert #%d I420 <-> YV12\n", j);
  uint8_t* outYV120Buffer = new uint8_t[frame_length_];

  EXPECT_EQ(0, ConvertFromI420(kYV12, orig_buffer, width_, height_,
                               outYV120Buffer, false, kRotateNone));
  EXPECT_EQ(0, ConvertYV12ToI420(outYV120Buffer, width_, height_,
                                 res_i420_buffer));
  fwrite(res_i420_buffer, frame_length_, 1, output_file);

  ImagePSNRfromBuffer(orig_buffer, res_i420_buffer, width_, height_, &psnr);
  EXPECT_EQ(48.0, psnr);
  j++;
  delete [] outYV120Buffer;

  // printf("\nTEST #%d I420 <-> YUY2\n", j);
  uint8_t* out_yuy2_buffer = new uint8_t[width_ * height_ * 2];

  EXPECT_EQ(0, ConvertFromI420(kYUY2, orig_buffer, width_, height_,
                               out_yuy2_buffer, false, kRotateNone));
  EXPECT_EQ(0, ConvertToI420(kYUY2, out_yuy2_buffer, width_, height_,
                             res_i420_buffer, false, kRotateNone));

  fwrite(res_i420_buffer, frame_length_, 1, output_file);
  ImagePSNRfromBuffer(orig_buffer, res_i420_buffer, width_, height_, &psnr);
  EXPECT_EQ(48.0, psnr);
  delete [] out_yuy2_buffer;

  delete [] res_i420_buffer;
  delete [] orig_buffer;
}

//TODO (mikhal): Converge the test into one function that accepts the method.
TEST_F(LibYuvTest, PointScaleTest) {
  ScaleMethod method = kScalePoint;
  // TODO (mikhal): use webrtc::test::OutputPath()
  std::string out_name = "PointScaleTest_176_144.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ / 2, height_ / 2);
  out_name = "PointScaleTest_320_240.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                320, 240);
  out_name = "PointScaleTest_704_576.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ * 2, height_ * 2);
  out_name = "PointScaleTest_300_200.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                300, 200);
  out_name = "PointScaleTest_400_300.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                400, 300);
}

TEST_F(LibYuvTest, BiLinearScaleTest) {
  ScaleMethod method = kScaleBilinear;
  std::string out_name = "BilinearScaleTest_176_144.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ / 2, height_ / 2);
  out_name = "BilinearScaleTest_320_240.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                320, 240);
  out_name = "BilinearScaleTest_704_576.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ * 2, height_ * 2);
  out_name = "BilinearScaleTest_300_200.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                300, 200);
  out_name = "BilinearScaleTest_400_300.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                400, 300);
}

TEST_F(LibYuvTest, BoxScaleTest) {
  ScaleMethod method = kScaleBox;
  std::string out_name = "BoxScaleTest_176_144.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ / 2, height_ / 2);
  out_name = "BoxScaleTest_320_240.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                320, 240);
  out_name = "BoxScaleTest_704_576.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ * 2, height_ * 2);
  out_name = "BoxScaleTest_300_200.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                300, 200);
  out_name = "BoxScaleTest_400_300.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                400, 300);
}

TEST_F(LibYuvTest, MirrorTest) {
  // TODO (mikhal): Add an automated test to confirm output.
  std::string str;
  int width = 16;
  int height = 8;
  int factor_y = 1;
  int factor_u = 1;
  int factor_v = 1;
  int start_buffer_offset = 10;
  int length = webrtc::CalcBufferSize(kI420, width, height);

  uint8_t* test_frame = new uint8_t[length];
  memset(test_frame, 255, length);

  // Create input frame
  uint8_t* in_frame = test_frame;
  uint8_t* in_frame_cb = in_frame + width * height;
  uint8_t* in_frame_cr = in_frame_cb + (width * height) / 4;
  CreateImage(width, height, in_frame, 10, factor_y, 1);  // Y
  CreateImage(width / 2, height / 2, in_frame_cb, 100, factor_u, 1);  // Cb
  CreateImage(width / 2, height / 2, in_frame_cr, 200, factor_v, 1);  // Cr
  EXPECT_EQ(0, PrintFrame(test_frame, width, height, "InputFrame"));

  uint8_t* test_frame2 = new uint8_t[length + start_buffer_offset * 2];
  memset(test_frame2, 255, length + start_buffer_offset * 2);
  uint8_t* out_frame = test_frame2;

  // LeftRight
  std::cout << "Test Mirror function: LeftRight" << std::endl;
  EXPECT_EQ(0, MirrorI420LeftRight(in_frame, out_frame, width, height));
  EXPECT_EQ(0, PrintFrame(test_frame2, width, height, "OutputFrame"));
  EXPECT_EQ(0, MirrorI420LeftRight(out_frame, test_frame, width, height));

  EXPECT_EQ(0, memcmp(in_frame, test_frame, length));

  // UpDown
  std::cout << "Test Mirror function: UpDown" << std::endl;
  EXPECT_EQ(0, MirrorI420UpDown(in_frame, out_frame, width, height));
  EXPECT_EQ(0, PrintFrame(test_frame2, width, height, "OutputFrame"));
  EXPECT_EQ(0, MirrorI420UpDown(out_frame, test_frame, width, height));

  EXPECT_EQ(0, memcmp(in_frame, test_frame, length));

  // TODO(mikhal): Write to a file, and ask to look at the file.

  std::cout << "Do the mirrored frames look correct?" << std::endl;
  delete [] test_frame;
  delete [] test_frame2;
}

// TODO (mikhal): Move part to a separate scale test.
void ScaleSequence(ScaleMethod method,
                   FILE* source_file, std::string out_name,
                   int src_width, int src_height,
                   int dst_width, int dst_height) {
  Scaler test_scaler;
  FILE* output_file;
  EXPECT_EQ(0, test_scaler.Set(src_width, src_height,
                               dst_width, dst_height,
                               kI420, kI420, method));

  output_file = fopen(out_name.c_str(), "wb");
  ASSERT_TRUE(output_file != NULL);

  rewind(source_file);

  int out_required_size = dst_width * dst_height * 3 / 2;
  int in_required_size = src_height * src_width * 3 / 2;
  uint8_t* input_buffer = new uint8_t[in_required_size];
  uint8_t* output_buffer = new uint8_t[out_required_size];

  int64_t start_clock, total_clock;
  total_clock = 0;
  int frame_count = 0;

  // Running through entire sequence
  while (feof(source_file) == 0) {
      if ((size_t)in_required_size !=
          fread(input_buffer, 1, in_required_size, source_file))
        break;

    start_clock = TickTime::MillisecondTimestamp();
    EXPECT_EQ(0, test_scaler.Scale(input_buffer, output_buffer,
                                   out_required_size));
    total_clock += TickTime::MillisecondTimestamp() - start_clock;
    fwrite(output_buffer, out_required_size, 1, output_file);
    frame_count++;
  }

  if (frame_count) {
    printf("Scaling[%d %d] => [%d %d]: ",
           src_width, src_height, dst_width, dst_height);
    printf("Average time per frame[ms]: %.2lf\n",
             (static_cast<double>(total_clock) / frame_count));
  }
  delete [] input_buffer;
  delete [] output_buffer;
}

}  // namespace
