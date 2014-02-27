// libjingle
// Copyright 2014 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <string>

#include "libyuv/convert.h"
#include "libyuv/convert_from.h"
#include "libyuv/convert_from_argb.h"
#include "libyuv/format_conversion.h"
#include "libyuv/mjpeg_decoder.h"
#include "libyuv/planar_functions.h"
#include "talk/base/flags.h"
#include "talk/base/gunit.h"
#include "talk/base/scoped_ptr.h"
#include "talk/media/base/testutils.h"
#include "talk/media/base/videocommon.h"

// Undefine macros for the windows build.
#undef max
#undef min

using cricket::DumpPlanarYuvTestImage;

DEFINE_bool(planarfunctions_dump, false,
    "whether to write out scaled images for inspection");
DEFINE_int(planarfunctions_repeat, 1,
    "how many times to perform each scaling operation (for perf testing)");

namespace cricket {

// Number of testing colors in each color channel.
static const int kTestingColorChannelResolution = 6;

// The total number of testing colors
// kTestingColorNum = kTestingColorChannelResolution^3;
static const int kTestingColorNum = kTestingColorChannelResolution *
    kTestingColorChannelResolution * kTestingColorChannelResolution;

static const int kWidth = 1280;
static const int kHeight = 720;
static const int kAlignment = 16;

class PlanarFunctionsTest : public testing::TestWithParam<int> {
 protected:
  PlanarFunctionsTest() : dump_(false), repeat_(1) {
    InitializeColorBand();
  }

  virtual void SetUp() {
    dump_ = FLAG_planarfunctions_dump;
    repeat_ = FLAG_planarfunctions_repeat;
  }

  // Initialize the color band for testing.
  void InitializeColorBand() {
    testing_color_y_.reset(new uint8[kTestingColorNum]);
    testing_color_u_.reset(new uint8[kTestingColorNum]);
    testing_color_v_.reset(new uint8[kTestingColorNum]);
    testing_color_r_.reset(new uint8[kTestingColorNum]);
    testing_color_g_.reset(new uint8[kTestingColorNum]);
    testing_color_b_.reset(new uint8[kTestingColorNum]);
    int color_counter = 0;
    for (int i = 0; i < kTestingColorChannelResolution; ++i) {
      uint8 color_r = static_cast<uint8>(
          i * 255 / (kTestingColorChannelResolution - 1));
      for (int j = 0; j < kTestingColorChannelResolution; ++j) {
        uint8 color_g = static_cast<uint8>(
            j * 255 / (kTestingColorChannelResolution - 1));
        for (int k = 0; k < kTestingColorChannelResolution; ++k) {
          uint8 color_b = static_cast<uint8>(
              k * 255 / (kTestingColorChannelResolution - 1));
          testing_color_r_[color_counter] = color_r;
          testing_color_g_[color_counter] = color_g;
          testing_color_b_[color_counter] = color_b;
           // Converting the testing RGB colors to YUV colors.
          ConvertRgbPixel(color_r, color_g, color_b,
                          &(testing_color_y_[color_counter]),
                          &(testing_color_u_[color_counter]),
                          &(testing_color_v_[color_counter]));
          ++color_counter;
        }
      }
    }
  }
  // Simple and slow RGB->YUV conversion. From NTSC standard, c/o Wikipedia.
  // (from lmivideoframe_unittest.cc)
  void ConvertRgbPixel(uint8 r, uint8 g, uint8 b,
                       uint8* y, uint8* u, uint8* v) {
    *y = ClampUint8(.257 * r + .504 * g + .098 * b + 16);
    *u = ClampUint8(-.148 * r - .291 * g + .439 * b + 128);
    *v = ClampUint8(.439 * r - .368 * g - .071 * b + 128);
  }

  uint8 ClampUint8(double value) {
    value = std::max(0., std::min(255., value));
    uint8 uint8_value = static_cast<uint8>(value);
    return uint8_value;
  }

  // Generate a Red-Green-Blue inter-weaving chessboard-like
  // YUV testing image (I420/I422/I444).
  // The pattern looks like c0 c1 c2 c3 ...
  //                        c1 c2 c3 c4 ...
  //                        c2 c3 c4 c5 ...
  //                        ...............
  // The size of each chrome block is (block_size) x (block_size).
  uint8* CreateFakeYuvTestingImage(int height, int width, int block_size,
                                   libyuv::JpegSubsamplingType subsample_type,
                                   uint8* &y_pointer,
                                   uint8* &u_pointer,
                                   uint8* &v_pointer) {
    if (height <= 0 || width <= 0 || block_size <= 0) { return NULL; }
    int y_size = height * width;
    int u_size, v_size;
    int vertical_sample_ratio = 1, horizontal_sample_ratio = 1;
    switch (subsample_type) {
      case libyuv::kJpegYuv420:
        u_size = ((height + 1) >> 1) * ((width + 1) >> 1);
        v_size = u_size;
        vertical_sample_ratio = 2, horizontal_sample_ratio = 2;
        break;
      case libyuv::kJpegYuv422:
        u_size = height * ((width + 1) >> 1);
        v_size = u_size;
        vertical_sample_ratio = 1, horizontal_sample_ratio = 2;
        break;
      case libyuv::kJpegYuv444:
        v_size = u_size = y_size;
        vertical_sample_ratio = 1, horizontal_sample_ratio = 1;
        break;
      case libyuv::kJpegUnknown:
      default:
        return NULL;
        break;
    }
    uint8* image_pointer = new uint8[y_size + u_size + v_size + kAlignment];
    y_pointer = ALIGNP(image_pointer, kAlignment);
    u_pointer = ALIGNP(&image_pointer[y_size], kAlignment);
    v_pointer = ALIGNP(&image_pointer[y_size + u_size], kAlignment);
    uint8* current_y_pointer = y_pointer;
    uint8* current_u_pointer = u_pointer;
    uint8* current_v_pointer = v_pointer;
    for (int j = 0; j < height; ++j) {
      for (int i = 0; i < width; ++i) {
        int color = ((i / block_size) + (j / block_size)) % kTestingColorNum;
        *(current_y_pointer++) = testing_color_y_[color];
        if (i % horizontal_sample_ratio == 0 &&
            j % vertical_sample_ratio == 0) {
          *(current_u_pointer++) = testing_color_u_[color];
          *(current_v_pointer++) = testing_color_v_[color];
        }
      }
    }
    return image_pointer;
  }

