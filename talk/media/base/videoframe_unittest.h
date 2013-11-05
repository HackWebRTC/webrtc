/*
 * libjingle
 * Copyright 2004 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_MEDIA_BASE_VIDEOFRAME_UNITTEST_H_
#define TALK_MEDIA_BASE_VIDEOFRAME_UNITTEST_H_

#include <string>

#include "libyuv/convert.h"
#include "libyuv/convert_from.h"
#include "libyuv/format_conversion.h"
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"
#include "talk/base/gunit.h"
#include "talk/base/pathutils.h"
#include "talk/base/stream.h"
#include "talk/base/stringutils.h"
#include "talk/media/base/testutils.h"
#include "talk/media/base/videocommon.h"
#include "talk/media/base/videoframe.h"

#if defined(_MSC_VER)
#define ALIGN16(var) __declspec(align(16)) var
#else
#define ALIGN16(var) var __attribute__((aligned(16)))
#endif

#define kImageFilename "faces.1280x720_P420.yuv"
#define kJpeg420Filename "faces_I420.jpg"
#define kJpeg422Filename "faces_I422.jpg"
#define kJpeg444Filename "faces_I444.jpg"
#define kJpeg411Filename "faces_I411.jpg"
#define kJpeg400Filename "faces_I400.jpg"

// Generic test class for testing various video frame implementations.
template <class T>
class VideoFrameTest : public testing::Test {
 public:
  VideoFrameTest() : repeat_(1) {}

 protected:
  static const int kWidth = 1280;
  static const int kHeight = 720;
  static const int kAlignment = 16;
  static const int kMinWidthAll = 1;  // Constants for ConstructYUY2AllSizes.
  static const int kMinHeightAll = 1;
  static const int kMaxWidthAll = 17;
  static const int kMaxHeightAll = 23;

  // Load a video frame from disk.
  bool LoadFrameNoRepeat(T* frame) {
    int save_repeat = repeat_;  // This LoadFrame disables repeat.
    repeat_ = 1;
    bool success = LoadFrame(kImageFilename, cricket::FOURCC_I420,
                            kWidth, kHeight, frame);
    repeat_ = save_repeat;
    return success;
  }

  bool LoadFrame(const std::string& filename, uint32 format,
                 int32 width, int32 height, T* frame) {
    return LoadFrame(filename, format, width, height,
                     width, abs(height), 0, frame);
  }
  bool LoadFrame(const std::string& filename, uint32 format,
                 int32 width, int32 height, int dw, int dh, int rotation,
                 T* frame) {
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(LoadSample(filename));
    return LoadFrame(ms.get(), format, width, height, dw, dh, rotation, frame);
  }
  // Load a video frame from a memory stream.
  bool LoadFrame(talk_base::MemoryStream* ms, uint32 format,
                 int32 width, int32 height, T* frame) {
    return LoadFrame(ms, format, width, height,
                     width, abs(height), 0, frame);
  }
  bool LoadFrame(talk_base::MemoryStream* ms, uint32 format,
                 int32 width, int32 height, int dw, int dh, int rotation,
                 T* frame) {
    if (!ms) {
      return false;
    }
    size_t data_size;
    bool ret = ms->GetSize(&data_size);
    EXPECT_TRUE(ret);
    if (ret) {
      ret = LoadFrame(reinterpret_cast<uint8*>(ms->GetBuffer()), data_size,
                      format, width, height, dw, dh, rotation, frame);
    }
    return ret;
  }
  // Load a frame from a raw buffer.
  bool LoadFrame(uint8* sample, size_t sample_size, uint32 format,
                 int32 width, int32 height, T* frame) {
    return LoadFrame(sample, sample_size, format, width, height,
                     width, abs(height), 0, frame);
  }
  bool LoadFrame(uint8* sample, size_t sample_size, uint32 format,
                 int32 width, int32 height, int dw, int dh, int rotation,
                 T* frame) {
    bool ret = false;
    for (int i = 0; i < repeat_; ++i) {
      ret = frame->Init(format, width, height, dw, dh,
                        sample, sample_size, 1, 1, 0, 0, rotation);
    }
    return ret;
  }

  talk_base::MemoryStream* LoadSample(const std::string& filename) {
    talk_base::Pathname path(cricket::GetTestFilePath(filename));
    talk_base::scoped_ptr<talk_base::FileStream> fs(
        talk_base::Filesystem::OpenFile(path, "rb"));
    if (!fs.get()) {
      return NULL;
    }

    char buf[4096];
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        new talk_base::MemoryStream());
    talk_base::StreamResult res = Flow(fs.get(), buf, sizeof(buf), ms.get());
    if (res != talk_base::SR_SUCCESS) {
      return NULL;
    }

    return ms.release();
  }

  // Write an I420 frame out to disk.
  bool DumpFrame(const std::string& prefix,
                 const cricket::VideoFrame& frame) {
    char filename[256];
    talk_base::sprintfn(filename, sizeof(filename), "%s.%dx%d_P420.yuv",
                        prefix.c_str(), frame.GetWidth(), frame.GetHeight());
    size_t out_size = cricket::VideoFrame::SizeOf(frame.GetWidth(),
                                                  frame.GetHeight());
    talk_base::scoped_ptr<uint8[]> out(new uint8[out_size]);
    frame.CopyToBuffer(out.get(), out_size);
    return DumpSample(filename, out.get(), out_size);
  }

  bool DumpSample(const std::string& filename, const void* buffer, int size) {
    talk_base::Pathname path(filename);
    talk_base::scoped_ptr<talk_base::FileStream> fs(
        talk_base::Filesystem::OpenFile(path, "wb"));
    if (!fs.get()) {
      return false;
    }

    return (fs->Write(buffer, size, NULL, NULL) == talk_base::SR_SUCCESS);
  }

  // Create a test image in the desired color space.
  // The image is a checkerboard pattern with 63x63 squares, which allows
  // I420 chroma artifacts to easily be seen on the square boundaries.
  // The pattern is { { green, orange }, { blue, purple } }
  // There is also a gradient within each square to ensure that the luma
  // values are handled properly.
  talk_base::MemoryStream* CreateYuv422Sample(uint32 fourcc,
                                              uint32 width, uint32 height) {
    int y1_pos, y2_pos, u_pos, v_pos;
    if (!GetYuv422Packing(fourcc, &y1_pos, &y2_pos, &u_pos, &v_pos)) {
      return NULL;
    }

    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        new talk_base::MemoryStream);
    int awidth = (width + 1) & ~1;
    int size = awidth * 2 * height;
    if (!ms->ReserveSize(size)) {
      return NULL;
    }
    for (uint32 y = 0; y < height; ++y) {
      for (int x = 0; x < awidth; x += 2) {
        uint8 quad[4];
        quad[y1_pos] = (x % 63 + y % 63) + 64;
        quad[y2_pos] = ((x + 1) % 63 + y % 63) + 64;
        quad[u_pos] = ((x / 63) & 1) ? 192 : 64;
        quad[v_pos] = ((y / 63) & 1) ? 192 : 64;
        ms->Write(quad, sizeof(quad), NULL, NULL);
      }
    }
    return ms.release();
  }

  // Create a test image for YUV 420 formats with 12 bits per pixel.
  talk_base::MemoryStream* CreateYuvSample(uint32 width, uint32 height,
                                           uint32 bpp) {
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        new talk_base::MemoryStream);
    if (!ms->ReserveSize(width * height * bpp / 8)) {
      return NULL;
    }

    for (uint32 i = 0; i < width * height * bpp / 8; ++i) {
      char value = ((i / 63) & 1) ? 192 : 64;
      ms->Write(&value, sizeof(value), NULL, NULL);
    }
    return ms.release();
  }

  talk_base::MemoryStream* CreateRgbSample(uint32 fourcc,
                                           uint32 width, uint32 height) {
    int r_pos, g_pos, b_pos, bytes;
    if (!GetRgbPacking(fourcc, &r_pos, &g_pos, &b_pos, &bytes)) {
      return NULL;
    }

    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        new talk_base::MemoryStream);
    if (!ms->ReserveSize(width * height * bytes)) {
      return NULL;
    }

    for (uint32 y = 0; y < height; ++y) {
      for (uint32 x = 0; x < width; ++x) {
        uint8 rgb[4] = { 255, 255, 255, 255 };
        rgb[r_pos] = ((x / 63) & 1) ? 224 : 32;
        rgb[g_pos] = (x % 63 + y % 63) + 96;
        rgb[b_pos] = ((y / 63) & 1) ? 224 : 32;
        ms->Write(rgb, bytes, NULL, NULL);
      }
    }
    return ms.release();
  }

  // Simple conversion routines to verify the optimized VideoFrame routines.
  // Converts from the specified colorspace to I420.
  bool ConvertYuv422(const talk_base::MemoryStream* ms,
                     uint32 fourcc, uint32 width, uint32 height,
                     T* frame) {
    int y1_pos, y2_pos, u_pos, v_pos;
    if (!GetYuv422Packing(fourcc, &y1_pos, &y2_pos, &u_pos, &v_pos)) {
      return false;
    }

    const uint8* start = reinterpret_cast<const uint8*>(ms->GetBuffer());
    int awidth = (width + 1) & ~1;
    frame->InitToBlack(width, height, 1, 1, 0, 0);
    int stride_y = frame->GetYPitch();
    int stride_u = frame->GetUPitch();
    int stride_v = frame->GetVPitch();
    for (uint32 y = 0; y < height; ++y) {
      for (uint32 x = 0; x < width; x += 2) {
        const uint8* quad1 = start + (y * awidth + x) * 2;
        frame->GetYPlane()[stride_y * y + x] = quad1[y1_pos];
        if ((x + 1) < width) {
          frame->GetYPlane()[stride_y * y + x + 1] = quad1[y2_pos];
        }
        if ((y & 1) == 0) {
          const uint8* quad2 = quad1 + awidth * 2;
          if ((y + 1) >= height) {
            quad2 = quad1;
          }
          frame->GetUPlane()[stride_u * (y / 2) + x / 2] =
              (quad1[u_pos] + quad2[u_pos] + 1) / 2;
          frame->GetVPlane()[stride_v * (y / 2) + x / 2] =
              (quad1[v_pos] + quad2[v_pos] + 1) / 2;
        }
      }
    }
    return true;
  }

  // Convert RGB to 420.
  // A negative height inverts the image.
  bool ConvertRgb(const talk_base::MemoryStream* ms,
                  uint32 fourcc, int32 width, int32 height,
                  T* frame) {
    int r_pos, g_pos, b_pos, bytes;
    if (!GetRgbPacking(fourcc, &r_pos, &g_pos, &b_pos, &bytes)) {
      return false;
    }
    int pitch = width * bytes;
    const uint8* start = reinterpret_cast<const uint8*>(ms->GetBuffer());
    if (height < 0) {
      height = -height;
      start = start + pitch * (height - 1);
      pitch = -pitch;
    }
    frame->InitToBlack(width, height, 1, 1, 0, 0);
    int stride_y = frame->GetYPitch();
    int stride_u = frame->GetUPitch();
    int stride_v = frame->GetVPitch();
    for (int32 y = 0; y < height; y += 2) {
      for (int32 x = 0; x < width; x += 2) {
        const uint8* rgb[4];
        uint8 yuv[4][3];
        rgb[0] = start + y * pitch + x * bytes;
        rgb[1] = rgb[0] + ((x + 1) < width ? bytes : 0);
        rgb[2] = rgb[0] + ((y + 1) < height ? pitch : 0);
        rgb[3] = rgb[2] + ((x + 1) < width ? bytes : 0);
        for (size_t i = 0; i < 4; ++i) {
          ConvertRgbPixel(rgb[i][r_pos], rgb[i][g_pos], rgb[i][b_pos],
                          &yuv[i][0], &yuv[i][1], &yuv[i][2]);
        }
        frame->GetYPlane()[stride_y * y + x] = yuv[0][0];
        if ((x + 1) < width) {
          frame->GetYPlane()[stride_y * y + x + 1] = yuv[1][0];
        }
        if ((y + 1) < height) {
          frame->GetYPlane()[stride_y * (y + 1) + x] = yuv[2][0];
          if ((x + 1) < width) {
            frame->GetYPlane()[stride_y * (y + 1) + x + 1] = yuv[3][0];
          }
        }
        frame->GetUPlane()[stride_u * (y / 2) + x / 2] =
            (yuv[0][1] + yuv[1][1] + yuv[2][1] + yuv[3][1] + 2) / 4;
        frame->GetVPlane()[stride_v * (y / 2) + x / 2] =
            (yuv[0][2] + yuv[1][2] + yuv[2][2] + yuv[3][2] + 2) / 4;
      }
    }
    return true;
  }

  // Simple and slow RGB->YUV conversion. From NTSC standard, c/o Wikipedia.
  void ConvertRgbPixel(uint8 r, uint8 g, uint8 b,
                       uint8* y, uint8* u, uint8* v) {
    *y = static_cast<int>(.257 * r + .504 * g + .098 * b) + 16;
    *u = static_cast<int>(-.148 * r - .291 * g + .439 * b) + 128;
    *v = static_cast<int>(.439 * r - .368 * g - .071 * b) + 128;
  }

  bool GetYuv422Packing(uint32 fourcc,
                        int* y1_pos, int* y2_pos, int* u_pos, int* v_pos) {
    if (fourcc == cricket::FOURCC_YUY2) {
      *y1_pos = 0; *u_pos = 1; *y2_pos = 2; *v_pos = 3;
    } else if (fourcc == cricket::FOURCC_UYVY) {
      *u_pos = 0; *y1_pos = 1; *v_pos = 2; *y2_pos = 3;
    } else {
      return false;
    }
    return true;
  }

  bool GetRgbPacking(uint32 fourcc,
                     int* r_pos, int* g_pos, int* b_pos, int* bytes) {
    if (fourcc == cricket::FOURCC_RAW) {
      *r_pos = 0; *g_pos = 1; *b_pos = 2; *bytes = 3;  // RGB in memory.
    } else if (fourcc == cricket::FOURCC_24BG) {
      *r_pos = 2; *g_pos = 1; *b_pos = 0; *bytes = 3;  // BGR in memory.
    } else if (fourcc == cricket::FOURCC_ABGR) {
      *r_pos = 0; *g_pos = 1; *b_pos = 2; *bytes = 4;  // RGBA in memory.
    } else if (fourcc == cricket::FOURCC_BGRA) {
      *r_pos = 1; *g_pos = 2; *b_pos = 3; *bytes = 4;  // ARGB in memory.
    } else if (fourcc == cricket::FOURCC_ARGB) {
      *r_pos = 2; *g_pos = 1; *b_pos = 0; *bytes = 4;  // BGRA in memory.
    } else {
      return false;
    }
    return true;
  }

  // Comparison functions for testing.
  static bool IsNull(const cricket::VideoFrame& frame) {
    return !frame.GetYPlane();
  }

  static bool IsSize(const cricket::VideoFrame& frame,
                     uint32 width, uint32 height) {
    return !IsNull(frame) &&
        frame.GetYPitch() >= static_cast<int32>(width) &&
        frame.GetUPitch() >= static_cast<int32>(width) / 2 &&
        frame.GetVPitch() >= static_cast<int32>(width) / 2 &&
        frame.GetWidth() == width && frame.GetHeight() == height;
  }

  static bool IsPlaneEqual(const std::string& name,
                           const uint8* plane1, uint32 pitch1,
                           const uint8* plane2, uint32 pitch2,
                           uint32 width, uint32 height,
                           int max_error) {
    const uint8* r1 = plane1;
    const uint8* r2 = plane2;
    for (uint32 y = 0; y < height; ++y) {
      for (uint32 x = 0; x < width; ++x) {
        if (abs(static_cast<int>(r1[x] - r2[x])) > max_error) {
          LOG(LS_INFO) << "IsPlaneEqual(" << name << "): pixel["
                       << x << "," << y << "] differs: "
                       << static_cast<int>(r1[x]) << " vs "
                       << static_cast<int>(r2[x]);
          return false;
        }
      }
      r1 += pitch1;
      r2 += pitch2;
    }
    return true;
  }

  static bool IsEqual(const cricket::VideoFrame& frame,
                      size_t width, size_t height,
                      size_t pixel_width, size_t pixel_height,
                      int64 elapsed_time, int64 time_stamp,
                      const uint8* y, uint32 ypitch,
                      const uint8* u, uint32 upitch,
                      const uint8* v, uint32 vpitch,
                      int max_error) {
    return IsSize(frame, width, height) &&
        frame.GetPixelWidth() == pixel_width &&
        frame.GetPixelHeight() == pixel_height &&
        frame.GetElapsedTime() == elapsed_time &&
        frame.GetTimeStamp() == time_stamp &&
        IsPlaneEqual("y", frame.GetYPlane(), frame.GetYPitch(), y, ypitch,
                     width, height, max_error) &&
        IsPlaneEqual("u", frame.GetUPlane(), frame.GetUPitch(), u, upitch,
                     (width + 1) / 2, (height + 1) / 2, max_error) &&
        IsPlaneEqual("v", frame.GetVPlane(), frame.GetVPitch(), v, vpitch,
                     (width + 1) / 2, (height + 1) / 2, max_error);
  }

  static bool IsEqual(const cricket::VideoFrame& frame1,
                      const cricket::VideoFrame& frame2,
                      int max_error) {
    return IsEqual(frame1,
                   frame2.GetWidth(), frame2.GetHeight(),
                   frame2.GetPixelWidth(), frame2.GetPixelHeight(),
                   frame2.GetElapsedTime(), frame2.GetTimeStamp(),
                   frame2.GetYPlane(), frame2.GetYPitch(),
                   frame2.GetUPlane(), frame2.GetUPitch(),
                   frame2.GetVPlane(), frame2.GetVPitch(),
                   max_error);
  }

  static bool IsEqualWithCrop(const cricket::VideoFrame& frame1,
                              const cricket::VideoFrame& frame2,
                              int hcrop, int vcrop, int max_error) {
    return frame1.GetWidth() <= frame2.GetWidth() &&
           frame1.GetHeight() <= frame2.GetHeight() &&
           IsEqual(frame1,
                   frame2.GetWidth() - hcrop * 2,
                   frame2.GetHeight() - vcrop * 2,
                   frame2.GetPixelWidth(), frame2.GetPixelHeight(),
                   frame2.GetElapsedTime(), frame2.GetTimeStamp(),
                   frame2.GetYPlane() + vcrop * frame2.GetYPitch()
                       + hcrop,
                   frame2.GetYPitch(),
                   frame2.GetUPlane() + vcrop * frame2.GetUPitch() / 2
                       + hcrop / 2,
                   frame2.GetUPitch(),
                   frame2.GetVPlane() + vcrop * frame2.GetVPitch() / 2
                       + hcrop / 2,
                   frame2.GetVPitch(),
                   max_error);
  }

  static bool IsBlack(const cricket::VideoFrame& frame) {
    return !IsNull(frame) &&
        *frame.GetYPlane() == 16 &&
        *frame.GetUPlane() == 128 &&
        *frame.GetVPlane() == 128;
  }

  ////////////////////////
  // Construction tests //
  ////////////////////////

  // Test constructing an image from a I420 buffer.
  void ConstructI420() {
    T frame;
    EXPECT_TRUE(IsNull(frame));
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuvSample(kWidth, kHeight, 12));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_I420,
                          kWidth, kHeight, &frame));

    const uint8* y = reinterpret_cast<uint8*>(ms.get()->GetBuffer());
    const uint8* u = y + kWidth * kHeight;
    const uint8* v = u + kWidth * kHeight / 4;
    EXPECT_TRUE(IsEqual(frame, kWidth, kHeight, 1, 1, 0, 0,
                        y, kWidth, u, kWidth / 2, v, kWidth / 2, 0));
  }

  // Test constructing an image from a YV12 buffer.
  void ConstructYV12() {
    T frame;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuvSample(kWidth, kHeight, 12));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_YV12,
                          kWidth, kHeight, &frame));

    const uint8* y = reinterpret_cast<uint8*>(ms.get()->GetBuffer());
    const uint8* v = y + kWidth * kHeight;
    const uint8* u = v + kWidth * kHeight / 4;
    EXPECT_TRUE(IsEqual(frame, kWidth, kHeight, 1, 1, 0, 0,
                        y, kWidth, u, kWidth / 2, v, kWidth / 2, 0));
  }

  // Test constructing an image from a I422 buffer.
  void ConstructI422() {
    T frame1, frame2;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    size_t buf_size = kWidth * kHeight * 2;
    talk_base::scoped_ptr<uint8[]> buf(new uint8[buf_size + kAlignment]);
    uint8* y = ALIGNP(buf.get(), kAlignment);
    uint8* u = y + kWidth * kHeight;
    uint8* v = u + (kWidth / 2) * kHeight;
    EXPECT_EQ(0, libyuv::I420ToI422(frame1.GetYPlane(), frame1.GetYPitch(),
                                    frame1.GetUPlane(), frame1.GetUPitch(),
                                    frame1.GetVPlane(), frame1.GetVPitch(),
                                    y, kWidth,
                                    u, kWidth / 2,
                                    v, kWidth / 2,
                                    kWidth, kHeight));
    EXPECT_TRUE(LoadFrame(y, buf_size, cricket::FOURCC_I422,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 0));
  }

  // Test constructing an image from a YUY2 buffer.
  void ConstructYuy2() {
    T frame1, frame2;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    size_t buf_size = kWidth * kHeight * 2;
    talk_base::scoped_ptr<uint8[]> buf(new uint8[buf_size + kAlignment]);
    uint8* yuy2 = ALIGNP(buf.get(), kAlignment);
    EXPECT_EQ(0, libyuv::I420ToYUY2(frame1.GetYPlane(), frame1.GetYPitch(),
                                    frame1.GetUPlane(), frame1.GetUPitch(),
                                    frame1.GetVPlane(), frame1.GetVPitch(),
                                    yuy2, kWidth * 2,
                                    kWidth, kHeight));
    EXPECT_TRUE(LoadFrame(yuy2, buf_size, cricket::FOURCC_YUY2,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 0));
  }

  // Test constructing an image from a YUY2 buffer with buffer unaligned.
  void ConstructYuy2Unaligned() {
    T frame1, frame2;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    size_t buf_size = kWidth * kHeight * 2;
    talk_base::scoped_ptr<uint8[]> buf(new uint8[buf_size + kAlignment + 1]);
    uint8* yuy2 = ALIGNP(buf.get(), kAlignment) + 1;
    EXPECT_EQ(0, libyuv::I420ToYUY2(frame1.GetYPlane(), frame1.GetYPitch(),
                                    frame1.GetUPlane(), frame1.GetUPitch(),
                                    frame1.GetVPlane(), frame1.GetVPitch(),
                                    yuy2, kWidth * 2,
                                    kWidth, kHeight));
    EXPECT_TRUE(LoadFrame(yuy2, buf_size, cricket::FOURCC_YUY2,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 0));
  }

  // Test constructing an image from a wide YUY2 buffer.
  // Normal is 1280x720.  Wide is 12800x72
  void ConstructYuy2Wide() {
    T frame1, frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuv422Sample(cricket::FOURCC_YUY2, kWidth * 10, kHeight / 10));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(ConvertYuv422(ms.get(), cricket::FOURCC_YUY2,
                              kWidth * 10, kHeight / 10,
                              &frame1));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_YUY2,
                          kWidth * 10, kHeight / 10, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 0));
  }

  // Test constructing an image from a UYVY buffer.
  void ConstructUyvy() {
    T frame1, frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuv422Sample(cricket::FOURCC_UYVY, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(ConvertYuv422(ms.get(), cricket::FOURCC_UYVY, kWidth, kHeight,
                              &frame1));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_UYVY,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 0));
  }

  // Test constructing an image from a random buffer.
  // We are merely verifying that the code succeeds and is free of crashes.
  void ConstructM420() {
    T frame;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuvSample(kWidth, kHeight, 12));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_M420,
                          kWidth, kHeight, &frame));
  }

  void ConstructQ420() {
    T frame;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuvSample(kWidth, kHeight, 12));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_Q420,
                          kWidth, kHeight, &frame));
  }

  void ConstructNV21() {
    T frame;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuvSample(kWidth, kHeight, 12));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_NV21,
                          kWidth, kHeight, &frame));
  }

  void ConstructNV12() {
    T frame;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuvSample(kWidth, kHeight, 12));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_NV12,
                          kWidth, kHeight, &frame));
  }

  // Test constructing an image from a ABGR buffer
  // Due to rounding, some pixels may differ slightly from the VideoFrame impl.
  void ConstructABGR() {
    T frame1, frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateRgbSample(cricket::FOURCC_ABGR, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(ConvertRgb(ms.get(), cricket::FOURCC_ABGR, kWidth, kHeight,
                           &frame1));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_ABGR,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 2));
  }

  // Test constructing an image from a ARGB buffer
  // Due to rounding, some pixels may differ slightly from the VideoFrame impl.
  void ConstructARGB() {
    T frame1, frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateRgbSample(cricket::FOURCC_ARGB, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(ConvertRgb(ms.get(), cricket::FOURCC_ARGB, kWidth, kHeight,
                           &frame1));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_ARGB,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 2));
  }

  // Test constructing an image from a wide ARGB buffer
  // Normal is 1280x720.  Wide is 12800x72
  void ConstructARGBWide() {
    T frame1, frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateRgbSample(cricket::FOURCC_ARGB, kWidth * 10, kHeight / 10));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(ConvertRgb(ms.get(), cricket::FOURCC_ARGB,
                           kWidth * 10, kHeight / 10, &frame1));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_ARGB,
                          kWidth * 10, kHeight / 10, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 2));
  }

  // Test constructing an image from an BGRA buffer.
  // Due to rounding, some pixels may differ slightly from the VideoFrame impl.
  void ConstructBGRA() {
    T frame1, frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateRgbSample(cricket::FOURCC_BGRA, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(ConvertRgb(ms.get(), cricket::FOURCC_BGRA, kWidth, kHeight,
                           &frame1));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_BGRA,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 2));
  }

  // Test constructing an image from a 24BG buffer.
  // Due to rounding, some pixels may differ slightly from the VideoFrame impl.
  void Construct24BG() {
    T frame1, frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateRgbSample(cricket::FOURCC_24BG, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(ConvertRgb(ms.get(), cricket::FOURCC_24BG, kWidth, kHeight,
                           &frame1));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_24BG,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 2));
  }

  // Test constructing an image from a raw RGB buffer.
  // Due to rounding, some pixels may differ slightly from the VideoFrame impl.
  void ConstructRaw() {
    T frame1, frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateRgbSample(cricket::FOURCC_RAW, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(ConvertRgb(ms.get(), cricket::FOURCC_RAW, kWidth, kHeight,
                           &frame1));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_RAW,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 2));
  }

  // Test constructing an image from a RGB565 buffer
  void ConstructRGB565() {
    T frame1, frame2;
    size_t out_size = kWidth * kHeight * 2;
    talk_base::scoped_ptr<uint8[]> outbuf(new uint8[out_size + kAlignment]);
    uint8 *out = ALIGNP(outbuf.get(), kAlignment);
    T frame;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    EXPECT_EQ(out_size, frame1.ConvertToRgbBuffer(cricket::FOURCC_RGBP,
                                                 out,
                                                 out_size, kWidth * 2));
    EXPECT_TRUE(LoadFrame(out, out_size, cricket::FOURCC_RGBP,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 20));
  }

  // Test constructing an image from a ARGB1555 buffer
  void ConstructARGB1555() {
    T frame1, frame2;
    size_t out_size = kWidth * kHeight * 2;
    talk_base::scoped_ptr<uint8[]> outbuf(new uint8[out_size + kAlignment]);
    uint8 *out = ALIGNP(outbuf.get(), kAlignment);
    T frame;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    EXPECT_EQ(out_size, frame1.ConvertToRgbBuffer(cricket::FOURCC_RGBO,
                                                 out,
                                                 out_size, kWidth * 2));
    EXPECT_TRUE(LoadFrame(out, out_size, cricket::FOURCC_RGBO,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 20));
  }

  // Test constructing an image from a ARGB4444 buffer
  void ConstructARGB4444() {
    T frame1, frame2;
    size_t out_size = kWidth * kHeight * 2;
    talk_base::scoped_ptr<uint8[]> outbuf(new uint8[out_size + kAlignment]);
    uint8 *out = ALIGNP(outbuf.get(), kAlignment);
    T frame;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    EXPECT_EQ(out_size, frame1.ConvertToRgbBuffer(cricket::FOURCC_R444,
                                                 out,
                                                 out_size, kWidth * 2));
    EXPECT_TRUE(LoadFrame(out, out_size, cricket::FOURCC_R444,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 20));
  }

  // Macro to help test different Bayer formats.
  // Error threshold of 60 allows for Bayer format subsampling.
  // TODO(fbarchard): Refactor this test to go from Bayer to I420 and
  // back to bayer, which would be less lossy.
  #define TEST_BYR(NAME, BAYER)                                                \
  void NAME() {                                                                \
    size_t bayer_size = kWidth * kHeight;                                      \
    talk_base::scoped_ptr<uint8[]> bayerbuf(new uint8[                         \
        bayer_size + kAlignment]);                                             \
    uint8 *bayer = ALIGNP(bayerbuf.get(), kAlignment);                         \
    T frame1, frame2;                                                          \
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(                         \
        CreateRgbSample(cricket::FOURCC_ARGB, kWidth, kHeight));               \
    ASSERT_TRUE(ms.get() != NULL);                                             \
    libyuv::ARGBToBayer##BAYER(reinterpret_cast<uint8 *>(ms->GetBuffer()),     \
                               kWidth * 4,                                     \
                               bayer, kWidth,                                  \
                               kWidth, kHeight);                               \
    EXPECT_TRUE(LoadFrame(bayer, bayer_size, cricket::FOURCC_##BAYER,          \
                          kWidth, kHeight,  &frame1));                         \
    EXPECT_TRUE(ConvertRgb(ms.get(), cricket::FOURCC_ARGB, kWidth, kHeight,    \
                           &frame2));                                          \
    EXPECT_TRUE(IsEqual(frame1, frame2, 60));                                  \
  }

  // Test constructing an image from Bayer formats.
  TEST_BYR(ConstructBayerGRBG, GRBG)
  TEST_BYR(ConstructBayerGBRG, GBRG)
  TEST_BYR(ConstructBayerBGGR, BGGR)
  TEST_BYR(ConstructBayerRGGB, RGGB)


// Macro to help test different rotations
#define TEST_MIRROR(FOURCC, BPP)                                               \
void Construct##FOURCC##Mirror() {                                             \
    T frame1, frame2, frame3;                                                  \
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(                         \
        CreateYuvSample(kWidth, kHeight, BPP));                                \
    ASSERT_TRUE(ms.get() != NULL);                                             \
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_##FOURCC,                  \
                          kWidth, -kHeight, kWidth, kHeight,                   \
                          cricket::ROTATION_180, &frame1));                    \
    size_t data_size;                                                          \
    bool ret = ms->GetSize(&data_size);                                        \
    EXPECT_TRUE(ret);                                                          \
    EXPECT_TRUE(frame2.Init(cricket::FOURCC_##FOURCC,                          \
                            kWidth, kHeight, kWidth, kHeight,                  \
                            reinterpret_cast<uint8*>(ms->GetBuffer()),         \
                            data_size,                                         \
                            1, 1, 0, 0, 0));                                   \
    int width_rotate = frame1.GetWidth();                                      \
    int height_rotate = frame1.GetHeight();                                    \
    EXPECT_TRUE(frame3.InitToBlack(width_rotate, height_rotate, 1, 1, 0, 0));  \
    libyuv::I420Mirror(frame2.GetYPlane(), frame2.GetYPitch(),                 \
                       frame2.GetUPlane(), frame2.GetUPitch(),                 \
                       frame2.GetVPlane(), frame2.GetVPitch(),                 \
                       frame3.GetYPlane(), frame3.GetYPitch(),                 \
                       frame3.GetUPlane(), frame3.GetUPitch(),                 \
                       frame3.GetVPlane(), frame3.GetVPitch(),                 \
                       kWidth, kHeight);                                       \
    EXPECT_TRUE(IsEqual(frame1, frame3, 0));                                   \
  }

  TEST_MIRROR(I420, 420)

// Macro to help test different rotations
#define TEST_ROTATE(FOURCC, BPP, ROTATE)                                       \
void Construct##FOURCC##Rotate##ROTATE() {                                     \
    T frame1, frame2, frame3;                                                  \
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(                         \
        CreateYuvSample(kWidth, kHeight, BPP));                                \
    ASSERT_TRUE(ms.get() != NULL);                                             \
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_##FOURCC,                  \
                          kWidth, kHeight, kWidth, kHeight,                    \
                          cricket::ROTATION_##ROTATE, &frame1));               \
    size_t data_size;                                                          \
    bool ret = ms->GetSize(&data_size);                                        \
    EXPECT_TRUE(ret);                                                          \
    EXPECT_TRUE(frame2.Init(cricket::FOURCC_##FOURCC,                          \
                            kWidth, kHeight, kWidth, kHeight,                  \
                            reinterpret_cast<uint8*>(ms->GetBuffer()),         \
                            data_size,                                         \
                            1, 1, 0, 0, 0));                                   \
    int width_rotate = frame1.GetWidth();                                      \
    int height_rotate = frame1.GetHeight();                                    \
    EXPECT_TRUE(frame3.InitToBlack(width_rotate, height_rotate, 1, 1, 0, 0));  \
    libyuv::I420Rotate(frame2.GetYPlane(), frame2.GetYPitch(),                 \
                       frame2.GetUPlane(), frame2.GetUPitch(),                 \
                       frame2.GetVPlane(), frame2.GetVPitch(),                 \
                       frame3.GetYPlane(), frame3.GetYPitch(),                 \
                       frame3.GetUPlane(), frame3.GetUPitch(),                 \
                       frame3.GetVPlane(), frame3.GetVPitch(),                 \
                       kWidth, kHeight, libyuv::kRotate##ROTATE);              \
    EXPECT_TRUE(IsEqual(frame1, frame3, 0));                                   \
  }

  // Test constructing an image with rotation.
  TEST_ROTATE(I420, 12, 0)
  TEST_ROTATE(I420, 12, 90)
  TEST_ROTATE(I420, 12, 180)
  TEST_ROTATE(I420, 12, 270)
  TEST_ROTATE(YV12, 12, 0)
  TEST_ROTATE(YV12, 12, 90)
  TEST_ROTATE(YV12, 12, 180)
  TEST_ROTATE(YV12, 12, 270)
  TEST_ROTATE(NV12, 12, 0)
  TEST_ROTATE(NV12, 12, 90)
  TEST_ROTATE(NV12, 12, 180)
  TEST_ROTATE(NV12, 12, 270)
  TEST_ROTATE(NV21, 12, 0)
  TEST_ROTATE(NV21, 12, 90)
  TEST_ROTATE(NV21, 12, 180)
  TEST_ROTATE(NV21, 12, 270)
  TEST_ROTATE(UYVY, 16, 0)
  TEST_ROTATE(UYVY, 16, 90)
  TEST_ROTATE(UYVY, 16, 180)
  TEST_ROTATE(UYVY, 16, 270)
  TEST_ROTATE(YUY2, 16, 0)
  TEST_ROTATE(YUY2, 16, 90)
  TEST_ROTATE(YUY2, 16, 180)
  TEST_ROTATE(YUY2, 16, 270)

  // Test constructing an image from a UYVY buffer rotated 90 degrees.
  void ConstructUyvyRotate90() {
    T frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuv422Sample(cricket::FOURCC_UYVY, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_UYVY,
                          kWidth, kHeight, kWidth, kHeight,
                          cricket::ROTATION_90, &frame2));
  }

  // Test constructing an image from a UYVY buffer rotated 180 degrees.
  void ConstructUyvyRotate180() {
    T frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuv422Sample(cricket::FOURCC_UYVY, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_UYVY,
                          kWidth, kHeight, kWidth, kHeight,
                          cricket::ROTATION_180, &frame2));
  }

  // Test constructing an image from a UYVY buffer rotated 270 degrees.
  void ConstructUyvyRotate270() {
    T frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuv422Sample(cricket::FOURCC_UYVY, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_UYVY,
                          kWidth, kHeight, kWidth, kHeight,
                          cricket::ROTATION_270, &frame2));
  }

  // Test constructing an image from a YUY2 buffer rotated 90 degrees.
  void ConstructYuy2Rotate90() {
    T frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuv422Sample(cricket::FOURCC_YUY2, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_YUY2,
                          kWidth, kHeight, kWidth, kHeight,
                          cricket::ROTATION_90, &frame2));
  }

  // Test constructing an image from a YUY2 buffer rotated 180 degrees.
  void ConstructYuy2Rotate180() {
    T frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuv422Sample(cricket::FOURCC_YUY2, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_YUY2,
                          kWidth, kHeight, kWidth, kHeight,
                          cricket::ROTATION_180, &frame2));
  }

  // Test constructing an image from a YUY2 buffer rotated 270 degrees.
  void ConstructYuy2Rotate270() {
    T frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuv422Sample(cricket::FOURCC_YUY2, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_YUY2,
                          kWidth, kHeight, kWidth, kHeight,
                          cricket::ROTATION_270, &frame2));
  }

  // Test 1 pixel edge case image I420 buffer.
  void ConstructI4201Pixel() {
    T frame;
    uint8 pixel[3] = { 1, 2, 3 };
    for (int i = 0; i < repeat_; ++i) {
      EXPECT_TRUE(frame.Init(cricket::FOURCC_I420, 1, 1, 1, 1,
                             pixel, sizeof(pixel),
                             1, 1, 0, 0, 0));
    }
    const uint8* y = pixel;
    const uint8* u = y + 1;
    const uint8* v = u + 1;
    EXPECT_TRUE(IsEqual(frame, 1, 1, 1, 1, 0, 0,
                        y, 1, u, 1, v, 1, 0));
  }

  // Test 5 pixel edge case image I420 buffer rounds down to 4.
  void ConstructI4205Pixel() {
    T frame;
    uint8 pixels5x5[5 * 5 + ((5 + 1) / 2 * (5 + 1) / 2) *  2];
    memset(pixels5x5, 1, 5 * 5 + ((5 + 1) / 2 * (5 + 1) / 2) *  2);
    for (int i = 0; i < repeat_; ++i) {
      EXPECT_TRUE(frame.Init(cricket::FOURCC_I420, 5, 5, 5, 5,
                             pixels5x5, sizeof(pixels5x5),
                             1, 1, 0, 0, 0));
    }
    EXPECT_EQ(4u, frame.GetWidth());
    EXPECT_EQ(4u, frame.GetHeight());
    EXPECT_EQ(4, frame.GetYPitch());
    EXPECT_EQ(2, frame.GetUPitch());
    EXPECT_EQ(2, frame.GetVPitch());
  }

  // Test 1 pixel edge case image ARGB buffer.
  void ConstructARGB1Pixel() {
    T frame;
    uint8 pixel[4] = { 64, 128, 192, 255 };
    for (int i = 0; i < repeat_; ++i) {
      EXPECT_TRUE(frame.Init(cricket::FOURCC_ARGB, 1, 1, 1, 1,
                             pixel, sizeof(pixel),
                             1, 1, 0, 0, 0));
    }
    // Convert back to ARGB.
    size_t out_size = 4;
    talk_base::scoped_ptr<uint8[]> outbuf(new uint8[out_size + kAlignment]);
    uint8 *out = ALIGNP(outbuf.get(), kAlignment);

    EXPECT_EQ(out_size, frame.ConvertToRgbBuffer(cricket::FOURCC_ARGB,
                                                 out,
                                                 out_size,    // buffer size
                                                 out_size));  // stride
  #ifdef USE_LMI_CONVERT
    // TODO(fbarchard): Expected to fail, but not crash.
    EXPECT_FALSE(IsPlaneEqual("argb", pixel, 4, out, 4, 3, 1, 2));
  #else
    // TODO(fbarchard): Check for overwrite.
    EXPECT_TRUE(IsPlaneEqual("argb", pixel, 4, out, 4, 3, 1, 2));
  #endif
  }

  // Test Black, White and Grey pixels.
  void ConstructARGBBlackWhitePixel() {
    T frame;
    uint8 pixel[10 * 4] = { 0, 0, 0, 255,  // Black.
                            0, 0, 0, 255,
                            64, 64, 64, 255,  // Dark Grey.
                            64, 64, 64, 255,
                            128, 128, 128, 255,  // Grey.
                            128, 128, 128, 255,
                            196, 196, 196, 255,  // Light Grey.
                            196, 196, 196, 255,
                            255, 255, 255, 255,  // White.
                            255, 255, 255, 255 };

    for (int i = 0; i < repeat_; ++i) {
      EXPECT_TRUE(frame.Init(cricket::FOURCC_ARGB, 10, 1, 10, 1,
                             pixel, sizeof(pixel),
                             1, 1, 0, 0, 0));
    }
    // Convert back to ARGB
    size_t out_size = 10 * 4;
    talk_base::scoped_ptr<uint8[]> outbuf(new uint8[out_size + kAlignment]);
    uint8 *out = ALIGNP(outbuf.get(), kAlignment);

    EXPECT_EQ(out_size, frame.ConvertToRgbBuffer(cricket::FOURCC_ARGB,
                                                 out,
                                                 out_size,    // buffer size.
                                                 out_size));  // stride.
    EXPECT_TRUE(IsPlaneEqual("argb", pixel, out_size,
                             out, out_size,
                             out_size, 1, 2));
  }

  // Test constructing an image from an I420 buffer with horizontal cropping.
  void ConstructI420CropHorizontal() {
    T frame1, frame2;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    ASSERT_TRUE(LoadFrame(kImageFilename, cricket::FOURCC_I420, kWidth, kHeight,
                          kWidth * 3 / 4, kHeight, 0, &frame2));
    EXPECT_TRUE(IsEqualWithCrop(frame2, frame1, kWidth / 8, 0, 0));
  }

  // Test constructing an image from a YUY2 buffer with horizontal cropping.
  void ConstructYuy2CropHorizontal() {
    T frame1, frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuv422Sample(cricket::FOURCC_YUY2, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(ConvertYuv422(ms.get(), cricket::FOURCC_YUY2, kWidth, kHeight,
                              &frame1));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_YUY2, kWidth, kHeight,
                          kWidth * 3 / 4, kHeight, 0, &frame2));
    EXPECT_TRUE(IsEqualWithCrop(frame2, frame1, kWidth / 8, 0, 0));
  }

  // Test constructing an image from an ARGB buffer with horizontal cropping.
  void ConstructARGBCropHorizontal() {
    T frame1, frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateRgbSample(cricket::FOURCC_ARGB, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(ConvertRgb(ms.get(), cricket::FOURCC_ARGB, kWidth, kHeight,
                           &frame1));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_ARGB, kWidth, kHeight,
                          kWidth * 3 / 4, kHeight, 0, &frame2));
    EXPECT_TRUE(IsEqualWithCrop(frame2, frame1, kWidth / 8, 0, 2));
  }

  // Test constructing an image from an I420 buffer, cropping top and bottom.
  void ConstructI420CropVertical() {
    T frame1, frame2;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    ASSERT_TRUE(LoadFrame(kImageFilename, cricket::FOURCC_I420, kWidth, kHeight,
                          kWidth, kHeight * 3 / 4, 0, &frame2));
    EXPECT_TRUE(IsEqualWithCrop(frame2, frame1, 0, kHeight / 8, 0));
  }

  // Test constructing an image from I420 synonymous formats.
  void ConstructI420Aliases() {
    T frame1, frame2, frame3;
    ASSERT_TRUE(LoadFrame(kImageFilename, cricket::FOURCC_I420, kWidth, kHeight,
                          &frame1));
    ASSERT_TRUE(LoadFrame(kImageFilename, cricket::FOURCC_IYUV, kWidth, kHeight,
                          &frame2));
    ASSERT_TRUE(LoadFrame(kImageFilename, cricket::FOURCC_YU12, kWidth, kHeight,
                          &frame3));
    EXPECT_TRUE(IsEqual(frame1, frame2, 0));
    EXPECT_TRUE(IsEqual(frame1, frame3, 0));
  }

  // Test constructing an image from an I420 MJPG buffer.
  void ConstructMjpgI420() {
    T frame1, frame2;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    ASSERT_TRUE(LoadFrame(kJpeg420Filename,
                          cricket::FOURCC_MJPG, kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 32));
  }

  // Test constructing an image from an I422 MJPG buffer.
  void ConstructMjpgI422() {
    T frame1, frame2;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    ASSERT_TRUE(LoadFrame(kJpeg422Filename,
                          cricket::FOURCC_MJPG, kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 32));
  }

  // Test constructing an image from an I444 MJPG buffer.
  void ConstructMjpgI444() {
    T frame1, frame2;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    ASSERT_TRUE(LoadFrame(kJpeg444Filename,
                          cricket::FOURCC_MJPG, kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 32));
  }

  // Test constructing an image from an I444 MJPG buffer.
  void ConstructMjpgI411() {
    T frame1, frame2;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    ASSERT_TRUE(LoadFrame(kJpeg411Filename,
                          cricket::FOURCC_MJPG, kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsEqual(frame1, frame2, 32));
  }

  // Test constructing an image from an I400 MJPG buffer.
  // TODO(fbarchard): Stronger compare on chroma.  Compare agaisnt a grey image.
  void ConstructMjpgI400() {
    T frame1, frame2;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    ASSERT_TRUE(LoadFrame(kJpeg400Filename,
                          cricket::FOURCC_MJPG, kWidth, kHeight, &frame2));
    EXPECT_TRUE(IsPlaneEqual("y", frame1.GetYPlane(), frame1.GetYPitch(),
                             frame2.GetYPlane(), frame2.GetYPitch(),
                             kWidth, kHeight, 32));
    EXPECT_TRUE(IsEqual(frame1, frame2, 128));
  }

  // Test constructing an image from an I420 MJPG buffer.
  void ValidateFrame(const char* name, uint32 fourcc, int data_adjust,
                     int size_adjust, bool expected_result) {
    T frame;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(LoadSample(name));
    ASSERT_TRUE(ms.get() != NULL);
    const uint8* sample = reinterpret_cast<const uint8*>(ms.get()->GetBuffer());
    size_t sample_size;
    ms->GetSize(&sample_size);
    // Optional adjust size to test invalid size.
    size_t data_size = sample_size + data_adjust;

    // Allocate a buffer with end page aligned.
    const int kPadToHeapSized = 16 * 1024 * 1024;
    talk_base::scoped_ptr<uint8[]> page_buffer(
        new uint8[((data_size + kPadToHeapSized + 4095) & ~4095)]);
    uint8* data_ptr = page_buffer.get();
    if (!data_ptr) {
      LOG(LS_WARNING) << "Failed to allocate memory for ValidateFrame test.";
      EXPECT_FALSE(expected_result);  // NULL is okay if failure was expected.
      return;
    }
    data_ptr += kPadToHeapSized + (-(static_cast<int>(data_size)) & 4095);
    memcpy(data_ptr, sample, talk_base::_min(data_size, sample_size));
    for (int i = 0; i < repeat_; ++i) {
      EXPECT_EQ(expected_result, frame.Validate(fourcc, kWidth, kHeight,
                                                data_ptr,
                                                sample_size + size_adjust));
    }
  }

  // Test validate for I420 MJPG buffer.
  void ValidateMjpgI420() {
    ValidateFrame(kJpeg420Filename, cricket::FOURCC_MJPG, 0, 0, true);
  }

  // Test validate for I422 MJPG buffer.
  void ValidateMjpgI422() {
    ValidateFrame(kJpeg422Filename, cricket::FOURCC_MJPG, 0, 0, true);
  }

  // Test validate for I444 MJPG buffer.
  void ValidateMjpgI444() {
    ValidateFrame(kJpeg444Filename, cricket::FOURCC_MJPG, 0, 0, true);
  }

  // Test validate for I411 MJPG buffer.
  void ValidateMjpgI411() {
    ValidateFrame(kJpeg411Filename, cricket::FOURCC_MJPG, 0, 0, true);
  }

  // Test validate for I400 MJPG buffer.
  void ValidateMjpgI400() {
    ValidateFrame(kJpeg400Filename, cricket::FOURCC_MJPG, 0, 0, true);
  }

  // Test validate for I420 buffer.
  void ValidateI420() {
    ValidateFrame(kImageFilename, cricket::FOURCC_I420, 0, 0, true);
  }

  // Test validate for I420 buffer where size is too small
  void ValidateI420SmallSize() {
    ValidateFrame(kImageFilename, cricket::FOURCC_I420, 0, -16384, false);
  }

  // Test validate for I420 buffer where size is too large (16 MB)
  // Will produce warning but pass.
  void ValidateI420LargeSize() {
    ValidateFrame(kImageFilename, cricket::FOURCC_I420, 16000000, 16000000,
                  true);
  }

  // Test validate for I420 buffer where size is 1 GB (not reasonable).
  void ValidateI420HugeSize() {
#ifndef WIN32  // TODO(fbarchard): Reenable when fixing bug 9603762.
    ValidateFrame(kImageFilename, cricket::FOURCC_I420, 1000000000u,
                  1000000000u, false);
#endif
  }

  // The following test that Validate crashes if the size is greater than the
  // actual buffer size.
  // TODO(fbarchard): Consider moving a filter into the capturer/plugin.
#if defined(_MSC_VER) && defined(_DEBUG)
  int ExceptionFilter(unsigned int code, struct _EXCEPTION_POINTERS *ep) {
    if (code == EXCEPTION_ACCESS_VIOLATION) {
      LOG(LS_INFO) << "Caught EXCEPTION_ACCESS_VIOLATION as expected.";
      return EXCEPTION_EXECUTE_HANDLER;
    } else {
      LOG(LS_INFO) << "Did not catch EXCEPTION_ACCESS_VIOLATION.  Unexpected.";
      return EXCEPTION_CONTINUE_SEARCH;
    }
  }

  // Test validate fails for truncated MJPG data buffer.  If ValidateFrame
  // crashes the exception handler will return and unittest passes with OK.
  void ValidateMjpgI420InvalidSize() {
    __try {
      ValidateFrame(kJpeg420Filename, cricket::FOURCC_MJPG, -16384, 0, false);
      FAIL() << "Validate was expected to cause EXCEPTION_ACCESS_VIOLATION.";
    } __except(ExceptionFilter(GetExceptionCode(), GetExceptionInformation())) {
      return;  // Successfully crashed in ValidateFrame.
    }
  }

  // Test validate fails for truncated I420 buffer.
  void ValidateI420InvalidSize() {
    __try {
      ValidateFrame(kImageFilename, cricket::FOURCC_I420, -16384, 0, false);
      FAIL() << "Validate was expected to cause EXCEPTION_ACCESS_VIOLATION.";
    } __except(ExceptionFilter(GetExceptionCode(), GetExceptionInformation())) {
      return;  // Successfully crashed in ValidateFrame.
    }
  }
#endif

  // Test constructing an image from a YUY2 buffer (and synonymous formats).
  void ConstructYuy2Aliases() {
    T frame1, frame2, frame3, frame4;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuv422Sample(cricket::FOURCC_YUY2, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(ConvertYuv422(ms.get(), cricket::FOURCC_YUY2, kWidth, kHeight,
                              &frame1));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_YUY2,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_YUVS,
                          kWidth, kHeight, &frame3));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_YUYV,
                          kWidth, kHeight, &frame4));
    EXPECT_TRUE(IsEqual(frame1, frame2, 0));
    EXPECT_TRUE(IsEqual(frame1, frame3, 0));
    EXPECT_TRUE(IsEqual(frame1, frame4, 0));
  }

  // Test constructing an image from a UYVY buffer (and synonymous formats).
  void ConstructUyvyAliases() {
    T frame1, frame2, frame3, frame4;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateYuv422Sample(cricket::FOURCC_UYVY, kWidth, kHeight));
    ASSERT_TRUE(ms.get() != NULL);
    EXPECT_TRUE(ConvertYuv422(ms.get(), cricket::FOURCC_UYVY, kWidth, kHeight,
                              &frame1));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_UYVY,
                          kWidth, kHeight, &frame2));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_2VUY,
                          kWidth, kHeight, &frame3));
    EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_HDYC,
                          kWidth, kHeight, &frame4));
    EXPECT_TRUE(IsEqual(frame1, frame2, 0));
    EXPECT_TRUE(IsEqual(frame1, frame3, 0));
    EXPECT_TRUE(IsEqual(frame1, frame4, 0));
  }

  // Test creating a copy.
  void ConstructCopy() {
    T frame1, frame2;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    for (int i = 0; i < repeat_; ++i) {
      EXPECT_TRUE(frame2.Init(frame1));
    }
    EXPECT_TRUE(IsEqual(frame1, frame2, 0));
  }

  // Test creating a copy and check that it just increments the refcount.
  void ConstructCopyIsRef() {
    T frame1, frame2;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    for (int i = 0; i < repeat_; ++i) {
      EXPECT_TRUE(frame2.Init(frame1));
    }
    EXPECT_TRUE(IsEqual(frame1, frame2, 0));
    EXPECT_EQ(frame1.GetYPlane(), frame2.GetYPlane());
    EXPECT_EQ(frame1.GetUPlane(), frame2.GetUPlane());
    EXPECT_EQ(frame1.GetVPlane(), frame2.GetVPlane());
  }

  // Test creating an empty image and initing it to black.
  void ConstructBlack() {
    T frame;
    for (int i = 0; i < repeat_; ++i) {
      EXPECT_TRUE(frame.InitToBlack(kWidth, kHeight, 1, 1, 0, 0));
    }
    EXPECT_TRUE(IsSize(frame, kWidth, kHeight));
    EXPECT_TRUE(IsBlack(frame));
  }

  // Test constructing an image from a YUY2 buffer with a range of sizes.
  // Only tests that conversion does not crash or corrupt heap.
  void ConstructYuy2AllSizes() {
    T frame1, frame2;
    for (int height = kMinHeightAll; height <= kMaxHeightAll; ++height) {
      for (int width = kMinWidthAll; width <= kMaxWidthAll; ++width) {
        talk_base::scoped_ptr<talk_base::MemoryStream> ms(
            CreateYuv422Sample(cricket::FOURCC_YUY2, width, height));
        ASSERT_TRUE(ms.get() != NULL);
        EXPECT_TRUE(ConvertYuv422(ms.get(), cricket::FOURCC_YUY2, width, height,
                                  &frame1));
        EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_YUY2,
                              width, height, &frame2));
        EXPECT_TRUE(IsEqual(frame1, frame2, 0));
      }
    }
  }

  // Test constructing an image from a ARGB buffer with a range of sizes.
  // Only tests that conversion does not crash or corrupt heap.
  void ConstructARGBAllSizes() {
    T frame1, frame2;
    for (int height = kMinHeightAll; height <= kMaxHeightAll; ++height) {
      for (int width = kMinWidthAll; width <= kMaxWidthAll; ++width) {
        talk_base::scoped_ptr<talk_base::MemoryStream> ms(
            CreateRgbSample(cricket::FOURCC_ARGB, width, height));
        ASSERT_TRUE(ms.get() != NULL);
        EXPECT_TRUE(ConvertRgb(ms.get(), cricket::FOURCC_ARGB, width, height,
                               &frame1));
        EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_ARGB,
                              width, height, &frame2));
        EXPECT_TRUE(IsEqual(frame1, frame2, 64));
      }
    }
    // Test a practical window size for screencasting usecase.
    const int kOddWidth = 1228;
    const int kOddHeight = 260;
    for (int j = 0; j < 2; ++j) {
      for (int i = 0; i < 2; ++i) {
        talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        CreateRgbSample(cricket::FOURCC_ARGB, kOddWidth + i, kOddHeight + j));
        ASSERT_TRUE(ms.get() != NULL);
        EXPECT_TRUE(ConvertRgb(ms.get(), cricket::FOURCC_ARGB,
                               kOddWidth + i, kOddHeight + j,
                               &frame1));
        EXPECT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_ARGB,
                              kOddWidth + i, kOddHeight + j, &frame2));
        EXPECT_TRUE(IsEqual(frame1, frame2, 64));
      }
    }
  }

  // Tests re-initing an existing image.
  void Reset() {
    T frame1, frame2;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        LoadSample(kImageFilename));
    ASSERT_TRUE(ms.get() != NULL);
    size_t data_size;
    ms->GetSize(&data_size);
    EXPECT_TRUE(frame1.InitToBlack(kWidth, kHeight, 1, 1, 0, 0));
    EXPECT_TRUE(frame2.InitToBlack(kWidth, kHeight, 1, 1, 0, 0));
    EXPECT_TRUE(IsBlack(frame1));
    EXPECT_TRUE(IsEqual(frame1, frame2, 0));
    EXPECT_TRUE(frame1.Reset(cricket::FOURCC_I420,
                             kWidth, kHeight, kWidth, kHeight,
                             reinterpret_cast<uint8*>(ms->GetBuffer()),
                             data_size, 1, 1, 0, 0, 0));
    EXPECT_FALSE(IsBlack(frame1));
    EXPECT_FALSE(IsEqual(frame1, frame2, 0));
  }

  //////////////////////
  // Conversion tests //
  //////////////////////

  enum ToFrom { TO, FROM };

  // Helper function for test converting from I420 to packed formats.
  inline void ConvertToBuffer(int bpp, int rowpad, bool invert, ToFrom to_from,
      int error, uint32 fourcc,
      int (*RGBToI420)(const uint8* src_frame, int src_stride_frame,
                       uint8* dst_y, int dst_stride_y,
                       uint8* dst_u, int dst_stride_u,
                       uint8* dst_v, int dst_stride_v,
                       int width, int height)) {
    T frame1, frame2;
    int repeat_to = (to_from == TO) ? repeat_ : 1;
    int repeat_from  = (to_from == FROM) ? repeat_ : 1;

    int astride = kWidth * bpp + rowpad;
    size_t out_size = astride * kHeight;
    talk_base::scoped_ptr<uint8[]> outbuf(new uint8[out_size + kAlignment + 1]);
    memset(outbuf.get(), 0, out_size + kAlignment + 1);
    uint8 *outtop = ALIGNP(outbuf.get(), kAlignment);
    uint8 *out = outtop;
    int stride = astride;
    if (invert) {
      out += (kHeight - 1) * stride;  // Point to last row.
      stride = -stride;
    }
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));

    for (int i = 0; i < repeat_to; ++i) {
      EXPECT_EQ(out_size, frame1.ConvertToRgbBuffer(fourcc,
                                                    out,
                                                    out_size, stride));
    }
    EXPECT_TRUE(frame2.InitToBlack(kWidth, kHeight, 1, 1, 0, 0));
    for (int i = 0; i < repeat_from; ++i) {
      EXPECT_EQ(0, RGBToI420(out, stride,
                             frame2.GetYPlane(), frame2.GetYPitch(),
                             frame2.GetUPlane(), frame2.GetUPitch(),
                             frame2.GetVPlane(), frame2.GetVPitch(),
                             kWidth, kHeight));
    }
    if (rowpad) {
      EXPECT_EQ(0, outtop[kWidth * bpp]);  // Ensure stride skipped end of row.
      EXPECT_NE(0, outtop[astride]);       // Ensure pixel at start of 2nd row.
    } else {
      EXPECT_NE(0, outtop[kWidth * bpp]);  // Expect something to be here.
    }
    EXPECT_EQ(0, outtop[out_size]);      // Ensure no overrun.
    EXPECT_TRUE(IsEqual(frame1, frame2, error));
  }

  static const int kError = 20;
  static const int kErrorHigh = 40;
  static const int kOddStride = 23;

  // Tests ConvertToRGBBuffer formats.
  void ConvertToARGBBuffer() {
    ConvertToBuffer(4, 0, false, TO, kError,
                    cricket::FOURCC_ARGB, libyuv::ARGBToI420);
  }
  void ConvertToBGRABuffer() {
    ConvertToBuffer(4, 0, false, TO, kError,
                    cricket::FOURCC_BGRA, libyuv::BGRAToI420);
  }
  void ConvertToABGRBuffer() {
    ConvertToBuffer(4, 0, false, TO, kError,
                    cricket::FOURCC_ABGR, libyuv::ABGRToI420);
  }
  void ConvertToRGB24Buffer() {
    ConvertToBuffer(3, 0, false, TO, kError,
                    cricket::FOURCC_24BG, libyuv::RGB24ToI420);
  }
  void ConvertToRAWBuffer() {
    ConvertToBuffer(3, 0, false, TO, kError,
                    cricket::FOURCC_RAW, libyuv::RAWToI420);
  }
  void ConvertToRGB565Buffer() {
    ConvertToBuffer(2, 0, false, TO, kError,
                    cricket::FOURCC_RGBP, libyuv::RGB565ToI420);
  }
  void ConvertToARGB1555Buffer() {
    ConvertToBuffer(2, 0, false, TO, kError,
                    cricket::FOURCC_RGBO, libyuv::ARGB1555ToI420);
  }
  void ConvertToARGB4444Buffer() {
    ConvertToBuffer(2, 0, false, TO, kError,
                    cricket::FOURCC_R444, libyuv::ARGB4444ToI420);
  }
  void ConvertToBayerBGGRBuffer() {
    ConvertToBuffer(1, 0, false, TO, kErrorHigh,
                    cricket::FOURCC_BGGR, libyuv::BayerBGGRToI420);
  }
  void ConvertToBayerGBRGBuffer() {
    ConvertToBuffer(1, 0, false, TO, kErrorHigh,
                    cricket::FOURCC_GBRG, libyuv::BayerGBRGToI420);
  }
  void ConvertToBayerGRBGBuffer() {
    ConvertToBuffer(1, 0, false, TO, kErrorHigh,
                    cricket::FOURCC_GRBG, libyuv::BayerGRBGToI420);
  }
  void ConvertToBayerRGGBBuffer() {
    ConvertToBuffer(1, 0, false, TO, kErrorHigh,
                    cricket::FOURCC_RGGB, libyuv::BayerRGGBToI420);
  }
  void ConvertToI400Buffer() {
    ConvertToBuffer(1, 0, false, TO, 128,
                    cricket::FOURCC_I400, libyuv::I400ToI420);
  }
  void ConvertToYUY2Buffer() {
    ConvertToBuffer(2, 0, false, TO, kError,
                    cricket::FOURCC_YUY2, libyuv::YUY2ToI420);
  }
  void ConvertToUYVYBuffer() {
    ConvertToBuffer(2, 0, false, TO, kError,
                    cricket::FOURCC_UYVY, libyuv::UYVYToI420);
  }

  // Tests ConvertToRGBBuffer formats with odd stride.
  void ConvertToARGBBufferStride() {
    ConvertToBuffer(4, kOddStride, false, TO, kError,
                    cricket::FOURCC_ARGB, libyuv::ARGBToI420);
  }
  void ConvertToBGRABufferStride() {
    ConvertToBuffer(4, kOddStride, false, TO, kError,
                    cricket::FOURCC_BGRA, libyuv::BGRAToI420);
  }
  void ConvertToABGRBufferStride() {
    ConvertToBuffer(4, kOddStride, false, TO, kError,
                    cricket::FOURCC_ABGR, libyuv::ABGRToI420);
  }
  void ConvertToRGB24BufferStride() {
    ConvertToBuffer(3, kOddStride, false, TO, kError,
                    cricket::FOURCC_24BG, libyuv::RGB24ToI420);
  }
  void ConvertToRAWBufferStride() {
    ConvertToBuffer(3, kOddStride, false, TO, kError,
                    cricket::FOURCC_RAW, libyuv::RAWToI420);
  }
  void ConvertToRGB565BufferStride() {
    ConvertToBuffer(2, kOddStride, false, TO, kError,
                    cricket::FOURCC_RGBP, libyuv::RGB565ToI420);
  }
  void ConvertToARGB1555BufferStride() {
    ConvertToBuffer(2, kOddStride, false, TO, kError,
                    cricket::FOURCC_RGBO, libyuv::ARGB1555ToI420);
  }
  void ConvertToARGB4444BufferStride() {
    ConvertToBuffer(2, kOddStride, false, TO, kError,
                    cricket::FOURCC_R444, libyuv::ARGB4444ToI420);
  }
  void ConvertToBayerBGGRBufferStride() {
    ConvertToBuffer(1, kOddStride, false, TO, kErrorHigh,
                    cricket::FOURCC_BGGR, libyuv::BayerBGGRToI420);
  }
  void ConvertToBayerGBRGBufferStride() {
    ConvertToBuffer(1, kOddStride, false, TO, kErrorHigh,
                    cricket::FOURCC_GBRG, libyuv::BayerGBRGToI420);
  }
  void ConvertToBayerGRBGBufferStride() {
    ConvertToBuffer(1, kOddStride, false, TO, kErrorHigh,
                    cricket::FOURCC_GRBG, libyuv::BayerGRBGToI420);
  }
  void ConvertToBayerRGGBBufferStride() {
    ConvertToBuffer(1, kOddStride, false, TO, kErrorHigh,
                    cricket::FOURCC_RGGB, libyuv::BayerRGGBToI420);
  }
  void ConvertToI400BufferStride() {
    ConvertToBuffer(1, kOddStride, false, TO, 128,
                    cricket::FOURCC_I400, libyuv::I400ToI420);
  }
  void ConvertToYUY2BufferStride() {
    ConvertToBuffer(2, kOddStride, false, TO, kError,
                    cricket::FOURCC_YUY2, libyuv::YUY2ToI420);
  }
  void ConvertToUYVYBufferStride() {
    ConvertToBuffer(2, kOddStride, false, TO, kError,
                    cricket::FOURCC_UYVY, libyuv::UYVYToI420);
  }

  // Tests ConvertToRGBBuffer formats with negative stride to invert image.
  void ConvertToARGBBufferInverted() {
    ConvertToBuffer(4, 0, true, TO, kError,
                    cricket::FOURCC_ARGB, libyuv::ARGBToI420);
  }
  void ConvertToBGRABufferInverted() {
    ConvertToBuffer(4, 0, true, TO, kError,
                    cricket::FOURCC_BGRA, libyuv::BGRAToI420);
  }
  void ConvertToABGRBufferInverted() {
    ConvertToBuffer(4, 0, true, TO, kError,
                    cricket::FOURCC_ABGR, libyuv::ABGRToI420);
  }
  void ConvertToRGB24BufferInverted() {
    ConvertToBuffer(3, 0, true, TO, kError,
                    cricket::FOURCC_24BG, libyuv::RGB24ToI420);
  }
  void ConvertToRAWBufferInverted() {
    ConvertToBuffer(3, 0, true, TO, kError,
                    cricket::FOURCC_RAW, libyuv::RAWToI420);
  }
  void ConvertToRGB565BufferInverted() {
    ConvertToBuffer(2, 0, true, TO, kError,
                    cricket::FOURCC_RGBP, libyuv::RGB565ToI420);
  }
  void ConvertToARGB1555BufferInverted() {
    ConvertToBuffer(2, 0, true, TO, kError,
                    cricket::FOURCC_RGBO, libyuv::ARGB1555ToI420);
  }
  void ConvertToARGB4444BufferInverted() {
    ConvertToBuffer(2, 0, true, TO, kError,
                    cricket::FOURCC_R444, libyuv::ARGB4444ToI420);
  }
  void ConvertToBayerBGGRBufferInverted() {
    ConvertToBuffer(1, 0, true, TO, kErrorHigh,
                    cricket::FOURCC_BGGR, libyuv::BayerBGGRToI420);
  }
  void ConvertToBayerGBRGBufferInverted() {
    ConvertToBuffer(1, 0, true, TO, kErrorHigh,
                    cricket::FOURCC_GBRG, libyuv::BayerGBRGToI420);
  }
  void ConvertToBayerGRBGBufferInverted() {
    ConvertToBuffer(1, 0, true, TO, kErrorHigh,
                    cricket::FOURCC_GRBG, libyuv::BayerGRBGToI420);
  }
  void ConvertToBayerRGGBBufferInverted() {
    ConvertToBuffer(1, 0, true, TO, kErrorHigh,
                    cricket::FOURCC_RGGB, libyuv::BayerRGGBToI420);
  }
  void ConvertToI400BufferInverted() {
    ConvertToBuffer(1, 0, true, TO, 128,
                    cricket::FOURCC_I400, libyuv::I400ToI420);
  }
  void ConvertToYUY2BufferInverted() {
    ConvertToBuffer(2, 0, true, TO, kError,
                    cricket::FOURCC_YUY2, libyuv::YUY2ToI420);
  }
  void ConvertToUYVYBufferInverted() {
    ConvertToBuffer(2, 0, true, TO, kError,
                    cricket::FOURCC_UYVY, libyuv::UYVYToI420);
  }

  // Tests ConvertFrom formats.
  void ConvertFromARGBBuffer() {
    ConvertToBuffer(4, 0, false, FROM, kError,
                    cricket::FOURCC_ARGB, libyuv::ARGBToI420);
  }
  void ConvertFromBGRABuffer() {
    ConvertToBuffer(4, 0, false, FROM, kError,
                    cricket::FOURCC_BGRA, libyuv::BGRAToI420);
  }
  void ConvertFromABGRBuffer() {
    ConvertToBuffer(4, 0, false, FROM, kError,
                    cricket::FOURCC_ABGR, libyuv::ABGRToI420);
  }
  void ConvertFromRGB24Buffer() {
    ConvertToBuffer(3, 0, false, FROM, kError,
                    cricket::FOURCC_24BG, libyuv::RGB24ToI420);
  }
  void ConvertFromRAWBuffer() {
    ConvertToBuffer(3, 0, false, FROM, kError,
                    cricket::FOURCC_RAW, libyuv::RAWToI420);
  }
  void ConvertFromRGB565Buffer() {
    ConvertToBuffer(2, 0, false, FROM, kError,
                    cricket::FOURCC_RGBP, libyuv::RGB565ToI420);
  }
  void ConvertFromARGB1555Buffer() {
    ConvertToBuffer(2, 0, false, FROM, kError,
                    cricket::FOURCC_RGBO, libyuv::ARGB1555ToI420);
  }
  void ConvertFromARGB4444Buffer() {
    ConvertToBuffer(2, 0, false, FROM, kError,
                    cricket::FOURCC_R444, libyuv::ARGB4444ToI420);
  }
  void ConvertFromBayerBGGRBuffer() {
    ConvertToBuffer(1, 0, false, FROM, kErrorHigh,
                    cricket::FOURCC_BGGR, libyuv::BayerBGGRToI420);
  }
  void ConvertFromBayerGBRGBuffer() {
    ConvertToBuffer(1, 0, false, FROM, kErrorHigh,
                    cricket::FOURCC_GBRG, libyuv::BayerGBRGToI420);
  }
  void ConvertFromBayerGRBGBuffer() {
    ConvertToBuffer(1, 0, false, FROM, kErrorHigh,
                    cricket::FOURCC_GRBG, libyuv::BayerGRBGToI420);
  }
  void ConvertFromBayerRGGBBuffer() {
    ConvertToBuffer(1, 0, false, FROM, kErrorHigh,
                    cricket::FOURCC_RGGB, libyuv::BayerRGGBToI420);
  }
  void ConvertFromI400Buffer() {
    ConvertToBuffer(1, 0, false, FROM, 128,
                    cricket::FOURCC_I400, libyuv::I400ToI420);
  }
  void ConvertFromYUY2Buffer() {
    ConvertToBuffer(2, 0, false, FROM, kError,
                    cricket::FOURCC_YUY2, libyuv::YUY2ToI420);
  }
  void ConvertFromUYVYBuffer() {
    ConvertToBuffer(2, 0, false, FROM, kError,
                    cricket::FOURCC_UYVY, libyuv::UYVYToI420);
  }

  // Tests ConvertFrom formats with odd stride.
  void ConvertFromARGBBufferStride() {
    ConvertToBuffer(4, kOddStride, false, FROM, kError,
                    cricket::FOURCC_ARGB, libyuv::ARGBToI420);
  }
  void ConvertFromBGRABufferStride() {
    ConvertToBuffer(4, kOddStride, false, FROM, kError,
                    cricket::FOURCC_BGRA, libyuv::BGRAToI420);
  }
  void ConvertFromABGRBufferStride() {
    ConvertToBuffer(4, kOddStride, false, FROM, kError,
                    cricket::FOURCC_ABGR, libyuv::ABGRToI420);
  }
  void ConvertFromRGB24BufferStride() {
    ConvertToBuffer(3, kOddStride, false, FROM, kError,
                    cricket::FOURCC_24BG, libyuv::RGB24ToI420);
  }
  void ConvertFromRAWBufferStride() {
    ConvertToBuffer(3, kOddStride, false, FROM, kError,
                    cricket::FOURCC_RAW, libyuv::RAWToI420);
  }
  void ConvertFromRGB565BufferStride() {
    ConvertToBuffer(2, kOddStride, false, FROM, kError,
                    cricket::FOURCC_RGBP, libyuv::RGB565ToI420);
  }
  void ConvertFromARGB1555BufferStride() {
    ConvertToBuffer(2, kOddStride, false, FROM, kError,
                    cricket::FOURCC_RGBO, libyuv::ARGB1555ToI420);
  }
  void ConvertFromARGB4444BufferStride() {
    ConvertToBuffer(2, kOddStride, false, FROM, kError,
                    cricket::FOURCC_R444, libyuv::ARGB4444ToI420);
  }
  void ConvertFromBayerBGGRBufferStride() {
    ConvertToBuffer(1, kOddStride, false, FROM, kErrorHigh,
                    cricket::FOURCC_BGGR, libyuv::BayerBGGRToI420);
  }
  void ConvertFromBayerGBRGBufferStride() {
    ConvertToBuffer(1, kOddStride, false, FROM, kErrorHigh,
                    cricket::FOURCC_GBRG, libyuv::BayerGBRGToI420);
  }
  void ConvertFromBayerGRBGBufferStride() {
    ConvertToBuffer(1, kOddStride, false, FROM, kErrorHigh,
                    cricket::FOURCC_GRBG, libyuv::BayerGRBGToI420);
  }
  void ConvertFromBayerRGGBBufferStride() {
    ConvertToBuffer(1, kOddStride, false, FROM, kErrorHigh,
                    cricket::FOURCC_RGGB, libyuv::BayerRGGBToI420);
  }
  void ConvertFromI400BufferStride() {
    ConvertToBuffer(1, kOddStride, false, FROM, 128,
                    cricket::FOURCC_I400, libyuv::I400ToI420);
  }
  void ConvertFromYUY2BufferStride() {
    ConvertToBuffer(2, kOddStride, false, FROM, kError,
                    cricket::FOURCC_YUY2, libyuv::YUY2ToI420);
  }
  void ConvertFromUYVYBufferStride() {
    ConvertToBuffer(2, kOddStride, false, FROM, kError,
                    cricket::FOURCC_UYVY, libyuv::UYVYToI420);
  }

  // Tests ConvertFrom formats with negative stride to invert image.
  void ConvertFromARGBBufferInverted() {
    ConvertToBuffer(4, 0, true, FROM, kError,
                    cricket::FOURCC_ARGB, libyuv::ARGBToI420);
  }
  void ConvertFromBGRABufferInverted() {
    ConvertToBuffer(4, 0, true, FROM, kError,
                    cricket::FOURCC_BGRA, libyuv::BGRAToI420);
  }
  void ConvertFromABGRBufferInverted() {
    ConvertToBuffer(4, 0, true, FROM, kError,
                    cricket::FOURCC_ABGR, libyuv::ABGRToI420);
  }
  void ConvertFromRGB24BufferInverted() {
    ConvertToBuffer(3, 0, true, FROM, kError,
                    cricket::FOURCC_24BG, libyuv::RGB24ToI420);
  }
  void ConvertFromRAWBufferInverted() {
    ConvertToBuffer(3, 0, true, FROM, kError,
                    cricket::FOURCC_RAW, libyuv::RAWToI420);
  }
  void ConvertFromRGB565BufferInverted() {
    ConvertToBuffer(2, 0, true, FROM, kError,
                    cricket::FOURCC_RGBP, libyuv::RGB565ToI420);
  }
  void ConvertFromARGB1555BufferInverted() {
    ConvertToBuffer(2, 0, true, FROM, kError,
                    cricket::FOURCC_RGBO, libyuv::ARGB1555ToI420);
  }
  void ConvertFromARGB4444BufferInverted() {
    ConvertToBuffer(2, 0, true, FROM, kError,
                    cricket::FOURCC_R444, libyuv::ARGB4444ToI420);
  }
  void ConvertFromBayerBGGRBufferInverted() {
    ConvertToBuffer(1, 0, true, FROM, kErrorHigh,
                    cricket::FOURCC_BGGR, libyuv::BayerBGGRToI420);
  }
  void ConvertFromBayerGBRGBufferInverted() {
    ConvertToBuffer(1, 0, true, FROM, kErrorHigh,
                    cricket::FOURCC_GBRG, libyuv::BayerGBRGToI420);
  }
  void ConvertFromBayerGRBGBufferInverted() {
    ConvertToBuffer(1, 0, true, FROM, kErrorHigh,
                    cricket::FOURCC_GRBG, libyuv::BayerGRBGToI420);
  }
  void ConvertFromBayerRGGBBufferInverted() {
    ConvertToBuffer(1, 0, true, FROM, kErrorHigh,
                    cricket::FOURCC_RGGB, libyuv::BayerRGGBToI420);
  }
  void ConvertFromI400BufferInverted() {
    ConvertToBuffer(1, 0, true, FROM, 128,
                    cricket::FOURCC_I400, libyuv::I400ToI420);
  }
  void ConvertFromYUY2BufferInverted() {
    ConvertToBuffer(2, 0, true, FROM, kError,
                    cricket::FOURCC_YUY2, libyuv::YUY2ToI420);
  }
  void ConvertFromUYVYBufferInverted() {
    ConvertToBuffer(2, 0, true, FROM, kError,
                    cricket::FOURCC_UYVY, libyuv::UYVYToI420);
  }

  // Test converting from I420 to I422.
  void ConvertToI422Buffer() {
    T frame1, frame2;
    size_t out_size = kWidth * kHeight * 2;
    talk_base::scoped_ptr<uint8[]> buf(new uint8[out_size + kAlignment]);
    uint8* y = ALIGNP(buf.get(), kAlignment);
    uint8* u = y + kWidth * kHeight;
    uint8* v = u + (kWidth / 2) * kHeight;
    ASSERT_TRUE(LoadFrameNoRepeat(&frame1));
    for (int i = 0; i < repeat_; ++i) {
      EXPECT_EQ(0, libyuv::I420ToI422(frame1.GetYPlane(), frame1.GetYPitch(),
                                      frame1.GetUPlane(), frame1.GetUPitch(),
                                      frame1.GetVPlane(), frame1.GetVPitch(),
                                      y, kWidth,
                                      u, kWidth / 2,
                                      v, kWidth / 2,
                                      kWidth, kHeight));
    }
    EXPECT_TRUE(frame2.Init(cricket::FOURCC_I422,
                            kWidth, kHeight, kWidth, kHeight,
                            y,
                            out_size,  1, 1, 0, 0, cricket::ROTATION_0));
    EXPECT_TRUE(IsEqual(frame1, frame2, 0));
  }

  #define TEST_TOBYR(NAME, BAYER)                                              \
  void NAME() {                                                                \
    size_t bayer_size = kWidth * kHeight;                                      \
    talk_base::scoped_ptr<uint8[]> bayerbuf(new uint8[                         \
        bayer_size + kAlignment]);                                             \
    uint8 *bayer = ALIGNP(bayerbuf.get(), kAlignment);                         \
    T frame;                                                                   \
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(                         \
        CreateRgbSample(cricket::FOURCC_ARGB, kWidth, kHeight));               \
    ASSERT_TRUE(ms.get() != NULL);                                             \
    for (int i = 0; i < repeat_; ++i) {                                        \
      libyuv::ARGBToBayer##BAYER(reinterpret_cast<uint8*>(ms->GetBuffer()),    \
                                 kWidth * 4,                                   \
                                 bayer, kWidth,                                \
                                 kWidth, kHeight);                             \
    }                                                                          \
    talk_base::scoped_ptr<talk_base::MemoryStream> ms2(                        \
        CreateRgbSample(cricket::FOURCC_ARGB, kWidth, kHeight));               \
    size_t data_size;                                                          \
    bool ret = ms2->GetSize(&data_size);                                       \
    EXPECT_TRUE(ret);                                                          \
    libyuv::Bayer##BAYER##ToARGB(bayer, kWidth,                                \
                                 reinterpret_cast<uint8*>(ms2->GetBuffer()),   \
                                 kWidth * 4,                                   \
                                 kWidth, kHeight);                             \
    EXPECT_TRUE(IsPlaneEqual("argb",                                           \
        reinterpret_cast<uint8*>(ms->GetBuffer()), kWidth * 4,                 \
        reinterpret_cast<uint8*>(ms2->GetBuffer()), kWidth * 4,                \
        kWidth * 4, kHeight, 240));                                            \
  }                                                                            \
  void NAME##Unaligned() {                                                     \
    size_t bayer_size = kWidth * kHeight;                                      \
    talk_base::scoped_ptr<uint8[]> bayerbuf(new uint8[                         \
        bayer_size + 1 + kAlignment]);                                         \
    uint8 *bayer = ALIGNP(bayerbuf.get(), kAlignment) + 1;                     \
    T frame;                                                                   \
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(                         \
        CreateRgbSample(cricket::FOURCC_ARGB, kWidth, kHeight));               \
    ASSERT_TRUE(ms.get() != NULL);                                             \
    for (int i = 0; i < repeat_; ++i) {                                        \
      libyuv::ARGBToBayer##BAYER(reinterpret_cast<uint8*>(ms->GetBuffer()),    \
                                 kWidth * 4,                                   \
                                 bayer, kWidth,                                \
                                 kWidth, kHeight);                             \
    }                                                                          \
    talk_base::scoped_ptr<talk_base::MemoryStream> ms2(                        \
        CreateRgbSample(cricket::FOURCC_ARGB, kWidth, kHeight));               \
    size_t data_size;                                                          \
    bool ret = ms2->GetSize(&data_size);                                       \
    EXPECT_TRUE(ret);                                                          \
    libyuv::Bayer##BAYER##ToARGB(bayer, kWidth,                                \
                           reinterpret_cast<uint8*>(ms2->GetBuffer()),         \
                           kWidth * 4,                                         \
                           kWidth, kHeight);                                   \
    EXPECT_TRUE(IsPlaneEqual("argb",                                           \
        reinterpret_cast<uint8*>(ms->GetBuffer()), kWidth * 4,                 \
        reinterpret_cast<uint8*>(ms2->GetBuffer()), kWidth * 4,                \
        kWidth * 4, kHeight, 240));                                            \
  }

  // Tests ARGB to Bayer formats.
  TEST_TOBYR(ConvertARGBToBayerGRBG, GRBG)
  TEST_TOBYR(ConvertARGBToBayerGBRG, GBRG)
  TEST_TOBYR(ConvertARGBToBayerBGGR, BGGR)
  TEST_TOBYR(ConvertARGBToBayerRGGB, RGGB)

  #define TEST_BYRTORGB(NAME, BAYER)                                           \
  void NAME() {                                                                \
    size_t bayer_size = kWidth * kHeight;                                      \
    talk_base::scoped_ptr<uint8[]> bayerbuf(new uint8[                         \
        bayer_size + kAlignment]);                                             \
    uint8 *bayer1 = ALIGNP(bayerbuf.get(), kAlignment);                        \
    for (int i = 0; i < kWidth * kHeight; ++i) {                               \
      bayer1[i] = static_cast<uint8>(i * 33u + 183u);                          \
    }                                                                          \
    T frame;                                                                   \
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(                         \
        CreateRgbSample(cricket::FOURCC_ARGB, kWidth, kHeight));               \
    ASSERT_TRUE(ms.get() != NULL);                                             \
    for (int i = 0; i < repeat_; ++i) {                                        \
      libyuv::Bayer##BAYER##ToARGB(bayer1, kWidth,                             \
                             reinterpret_cast<uint8*>(ms->GetBuffer()),        \
                             kWidth * 4,                                       \
                             kWidth, kHeight);                                 \
    }                                                                          \
    talk_base::scoped_ptr<uint8[]> bayer2buf(new uint8[                        \
        bayer_size + kAlignment]);                                             \
    uint8 *bayer2 = ALIGNP(bayer2buf.get(), kAlignment);                       \
    libyuv::ARGBToBayer##BAYER(reinterpret_cast<uint8*>(ms->GetBuffer()),      \
                           kWidth * 4,                                         \
                           bayer2, kWidth,                                     \
                           kWidth, kHeight);                                   \
    EXPECT_TRUE(IsPlaneEqual("bayer",                                          \
        bayer1, kWidth,                                                        \
        bayer2, kWidth,                                                        \
        kWidth, kHeight, 0));                                                  \
  }

  // Tests Bayer formats to ARGB.
  TEST_BYRTORGB(ConvertBayerGRBGToARGB, GRBG)
  TEST_BYRTORGB(ConvertBayerGBRGToARGB, GBRG)
  TEST_BYRTORGB(ConvertBayerBGGRToARGB, BGGR)
  TEST_BYRTORGB(ConvertBayerRGGBToARGB, RGGB)

  ///////////////////
  // General tests //
  ///////////////////

  void Copy() {
    talk_base::scoped_ptr<T> source(new T);
    talk_base::scoped_ptr<cricket::VideoFrame> target;
    ASSERT_TRUE(LoadFrameNoRepeat(source.get()));
    target.reset(source->Copy());
    EXPECT_TRUE(IsEqual(*source, *target, 0));
    source.reset();
    EXPECT_TRUE(target->GetYPlane() != NULL);
  }

  void CopyIsRef() {
    talk_base::scoped_ptr<T> source(new T);
    talk_base::scoped_ptr<cricket::VideoFrame> target;
    ASSERT_TRUE(LoadFrameNoRepeat(source.get()));
    target.reset(source->Copy());
    EXPECT_TRUE(IsEqual(*source, *target, 0));
    EXPECT_EQ(source->GetYPlane(), target->GetYPlane());
    EXPECT_EQ(source->GetUPlane(), target->GetUPlane());
    EXPECT_EQ(source->GetVPlane(), target->GetVPlane());
  }

  void MakeExclusive() {
    talk_base::scoped_ptr<T> source(new T);
    talk_base::scoped_ptr<cricket::VideoFrame> target;
    ASSERT_TRUE(LoadFrameNoRepeat(source.get()));
    target.reset(source->Copy());
    EXPECT_TRUE(target->MakeExclusive());
    EXPECT_TRUE(IsEqual(*source, *target, 0));
    EXPECT_NE(target->GetYPlane(), source->GetYPlane());
    EXPECT_NE(target->GetUPlane(), source->GetUPlane());
    EXPECT_NE(target->GetVPlane(), source->GetVPlane());
  }

  void CopyToBuffer() {
    T frame;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        LoadSample(kImageFilename));
    ASSERT_TRUE(ms.get() != NULL);
    ASSERT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_I420, kWidth, kHeight,
                          &frame));
    size_t out_size = kWidth * kHeight * 3 / 2;
    talk_base::scoped_ptr<uint8[]> out(new uint8[out_size]);
    for (int i = 0; i < repeat_; ++i) {
      EXPECT_EQ(out_size, frame.CopyToBuffer(out.get(), out_size));
    }
    EXPECT_EQ(0, memcmp(out.get(), ms->GetBuffer(), out_size));
  }

  void CopyToFrame() {
    T source;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        LoadSample(kImageFilename));
    ASSERT_TRUE(ms.get() != NULL);
    ASSERT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_I420, kWidth, kHeight,
                          &source));

    // Create the target frame by loading from a file.
    T target;
    ASSERT_TRUE(LoadFrameNoRepeat(&target));
    EXPECT_FALSE(IsBlack(target));

    // Stretch and check if the stretched target is black.
    source.CopyToFrame(&target);

    EXPECT_TRUE(IsEqual(source, target, 0));
  }

  void Write() {
    T frame;
    talk_base::scoped_ptr<talk_base::MemoryStream> ms(
        LoadSample(kImageFilename));
    ASSERT_TRUE(ms.get() != NULL);
    talk_base::MemoryStream ms2;
    size_t size;
    ASSERT_TRUE(ms->GetSize(&size));
    ASSERT_TRUE(ms2.ReserveSize(size));
    ASSERT_TRUE(LoadFrame(ms.get(), cricket::FOURCC_I420, kWidth, kHeight,
                          &frame));
    for (int i = 0; i < repeat_; ++i) {
      ms2.SetPosition(0u);  // Useful when repeat_ > 1.
      int error;
      EXPECT_EQ(talk_base::SR_SUCCESS, frame.Write(&ms2, &error));
    }
    size_t out_size = cricket::VideoFrame::SizeOf(kWidth, kHeight);
    EXPECT_EQ(0, memcmp(ms2.GetBuffer(), ms->GetBuffer(), out_size));
  }

  void CopyToBuffer1Pixel() {
    size_t out_size = 3;
    talk_base::scoped_ptr<uint8[]> out(new uint8[out_size + 1]);
    memset(out.get(), 0xfb, out_size + 1);  // Fill buffer
    uint8 pixel[3] = { 1, 2, 3 };
    T frame;
    EXPECT_TRUE(frame.Init(cricket::FOURCC_I420, 1, 1, 1, 1,
                           pixel, sizeof(pixel),
                           1, 1, 0, 0, 0));
    for (int i = 0; i < repeat_; ++i) {
      EXPECT_EQ(out_size, frame.CopyToBuffer(out.get(), out_size));
    }
    EXPECT_EQ(1, out.get()[0]);  // Check Y.  Should be 1.
    EXPECT_EQ(2, out.get()[1]);  // Check U.  Should be 2.
    EXPECT_EQ(3, out.get()[2]);  // Check V.  Should be 3.
    EXPECT_EQ(0xfb, out.get()[3]);  // Check sentinel is still intact.
  }

  void StretchToFrame() {
    // Create the source frame as a black frame.
    T source;
    EXPECT_TRUE(source.InitToBlack(kWidth * 2, kHeight * 2, 1, 1, 0, 0));
    EXPECT_TRUE(IsSize(source, kWidth * 2, kHeight * 2));

    // Create the target frame by loading from a file.
    T target1;
    ASSERT_TRUE(LoadFrameNoRepeat(&target1));
    EXPECT_FALSE(IsBlack(target1));

    // Stretch and check if the stretched target is black.
    source.StretchToFrame(&target1, true, false);
    EXPECT_TRUE(IsBlack(target1));

    // Crop and stretch and check if the stretched target is black.
    T target2;
    ASSERT_TRUE(LoadFrameNoRepeat(&target2));
    source.StretchToFrame(&target2, true, true);
    EXPECT_TRUE(IsBlack(target2));
    EXPECT_EQ(source.GetElapsedTime(), target2.GetElapsedTime());
    EXPECT_EQ(source.GetTimeStamp(), target2.GetTimeStamp());
  }

  int repeat_;
};

#endif  // TALK_MEDIA_BASE_VIDEOFRAME_UNITTEST_H_