  // Generate a Red-Green-Blue inter-weaving chessboard-like
  // YUY2/UYVY testing image.
  // The pattern looks like c0 c1 c2 c3 ...
  //                        c1 c2 c3 c4 ...
  //                        c2 c3 c4 c5 ...
  //                        ...............
  // The size of each chrome block is (block_size) x (block_size).
  uint8* CreateFakeInterleaveYuvTestingImage(
      int height, int width, int block_size,
      uint8* &yuv_pointer, FourCC fourcc_type) {
    if (height <= 0 || width <= 0 || block_size <= 0) { return NULL; }
    if (fourcc_type != FOURCC_YUY2 && fourcc_type != FOURCC_UYVY) {
      LOG(LS_ERROR) << "Format " << static_cast<int>(fourcc_type)
                    << " is not supported.";
      return NULL;
    }
    // Regularize the width of the output to be even.
    int awidth = (width + 1) & ~1;

    uint8* image_pointer = new uint8[2 * height * awidth + kAlignment];
    yuv_pointer = ALIGNP(image_pointer, kAlignment);
    uint8* current_yuv_pointer = yuv_pointer;
    switch (fourcc_type) {
      case FOURCC_YUY2: {
        for (int j = 0; j < height; ++j) {
          for (int i = 0; i < awidth; i += 2, current_yuv_pointer += 4) {
            int color1 = ((i / block_size) + (j / block_size)) %
                kTestingColorNum;
            int color2 = (((i + 1) / block_size) + (j / block_size)) %
                kTestingColorNum;
            current_yuv_pointer[0] = testing_color_y_[color1];
            if (i < width) {
              current_yuv_pointer[1] = static_cast<uint8>(
                  (static_cast<uint32>(testing_color_u_[color1]) +
                  static_cast<uint32>(testing_color_u_[color2])) / 2);
              current_yuv_pointer[2] = testing_color_y_[color2];
              current_yuv_pointer[3] = static_cast<uint8>(
                  (static_cast<uint32>(testing_color_v_[color1]) +
                  static_cast<uint32>(testing_color_v_[color2])) / 2);
            } else {
              current_yuv_pointer[1] = testing_color_u_[color1];
              current_yuv_pointer[2] = 0;
              current_yuv_pointer[3] = testing_color_v_[color1];
            }
          }
        }
        break;
      }
      case FOURCC_UYVY: {
        for (int j = 0; j < height; ++j) {
          for (int i = 0; i < awidth; i += 2, current_yuv_pointer += 4) {
            int color1 = ((i / block_size) + (j / block_size)) %
                kTestingColorNum;
            int color2 = (((i + 1) / block_size) + (j / block_size)) %
                kTestingColorNum;
            if (i < width) {
              current_yuv_pointer[0] = static_cast<uint8>(
                  (static_cast<uint32>(testing_color_u_[color1]) +
                  static_cast<uint32>(testing_color_u_[color2])) / 2);
              current_yuv_pointer[1] = testing_color_y_[color1];
              current_yuv_pointer[2] = static_cast<uint8>(
                  (static_cast<uint32>(testing_color_v_[color1]) +
                  static_cast<uint32>(testing_color_v_[color2])) / 2);
              current_yuv_pointer[3] = testing_color_y_[color2];
            } else {
              current_yuv_pointer[0] = testing_color_u_[color1];
              current_yuv_pointer[1] = testing_color_y_[color1];
              current_yuv_pointer[2] = testing_color_v_[color1];
              current_yuv_pointer[3] = 0;
            }
          }
        }
        break;
      }
    }
    return image_pointer;
  }
  // Generate a Red-Green-Blue inter-weaving chessboard-like
  // Q420 testing image.
  // The pattern looks like c0 c1 c2 c3 ...
  //                        c1 c2 c3 c4 ...
  //                        c2 c3 c4 c5 ...
  //                        ...............
  // The size of each chrome block is (block_size) x (block_size).
  uint8* CreateFakeQ420TestingImage(int height, int width, int block_size,
      uint8* &y_pointer, uint8* &yuy2_pointer) {
    if (height <= 0 || width <= 0 || block_size <= 0) { return NULL; }
    // Regularize the width of the output to be even.
    int awidth = (width + 1) & ~1;

    uint8* image_pointer = new uint8[(height / 2) * awidth * 2 +
        ((height + 1) / 2) * width + kAlignment];
    y_pointer = ALIGNP(image_pointer, kAlignment);
    yuy2_pointer = y_pointer + ((height + 1) / 2) * width;
    uint8* current_yuy2_pointer = yuy2_pointer;
    uint8* current_y_pointer = y_pointer;
    for (int j = 0; j < height; ++j) {
      if (j % 2 == 0) {
        for (int i = 0; i < width; ++i) {
          int color = ((i / block_size) + (j / block_size)) %
              kTestingColorNum;
          *(current_y_pointer++) = testing_color_y_[color];
        }
      } else {
        for (int i = 0; i < awidth; i += 2, current_yuy2_pointer += 4) {
          int color1 = ((i / block_size) + (j / block_size)) %
              kTestingColorNum;
          int color2 = (((i + 1) / block_size) + (j / block_size)) %
              kTestingColorNum;
          current_yuy2_pointer[0] = testing_color_y_[color1];
          if (i < width) {
            current_yuy2_pointer[1] = static_cast<uint8>(
                (static_cast<uint32>(testing_color_u_[color1]) +
                static_cast<uint32>(testing_color_u_[color2])) / 2);
            current_yuy2_pointer[2] = testing_color_y_[color2];
            current_yuy2_pointer[3] = static_cast<uint8>(
                (static_cast<uint32>(testing_color_v_[color1]) +
                static_cast<uint32>(testing_color_v_[color2])) / 2);
          } else {
            current_yuy2_pointer[1] = testing_color_u_[color1];
            current_yuy2_pointer[2] = 0;
            current_yuy2_pointer[3] = testing_color_v_[color1];
          }
        }
      }
    }
    return image_pointer;
  }

  // Generate a Red-Green-Blue inter-weaving chessboard-like
  // NV12 testing image.
  // (Note: No interpolation is used.)
  // The pattern looks like c0 c1 c2 c3 ...
  //                        c1 c2 c3 c4 ...
  //                        c2 c3 c4 c5 ...
  //                        ...............
  // The size of each chrome block is (block_size) x (block_size).
  uint8* CreateFakeNV12TestingImage(int height, int width, int block_size,
      uint8* &y_pointer, uint8* &uv_pointer) {
    if (height <= 0 || width <= 0 || block_size <= 0) { return NULL; }

    uint8* image_pointer = new uint8[height * width +
        ((height + 1) / 2) * ((width + 1) / 2) * 2 + kAlignment];
    y_pointer = ALIGNP(image_pointer, kAlignment);
    uv_pointer = y_pointer + height * width;
    uint8* current_uv_pointer = uv_pointer;
    uint8* current_y_pointer = y_pointer;
    for (int j = 0; j < height; ++j) {
      for (int i = 0; i < width; ++i) {
        int color = ((i / block_size) + (j / block_size)) %
            kTestingColorNum;
        *(current_y_pointer++) = testing_color_y_[color];
      }
      if (j % 2 == 0) {
        for (int i = 0; i < width; i += 2, current_uv_pointer += 2) {
          int color = ((i / block_size) + (j / block_size)) %
              kTestingColorNum;
          current_uv_pointer[0] = testing_color_u_[color];
          current_uv_pointer[1] = testing_color_v_[color];
        }
      }
    }
    return image_pointer;
  }

  // Generate a Red-Green-Blue inter-weaving chessboard-like
  // M420 testing image.
  // (Note: No interpolation is used.)
  // The pattern looks like c0 c1 c2 c3 ...
  //                        c1 c2 c3 c4 ...
  //                        c2 c3 c4 c5 ...
  //                        ...............
  // The size of each chrome block is (block_size) x (block_size).
  uint8* CreateFakeM420TestingImage(
      int height, int width, int block_size, uint8* &m420_pointer) {
    if (height <= 0 || width <= 0 || block_size <= 0) { return NULL; }

    uint8* image_pointer = new uint8[height * width +
        ((height + 1) / 2) * ((width + 1) / 2) * 2 + kAlignment];
    m420_pointer = ALIGNP(image_pointer, kAlignment);
    uint8* current_m420_pointer = m420_pointer;
    for (int j = 0; j < height; ++j) {
      for (int i = 0; i < width; ++i) {
        int color = ((i / block_size) + (j / block_size)) %
            kTestingColorNum;
        *(current_m420_pointer++) = testing_color_y_[color];
      }
      if (j % 2 == 1) {
        for (int i = 0; i < width; i += 2, current_m420_pointer += 2) {
          int color = ((i / block_size) + ((j - 1) / block_size)) %
              kTestingColorNum;
          current_m420_pointer[0] = testing_color_u_[color];
          current_m420_pointer[1] = testing_color_v_[color];
        }
      }
    }
    return image_pointer;
  }

  // Generate a Red-Green-Blue inter-weaving chessboard-like
  // ARGB/ABGR/RAW/BG24 testing image.
  // The pattern looks like c0 c1 c2 c3 ...
  //                        c1 c2 c3 c4 ...
  //                        c2 c3 c4 c5 ...
  //                        ...............
  // The size of each chrome block is (block_size) x (block_size).
  uint8* CreateFakeArgbTestingImage(int height, int width, int block_size,
                                    uint8* &argb_pointer, FourCC fourcc_type) {
    if (height <= 0 || width <= 0 || block_size <= 0) { return NULL; }
    uint8* image_pointer = NULL;
    if (fourcc_type == FOURCC_ABGR || fourcc_type == FOURCC_BGRA ||
        fourcc_type == FOURCC_ARGB) {
      image_pointer = new uint8[height * width * 4 + kAlignment];
    } else if (fourcc_type == FOURCC_RAW || fourcc_type == FOURCC_24BG) {
      image_pointer = new uint8[height * width * 3 + kAlignment];
    } else {
      LOG(LS_ERROR) << "Format " << static_cast<int>(fourcc_type)
                    << " is not supported.";
      return NULL;
    }
    argb_pointer = ALIGNP(image_pointer, kAlignment);
    uint8* current_pointer = argb_pointer;
    switch (fourcc_type) {
      case FOURCC_ARGB: {
        for (int j = 0; j < height; ++j) {
          for (int i = 0; i < width; ++i) {
            int color = ((i / block_size) + (j / block_size)) %
                kTestingColorNum;
            *(current_pointer++) = testing_color_b_[color];
            *(current_pointer++) = testing_color_g_[color];
            *(current_pointer++) = testing_color_r_[color];
            *(current_pointer++) = 255;
          }
        }
        break;
      }
      case FOURCC_ABGR: {
        for (int j = 0; j < height; ++j) {
          for (int i = 0; i < width; ++i) {
            int color = ((i / block_size) + (j / block_size)) %
                kTestingColorNum;
            *(current_pointer++) = testing_color_r_[color];
            *(current_pointer++) = testing_color_g_[color];
            *(current_pointer++) = testing_color_b_[color];
            *(current_pointer++) = 255;
          }
        }
        break;
      }
      case FOURCC_BGRA: {
        for (int j = 0; j < height; ++j) {
          for (int i = 0; i < width; ++i) {
            int color = ((i / block_size) + (j / block_size)) %
                kTestingColorNum;
            *(current_pointer++) = 255;
            *(current_pointer++) = testing_color_r_[color];
            *(current_pointer++) = testing_color_g_[color];
            *(current_pointer++) = testing_color_b_[color];
           }
        }
        break;
      }
      case FOURCC_24BG: {
        for (int j = 0; j < height; ++j) {
          for (int i = 0; i < width; ++i) {
            int color = ((i / block_size) + (j / block_size)) %
                kTestingColorNum;
            *(current_pointer++) = testing_color_b_[color];
            *(current_pointer++) = testing_color_g_[color];
            *(current_pointer++) = testing_color_r_[color];
          }
        }
        break;
      }
      case FOURCC_RAW: {
        for (int j = 0; j < height; ++j) {
          for (int i = 0; i < width; ++i) {
            int color = ((i / block_size) + (j / block_size)) %
                kTestingColorNum;
            *(current_pointer++) = testing_color_r_[color];
            *(current_pointer++) = testing_color_g_[color];
            *(current_pointer++) = testing_color_b_[color];
          }
        }
        break;
      }
      default: {
        LOG(LS_ERROR) << "Format " << static_cast<int>(fourcc_type)
                      << " is not supported.";
      }
    }
    return image_pointer;
  }

  // Check if two memory chunks are equal.
  // (tolerate MSE errors within a threshold).
  static bool IsMemoryEqual(const uint8* ibuf, const uint8* obuf,
                            int osize, double average_error) {
    double sse = cricket::ComputeSumSquareError(ibuf, obuf, osize);
    double error = sse / osize;  // Mean Squared Error.
    double PSNR = cricket::ComputePSNR(sse, osize);
    LOG(LS_INFO) << "Image MSE: "  << error << " Image PSNR: " << PSNR
                 << " First Diff Byte: " << FindDiff(ibuf, obuf, osize);
    return (error < average_error);
  }

  // Returns the index of the first differing byte. Easier to debug than memcmp.
  static int FindDiff(const uint8* buf1, const uint8* buf2, int len) {
    int i = 0;
    while (i < len && buf1[i] == buf2[i]) {
      i++;
    }
    return (i < len) ? i : -1;
  }

  // Dump the result image (ARGB format).
  void DumpArgbImage(const uint8* obuf, int width, int height) {
    DumpPlanarArgbTestImage(GetTestName(), obuf, width, height);
  }

  // Dump the result image (YUV420 format).
  void DumpYuvImage(const uint8* obuf, int width, int height) {
    DumpPlanarYuvTestImage(GetTestName(), obuf, width, height);
  }

  std::string GetTestName() {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    std::string test_name(test_info->name());
    return test_name;
  }

  bool dump_;
  int repeat_;

  // Y, U, V and R, G, B channels of testing colors.
  talk_base::scoped_ptr<uint8[]> testing_color_y_;
  talk_base::scoped_ptr<uint8[]> testing_color_u_;
  talk_base::scoped_ptr<uint8[]> testing_color_v_;
  talk_base::scoped_ptr<uint8[]> testing_color_r_;
  talk_base::scoped_ptr<uint8[]> testing_color_g_;
  talk_base::scoped_ptr<uint8[]> testing_color_b_;
};

TEST_F(PlanarFunctionsTest, I420Copy) {
  uint8 *y_pointer = NULL, *u_pointer = NULL, *v_pointer = NULL;
  int y_pitch = kWidth;
  int u_pitch = (kWidth + 1) >> 1;
  int v_pitch = (kWidth + 1) >> 1;
  int y_size = kHeight * kWidth;
  int uv_size = ((kHeight + 1) >> 1) * ((kWidth + 1) >> 1);
  int block_size = 3;
  // Generate a fake input image.
  talk_base::scoped_ptr<uint8[]> yuv_input(
      CreateFakeYuvTestingImage(kHeight, kWidth, block_size,
                                libyuv::kJpegYuv420,
                                y_pointer, u_pointer, v_pointer));
  // Allocate space for the output image.
  talk_base::scoped_ptr<uint8[]> yuv_output(
      new uint8[I420_SIZE(kHeight, kWidth) + kAlignment]);
  uint8 *y_output_pointer = ALIGNP(yuv_output.get(), kAlignment);
  uint8 *u_output_pointer = y_output_pointer + y_size;
  uint8 *v_output_pointer = u_output_pointer + uv_size;

  for (int i = 0; i < repeat_; ++i) {
  libyuv::I420Copy(y_pointer, y_pitch,
                   u_pointer, u_pitch,
                   v_pointer, v_pitch,
                   y_output_pointer, y_pitch,
                   u_output_pointer, u_pitch,
                   v_output_pointer, v_pitch,
                   kWidth, kHeight);
  }

  // Expect the copied frame to be exactly the same.
  EXPECT_TRUE(IsMemoryEqual(y_output_pointer, y_pointer,
      I420_SIZE(kHeight, kWidth), 1.e-6));

  if (dump_) { DumpYuvImage(y_output_pointer, kWidth, kHeight); }
}

TEST_F(PlanarFunctionsTest, I422ToI420) {
  uint8 *y_pointer = NULL, *u_pointer = NULL, *v_pointer = NULL;
  int y_pitch = kWidth;
  int u_pitch = (kWidth + 1) >> 1;
  int v_pitch = (kWidth + 1) >> 1;
  int y_size = kHeight * kWidth;
  int uv_size = ((kHeight + 1) >> 1) * ((kWidth + 1) >> 1);
  int block_size = 2;
  // Generate a fake input image.
  talk_base::scoped_ptr<uint8[]> yuv_input(
      CreateFakeYuvTestingImage(kHeight, kWidth, block_size,
                                libyuv::kJpegYuv422,
                                y_pointer, u_pointer, v_pointer));
  // Allocate space for the output image.
  talk_base::scoped_ptr<uint8[]> yuv_output(
      new uint8[I420_SIZE(kHeight, kWidth) + kAlignment]);
  uint8 *y_output_pointer = ALIGNP(yuv_output.get(), kAlignment);
  uint8 *u_output_pointer = y_output_pointer + y_size;
  uint8 *v_output_pointer = u_output_pointer + uv_size;
  // Generate the expected output.
  uint8 *y_expected_pointer = NULL, *u_expected_pointer = NULL,
        *v_expected_pointer = NULL;
  talk_base::scoped_ptr<uint8[]> yuv_output_expected(
      CreateFakeYuvTestingImage(kHeight, kWidth, block_size,
          libyuv::kJpegYuv420,
          y_expected_pointer, u_expected_pointer, v_expected_pointer));

  for (int i = 0; i < repeat_; ++i) {
  libyuv::I422ToI420(y_pointer, y_pitch,
                     u_pointer, u_pitch,
                     v_pointer, v_pitch,
                     y_output_pointer, y_pitch,
                     u_output_pointer, u_pitch,
                     v_output_pointer, v_pitch,
                     kWidth, kHeight);
  }

  // Compare the output frame with what is expected; expect exactly the same.
  // Note: MSE should be set to a larger threshold if an odd block width
  // is used, since the conversion will be lossy.
  EXPECT_TRUE(IsMemoryEqual(y_output_pointer, y_expected_pointer,
      I420_SIZE(kHeight, kWidth), 1.e-6));

  if (dump_) { DumpYuvImage(y_output_pointer, kWidth, kHeight); }
}

TEST_P(PlanarFunctionsTest, Q420ToI420) {
  // Get the unalignment offset
  int unalignment = GetParam();
  uint8 *y_pointer = NULL, *yuy2_pointer = NULL;
  int y_pitch = kWidth;
  int yuy2_pitch = 2 * ((kWidth + 1) & ~1);
  int u_pitch = (kWidth + 1) >> 1;
  int v_pitch = (kWidth + 1) >> 1;
  int y_size = kHeight * kWidth;
  int uv_size = ((kHeight + 1) >> 1) * ((kWidth + 1) >> 1);
  int block_size = 2;
  // Generate a fake input image.
  talk_base::scoped_ptr<uint8[]> yuv_input(
      CreateFakeQ420TestingImage(kHeight, kWidth, block_size,
                                 y_pointer, yuy2_pointer));
  // Allocate space for the output image.
  talk_base::scoped_ptr<uint8[]> yuv_output(
      new uint8[I420_SIZE(kHeight, kWidth) + kAlignment + unalignment]);
  uint8 *y_output_pointer = ALIGNP(yuv_output.get(), kAlignment) +
      unalignment;
  uint8 *u_output_pointer = y_output_pointer + y_size;
  uint8 *v_output_pointer = u_output_pointer + uv_size;
  // Generate the expected output.
  uint8 *y_expected_pointer = NULL, *u_expected_pointer = NULL,
        *v_expected_pointer = NULL;
  talk_base::scoped_ptr<uint8[]> yuv_output_expected(
      CreateFakeYuvTestingImage(kHeight, kWidth, block_size,
          libyuv::kJpegYuv420,
          y_expected_pointer, u_expected_pointer, v_expected_pointer));

  for (int i = 0; i < repeat_; ++i) {
  libyuv::Q420ToI420(y_pointer, y_pitch,
                     yuy2_pointer, yuy2_pitch,
                     y_output_pointer, y_pitch,
                     u_output_pointer, u_pitch,
                     v_output_pointer, v_pitch,
                     kWidth, kHeight);
  }
  // Compare the output frame with what is expected; expect exactly the same.
  // Note: MSE should be set to a larger threshold if an odd block width
  // is used, since the conversion will be lossy.
  EXPECT_TRUE(IsMemoryEqual(y_output_pointer, y_expected_pointer,
      I420_SIZE(kHeight, kWidth), 1.e-6));

  if (dump_) { DumpYuvImage(y_output_pointer, kWidth, kHeight); }
}

TEST_P(PlanarFunctionsTest, M420ToI420) {
  // Get the unalignment offset
  int unalignment = GetParam();
  uint8 *m420_pointer = NULL;
  int y_pitch = kWidth;
  int m420_pitch = kWidth;
  int u_pitch = (kWidth + 1) >> 1;
  int v_pitch = (kWidth + 1) >> 1;
  int y_size = kHeight * kWidth;
  int uv_size = ((kHeight + 1) >> 1) * ((kWidth + 1) >> 1);
  int block_size = 2;
  // Generate a fake input image.
  talk_base::scoped_ptr<uint8[]> yuv_input(
      CreateFakeM420TestingImage(kHeight, kWidth, block_size, m420_pointer));
  // Allocate space for the output image.
  talk_base::scoped_ptr<uint8[]> yuv_output(
      new uint8[I420_SIZE(kHeight, kWidth) + kAlignment + unalignment]);
  uint8 *y_output_pointer = ALIGNP(yuv_output.get(), kAlignment) + unalignment;
  uint8 *u_output_pointer = y_output_pointer + y_size;
  uint8 *v_output_pointer = u_output_pointer + uv_size;
  // Generate the expected output.
  uint8 *y_expected_pointer = NULL, *u_expected_pointer = NULL,
        *v_expected_pointer = NULL;
  talk_base::scoped_ptr<uint8[]> yuv_output_expected(
      CreateFakeYuvTestingImage(kHeight, kWidth, block_size,
          libyuv::kJpegYuv420,
          y_expected_pointer, u_expected_pointer, v_expected_pointer));

  for (int i = 0; i < repeat_; ++i) {
  libyuv::M420ToI420(m420_pointer, m420_pitch,
                     y_output_pointer, y_pitch,
                     u_output_pointer, u_pitch,
                     v_output_pointer, v_pitch,
                     kWidth, kHeight);
  }
  // Compare the output frame with what is expected; expect exactly the same.
  // Note: MSE should be set to a larger threshold if an odd block width
  // is used, since the conversion will be lossy.
  EXPECT_TRUE(IsMemoryEqual(y_output_pointer, y_expected_pointer,
      I420_SIZE(kHeight, kWidth), 1.e-6));

  if (dump_) { DumpYuvImage(y_output_pointer, kWidth, kHeight); }
}

TEST_P(PlanarFunctionsTest, NV12ToI420) {
  // Get the unalignment offset
  int unalignment = GetParam();
  uint8 *y_pointer = NULL, *uv_pointer = NULL;
  int y_pitch = kWidth;
  int uv_pitch = 2 * ((kWidth + 1) >> 1);
  int u_pitch = (kWidth + 1) >> 1;
  int v_pitch = (kWidth + 1) >> 1;
  int y_size = kHeight * kWidth;
  int uv_size = ((kHeight + 1) >> 1) * ((kWidth + 1) >> 1);
  int block_size = 2;
  // Generate a fake input image.
  talk_base::scoped_ptr<uint8[]> yuv_input(
      CreateFakeNV12TestingImage(kHeight, kWidth, block_size,
                                 y_pointer, uv_pointer));
  // Allocate space for the output image.
  talk_base::scoped_ptr<uint8[]> yuv_output(
      new uint8[I420_SIZE(kHeight, kWidth) + kAlignment + unalignment]);
  uint8 *y_output_pointer = ALIGNP(yuv_output.get(), kAlignment) + unalignment;
  uint8 *u_output_pointer = y_output_pointer + y_size;
  uint8 *v_output_pointer = u_output_pointer + uv_size;
  // Generate the expected output.
  uint8 *y_expected_pointer = NULL, *u_expected_pointer = NULL,
        *v_expected_pointer = NULL;
  talk_base::scoped_ptr<uint8[]> yuv_output_expected(
      CreateFakeYuvTestingImage(kHeight, kWidth, block_size,
          libyuv::kJpegYuv420,
          y_expected_pointer, u_expected_pointer, v_expected_pointer));

  for (int i = 0; i < repeat_; ++i) {
  libyuv::NV12ToI420(y_pointer, y_pitch,
                     uv_pointer, uv_pitch,
                     y_output_pointer, y_pitch,
                     u_output_pointer, u_pitch,
                     v_output_pointer, v_pitch,
                     kWidth, kHeight);
  }
  // Compare the output frame with what is expected; expect exactly the same.
  // Note: MSE should be set to a larger threshold if an odd block width
  // is used, since the conversion will be lossy.
  EXPECT_TRUE(IsMemoryEqual(y_output_pointer, y_expected_pointer,
      I420_SIZE(kHeight, kWidth), 1.e-6));

  if (dump_) { DumpYuvImage(y_output_pointer, kWidth, kHeight); }
}

// A common macro for testing converting YUY2/UYVY to I420.
#define TEST_YUVTOI420(SRC_NAME, MSE, BLOCK_SIZE) \
TEST_P(PlanarFunctionsTest, SRC_NAME##ToI420) { \
  /* Get the unalignment offset.*/ \
  int unalignment = GetParam(); \
  uint8 *yuv_pointer = NULL; \
  int yuv_pitch = 2 * ((kWidth + 1) & ~1); \
  int y_pitch = kWidth; \
  int u_pitch = (kWidth + 1) >> 1; \
  int v_pitch = (kWidth + 1) >> 1; \
  int y_size = kHeight * kWidth; \
  int uv_size = ((kHeight + 1) >> 1) * ((kWidth + 1) >> 1); \
  int block_size = 2; \
  /* Generate a fake input image.*/ \
  talk_base::scoped_ptr<uint8[]> yuv_input( \
      CreateFakeInterleaveYuvTestingImage(kHeight, kWidth, BLOCK_SIZE, \
          yuv_pointer, FOURCC_##SRC_NAME)); \
  /* Allocate space for the output image.*/ \
  talk_base::scoped_ptr<uint8[]> yuv_output( \
      new uint8[I420_SIZE(kHeight, kWidth) + kAlignment + unalignment]); \
  uint8 *y_output_pointer = ALIGNP(yuv_output.get(), kAlignment) + \
      unalignment; \
  uint8 *u_output_pointer = y_output_pointer + y_size; \
  uint8 *v_output_pointer = u_output_pointer + uv_size; \
  /* Generate the expected output.*/ \
  uint8 *y_expected_pointer = NULL, *u_expected_pointer = NULL, \
        *v_expected_pointer = NULL; \
  talk_base::scoped_ptr<uint8[]> yuv_output_expected( \
      CreateFakeYuvTestingImage(kHeight, kWidth, block_size, \
          libyuv::kJpegYuv420, \
          y_expected_pointer, u_expected_pointer, v_expected_pointer)); \
  for (int i = 0; i < repeat_; ++i) { \
    libyuv::SRC_NAME##ToI420(yuv_pointer, yuv_pitch, \
                             y_output_pointer, y_pitch, \
                             u_output_pointer, u_pitch, \
                             v_output_pointer, v_pitch, \
                             kWidth, kHeight); \
  } \
  /* Compare the output frame with what is expected.*/ \
  /* Note: MSE should be set to a larger threshold if an odd block width*/ \
  /* is used, since the conversion will be lossy.*/ \
  EXPECT_TRUE(IsMemoryEqual(y_output_pointer, y_expected_pointer, \
      I420_SIZE(kHeight, kWidth), MSE)); \
  if (dump_) { DumpYuvImage(y_output_pointer, kWidth, kHeight); } \
} \

// TEST_P(PlanarFunctionsTest, YUV2ToI420)
TEST_YUVTOI420(YUY2, 1.e-6, 2);
// TEST_P(PlanarFunctionsTest, UYVYToI420)
TEST_YUVTOI420(UYVY, 1.e-6, 2);

// A common macro for testing converting I420 to ARGB, BGRA and ABGR.
#define TEST_YUVTORGB(SRC_NAME, DST_NAME, JPG_TYPE, MSE, BLOCK_SIZE) \
TEST_F(PlanarFunctionsTest, SRC_NAME##To##DST_NAME) { \
  uint8 *y_pointer = NULL, *u_pointer = NULL, *v_pointer = NULL; \
  uint8 *argb_expected_pointer = NULL; \
  int y_pitch = kWidth; \
  int u_pitch = (kWidth + 1) >> 1; \
  int v_pitch = (kWidth + 1) >> 1; \
  /* Generate a fake input image.*/ \
  talk_base::scoped_ptr<uint8[]> yuv_input( \
      CreateFakeYuvTestingImage(kHeight, kWidth, BLOCK_SIZE, JPG_TYPE, \
                                y_pointer, u_pointer, v_pointer)); \
  /* Generate the expected output.*/ \
  talk_base::scoped_ptr<uint8[]> argb_expected( \
      CreateFakeArgbTestingImage(kHeight, kWidth, BLOCK_SIZE, \
                                 argb_expected_pointer, FOURCC_##DST_NAME)); \
  /* Allocate space for the output.*/ \
  talk_base::scoped_ptr<uint8[]> argb_output( \
    new uint8[kHeight * kWidth * 4 + kAlignment]); \
  uint8 *argb_pointer = ALIGNP(argb_expected.get(), kAlignment); \
  for (int i = 0; i < repeat_; ++i) { \
    libyuv::SRC_NAME##To##DST_NAME(y_pointer, y_pitch, \
                                   u_pointer, u_pitch, \
                                   v_pointer, v_pitch, \
                                   argb_pointer, \
                                   kWidth * 4, \
                                   kWidth, kHeight); \
  } \
  EXPECT_TRUE(IsMemoryEqual(argb_expected_pointer, argb_pointer, \
                            kHeight * kWidth * 4, MSE)); \
  if (dump_) { DumpArgbImage(argb_pointer, kWidth, kHeight); } \
}

// TEST_F(PlanarFunctionsTest, I420ToARGB)
TEST_YUVTORGB(I420, ARGB, libyuv::kJpegYuv420, 3., 2);
// TEST_F(PlanarFunctionsTest, I420ToABGR)
TEST_YUVTORGB(I420, ABGR, libyuv::kJpegYuv420, 3., 2);
// TEST_F(PlanarFunctionsTest, I420ToBGRA)
TEST_YUVTORGB(I420, BGRA, libyuv::kJpegYuv420, 3., 2);
// TEST_F(PlanarFunctionsTest, I422ToARGB)
TEST_YUVTORGB(I422, ARGB, libyuv::kJpegYuv422, 3., 2);
// TEST_F(PlanarFunctionsTest, I444ToARGB)
TEST_YUVTORGB(I444, ARGB, libyuv::kJpegYuv444, 3., 3);
// Note: an empirical MSE tolerance 3.0 is used here for the probable
// error from float-to-uint8 type conversion.

TEST_F(PlanarFunctionsTest, I400ToARGB_Reference) {
  uint8 *y_pointer = NULL, *u_pointer = NULL, *v_pointer = NULL;
  int y_pitch = kWidth;
  int u_pitch = (kWidth + 1) >> 1;
  int v_pitch = (kWidth + 1) >> 1;
  int block_size = 3;
  // Generate a fake input image.
  talk_base::scoped_ptr<uint8[]> yuv_input(
      CreateFakeYuvTestingImage(kHeight, kWidth, block_size,
                                libyuv::kJpegYuv420,
                                y_pointer, u_pointer, v_pointer));
  // As the comparison standard, we convert a grayscale image (by setting both
  // U and V channels to be 128) using an I420 converter.
  int uv_size = ((kHeight + 1) >> 1) * ((kWidth + 1) >> 1);

  talk_base::scoped_ptr<uint8[]> uv(new uint8[uv_size + kAlignment]);
  u_pointer = v_pointer = ALIGNP(uv.get(), kAlignment);
  memset(u_pointer, 128, uv_size);

  // Allocate space for the output image and generate the expected output.
  talk_base::scoped_ptr<uint8[]> argb_expected(
      new uint8[kHeight * kWidth * 4 + kAlignment]);
  talk_base::scoped_ptr<uint8[]> argb_output(
      new uint8[kHeight * kWidth * 4 + kAlignment]);
  uint8 *argb_expected_pointer = ALIGNP(argb_expected.get(), kAlignment);
  uint8 *argb_pointer = ALIGNP(argb_output.get(), kAlignment);

  libyuv::I420ToARGB(y_pointer, y_pitch,
                     u_pointer, u_pitch,
                     v_pointer, v_pitch,
                     argb_expected_pointer, kWidth * 4,
                     kWidth, kHeight);
  for (int i = 0; i < repeat_; ++i) {
    libyuv::I400ToARGB_Reference(y_pointer, y_pitch,
                                 argb_pointer, kWidth * 4,
                                 kWidth, kHeight);
  }

  // Note: I420ToARGB and I400ToARGB_Reference should produce identical results.
  EXPECT_TRUE(IsMemoryEqual(argb_expected_pointer, argb_pointer,
                            kHeight * kWidth * 4, 2.));
  if (dump_) { DumpArgbImage(argb_pointer, kWidth, kHeight); }
}

TEST_P(PlanarFunctionsTest, I400ToARGB) {
  // Get the unalignment offset
  int unalignment = GetParam();
  uint8 *y_pointer = NULL, *u_pointer = NULL, *v_pointer = NULL;
  int y_pitch = kWidth;
  int u_pitch = (kWidth + 1) >> 1;
  int v_pitch = (kWidth + 1) >> 1;
  int block_size = 3;
  // Generate a fake input image.
  talk_base::scoped_ptr<uint8[]> yuv_input(
      CreateFakeYuvTestingImage(kHeight, kWidth, block_size,
                                libyuv::kJpegYuv420,
                                y_pointer, u_pointer, v_pointer));
  // As the comparison standard, we convert a grayscale image (by setting both
  // U and V channels to be 128) using an I420 converter.
  int uv_size = ((kHeight + 1) >> 1) * ((kWidth + 1) >> 1);

  // 1 byte extra if in the unaligned mode.
  talk_base::scoped_ptr<uint8[]> uv(new uint8[uv_size * 2 + kAlignment]);
  u_pointer = ALIGNP(uv.get(), kAlignment);
  v_pointer = u_pointer + uv_size;
  memset(u_pointer, 128, uv_size);
  memset(v_pointer, 128, uv_size);

  // Allocate space for the output image and generate the expected output.
  talk_base::scoped_ptr<uint8[]> argb_expected(
      new uint8[kHeight * kWidth * 4 + kAlignment]);
  // 1 byte extra if in the misalinged mode.
  talk_base::scoped_ptr<uint8[]> argb_output(
      new uint8[kHeight * kWidth * 4 + kAlignment + unalignment]);
  uint8 *argb_expected_pointer = ALIGNP(argb_expected.get(), kAlignment);
  uint8 *argb_pointer = ALIGNP(argb_output.get(), kAlignment) + unalignment;

  libyuv::I420ToARGB(y_pointer, y_pitch,
                     u_pointer, u_pitch,
                     v_pointer, v_pitch,
                     argb_expected_pointer, kWidth * 4,
                     kWidth, kHeight);
  for (int i = 0; i < repeat_; ++i) {
    libyuv::I400ToARGB(y_pointer, y_pitch,
                       argb_pointer, kWidth * 4,
                       kWidth, kHeight);
  }

  // Note: current I400ToARGB uses an approximate method,
  // so the error tolerance is larger here.
  EXPECT_TRUE(IsMemoryEqual(argb_expected_pointer, argb_pointer,
                            kHeight * kWidth * 4, 64.0));
  if (dump_) { DumpArgbImage(argb_pointer, kWidth, kHeight); }
}

TEST_P(PlanarFunctionsTest, ARGBToI400) {
  // Get the unalignment offset
  int unalignment = GetParam();
  // Create a fake ARGB input image.
  uint8 *y_pointer = NULL, *u_pointer = NULL, *v_pointer = NULL;
  uint8 *argb_pointer = NULL;
  int block_size = 3;
  // Generate a fake input image.
  talk_base::scoped_ptr<uint8[]> argb_input(
      CreateFakeArgbTestingImage(kHeight, kWidth, block_size,
                                 argb_pointer, FOURCC_ARGB));
  // Generate the expected output. Only Y channel is used
  talk_base::scoped_ptr<uint8[]> yuv_expected(
      CreateFakeYuvTestingImage(kHeight, kWidth, block_size,
                                libyuv::kJpegYuv420,
                                y_pointer, u_pointer, v_pointer));
  // Allocate space for the Y output.
  talk_base::scoped_ptr<uint8[]> y_output(
    new uint8[kHeight * kWidth + kAlignment + unalignment]);
  uint8 *y_output_pointer = ALIGNP(y_output.get(), kAlignment) + unalignment;

  for (int i = 0; i < repeat_; ++i) {
    libyuv::ARGBToI400(argb_pointer, kWidth * 4, y_output_pointer, kWidth,
                       kWidth, kHeight);
  }
  // Check if the output matches the input Y channel.
  // Note: an empirical MSE tolerance 2.0 is used here for the probable
  // error from float-to-uint8 type conversion.
  EXPECT_TRUE(IsMemoryEqual(y_output_pointer, y_pointer,
                            kHeight * kWidth, 2.));
  if (dump_) { DumpArgbImage(argb_pointer, kWidth, kHeight); }
}

// A common macro for testing converting RAW, BG24, BGRA, and ABGR
// to ARGB.
#define TEST_ARGB(SRC_NAME, FC_ID, BPP, BLOCK_SIZE) \
TEST_P(PlanarFunctionsTest, SRC_NAME##ToARGB) { \
  int unalignment = GetParam();  /* Get the unalignment offset.*/ \
  uint8 *argb_expected_pointer = NULL, *src_pointer = NULL; \
  /* Generate a fake input image.*/ \
  talk_base::scoped_ptr<uint8[]> src_input(  \
      CreateFakeArgbTestingImage(kHeight, kWidth, BLOCK_SIZE, \
                                 src_pointer, FOURCC_##FC_ID)); \
  /* Generate the expected output.*/ \
  talk_base::scoped_ptr<uint8[]> argb_expected( \
      CreateFakeArgbTestingImage(kHeight, kWidth, BLOCK_SIZE, \
                                 argb_expected_pointer, FOURCC_ARGB)); \
  /* Allocate space for the output; 1 byte extra if in the unaligned mode.*/ \
  talk_base::scoped_ptr<uint8[]> argb_output( \
      new uint8[kHeight * kWidth * 4 + kAlignment + unalignment]); \
  uint8 *argb_pointer = ALIGNP(argb_output.get(), kAlignment) + unalignment; \
  for (int i = 0; i < repeat_; ++i) { \
    libyuv:: SRC_NAME##ToARGB(src_pointer, kWidth * (BPP), argb_pointer, \
                              kWidth * 4, kWidth, kHeight); \
  } \
  /* Compare the result; expect identical.*/ \
  EXPECT_TRUE(IsMemoryEqual(argb_expected_pointer, argb_pointer, \
                            kHeight * kWidth * 4, 1.e-6)); \
  if (dump_) { DumpArgbImage(argb_pointer, kWidth, kHeight); } \
}

TEST_ARGB(RAW, RAW, 3, 3);    // TEST_P(PlanarFunctionsTest, RAWToARGB)
TEST_ARGB(BG24, 24BG, 3, 3);  // TEST_P(PlanarFunctionsTest, BG24ToARGB)
TEST_ARGB(ABGR, ABGR, 4, 3);  // TEST_P(PlanarFunctionsTest, ABGRToARGB)
TEST_ARGB(BGRA, BGRA, 4, 3);  // TEST_P(PlanarFunctionsTest, BGRAToARGB)

// Parameter Test: The parameter is the unalignment offset.
// Aligned data for testing assembly versions.
INSTANTIATE_TEST_CASE_P(PlanarFunctionsAligned, PlanarFunctionsTest,
    ::testing::Values(0));

// Purposely unalign the output argb pointer to test slow path (C version).
INSTANTIATE_TEST_CASE_P(PlanarFunctionsMisaligned, PlanarFunctionsTest,
    ::testing::Values(1));

}  // namespace cricket
