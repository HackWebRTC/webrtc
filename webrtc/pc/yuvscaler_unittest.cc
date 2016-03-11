/*
 *  Copyright 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <sstream>

#include "libyuv/cpu_id.h"
#include "libyuv/scale.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/flags.h"
#include "webrtc/base/gunit.h"
#include "webrtc/media/base/testutils.h"

#if defined(_MSC_VER)
#define ALIGN16(var) __declspec(align(16)) var
#else
#define ALIGN16(var) var __attribute__((aligned(16)))
#endif

using cricket::LoadPlanarYuvTestImage;
using cricket::DumpPlanarYuvTestImage;

DEFINE_bool(yuvscaler_dump, false,
    "whether to write out scaled images for inspection");
DEFINE_int(yuvscaler_repeat, 1,
    "how many times to perform each scaling operation (for perf testing)");

static const int kAlignment = 16;

// TEST_UNCACHED flushes cache to test real memory performance.
// TEST_RSTSC uses cpu cycles for more accurate benchmark of the scale function.
#ifndef __arm__
// #define TEST_UNCACHED 1
// #define TEST_RSTSC 1
#endif

#if defined(TEST_UNCACHED) || defined(TEST_RSTSC)
#ifdef _MSC_VER
#include <emmintrin.h>  // NOLINT
#endif

#if defined(__GNUC__) && defined(__i386__)
static inline uint64_t __rdtsc(void) {
  uint32_t a, d;
  __asm__ volatile("rdtsc" : "=a" (a), "=d" (d));
  return (reinterpret_cast<uint64_t>(d) << 32) + a;
}

static inline void _mm_clflush(volatile void *__p) {
  asm volatile("clflush %0" : "+m" (*(volatile char *)__p));
}
#endif

static void FlushCache(uint8_t* dst, int count) {
  while (count >= 32) {
    _mm_clflush(dst);
    dst += 32;
    count -= 32;
  }
}
#endif

class YuvScalerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    dump_ = *rtc::FlagList::Lookup("yuvscaler_dump")->bool_variable();
    repeat_ = *rtc::FlagList::Lookup("yuvscaler_repeat")->int_variable();
  }

  // Scale an image and compare against a Lanczos-filtered test image.
  // Lanczos is considered to be the "ideal" image resampling method, so we try
  // to get as close to that as possible, while being as fast as possible.
  bool TestScale(int iw, int ih, int ow, int oh, int offset, bool usefile,
                 bool optimize, int cpuflags, bool interpolate,
                 int memoffset, double* error) {
    *error = 0.;
    size_t isize = I420_SIZE(iw, ih);
    size_t osize = I420_SIZE(ow, oh);
    std::unique_ptr<uint8_t[]> ibuffer(
        new uint8_t[isize + kAlignment + memoffset]());
    std::unique_ptr<uint8_t[]> obuffer(
        new uint8_t[osize + kAlignment + memoffset]());
    std::unique_ptr<uint8_t[]> xbuffer(
        new uint8_t[osize + kAlignment + memoffset]());

    uint8_t* ibuf = ALIGNP(ibuffer.get(), kAlignment) + memoffset;
    uint8_t* obuf = ALIGNP(obuffer.get(), kAlignment) + memoffset;
    uint8_t* xbuf = ALIGNP(xbuffer.get(), kAlignment) + memoffset;

    if (usefile) {
      if (!LoadPlanarYuvTestImage("faces", iw, ih, ibuf) ||
          !LoadPlanarYuvTestImage("faces", ow, oh, xbuf)) {
        LOG(LS_ERROR) << "Failed to load image";
        return false;
      }
    } else {
      // These are used to test huge images.
      memset(ibuf, 213, isize);  // Input is constant color.
      memset(obuf, 100, osize);  // Output set to something wrong for now.
      memset(xbuf, 213, osize);  // Expected result.
    }

#ifdef TEST_UNCACHED
    FlushCache(ibuf, isize);
    FlushCache(obuf, osize);
    FlushCache(xbuf, osize);
#endif

    // Scale down.
    // If cpu true, disable cpu optimizations.  Else allow auto detect
    // TODO(fbarchard): set flags for libyuv
    libyuv::MaskCpuFlags(cpuflags);
#ifdef TEST_RSTSC
    uint64_t t = 0;
#endif
    for (int i = 0; i < repeat_; ++i) {
#ifdef TEST_UNCACHED
      FlushCache(ibuf, isize);
      FlushCache(obuf, osize);
#endif
#ifdef TEST_RSTSC
      uint64_t t1 = __rdtsc();
#endif
      EXPECT_EQ(0, libyuv::ScaleOffset(ibuf, iw, ih, obuf, ow, oh,
                                       offset, interpolate));
#ifdef TEST_RSTSC
      uint64_t t2 = __rdtsc();
      t += t2 - t1;
#endif
    }

#ifdef TEST_RSTSC
    LOG(LS_INFO) << "Time: " << std::setw(9) << t;
#endif

    if (dump_) {
      const testing::TestInfo* const test_info =
          testing::UnitTest::GetInstance()->current_test_info();
      std::string test_name(test_info->name());
      DumpPlanarYuvTestImage(test_name, obuf, ow, oh);
    }

    double sse = cricket::ComputeSumSquareError(obuf, xbuf, osize);
    *error = sse / osize;  // Mean Squared Error.
    double PSNR = cricket::ComputePSNR(sse, osize);
    LOG(LS_INFO) << "Image MSE: " <<
      std::setw(6) << std::setprecision(4) << *error <<
      " Image PSNR: " << PSNR;
    return true;
  }

  // Returns the index of the first differing byte. Easier to debug than memcmp.
  static int FindDiff(const uint8_t* buf1, const uint8_t* buf2, int len) {
    int i = 0;
    while (i < len && buf1[i] == buf2[i]) {
      i++;
    }
    return (i < len) ? i : -1;
  }

 protected:
  bool dump_;
  int repeat_;
};

// Tests straight copy of data.
TEST_F(YuvScalerTest, TestCopy) {
  const int iw = 640, ih = 360;
  const int ow = 640, oh = 360;
  ALIGN16(uint8_t ibuf[I420_SIZE(iw, ih)]);
  ALIGN16(uint8_t obuf[I420_SIZE(ow, oh)]);

  // Load the frame, scale it, check it.
  ASSERT_TRUE(LoadPlanarYuvTestImage("faces", iw, ih, ibuf));
  for (int i = 0; i < repeat_; ++i) {
    libyuv::ScaleOffset(ibuf, iw, ih, obuf, ow, oh, 0, false);
  }
  if (dump_) DumpPlanarYuvTestImage("TestCopy", obuf, ow, oh);
  EXPECT_EQ(-1, FindDiff(obuf, ibuf, sizeof(ibuf)));
}

// Tests copy from 4:3 to 16:9.
TEST_F(YuvScalerTest, TestOffset16_10Copy) {
  const int iw = 640, ih = 360;
  const int ow = 640, oh = 480;
  const int offset = (480 - 360) / 2;
  std::unique_ptr<uint8_t[]> ibuffer(
      new uint8_t[I420_SIZE(iw, ih) + kAlignment]);
  std::unique_ptr<uint8_t[]> obuffer(
      new uint8_t[I420_SIZE(ow, oh) + kAlignment]);

  uint8_t* ibuf = ALIGNP(ibuffer.get(), kAlignment);
  uint8_t* obuf = ALIGNP(obuffer.get(), kAlignment);

  // Load the frame, scale it, check it.
  ASSERT_TRUE(LoadPlanarYuvTestImage("faces", iw, ih, ibuf));

  // Clear to black, which is Y = 0 and U and V = 128
  memset(obuf, 0, ow * oh);
  memset(obuf + ow * oh, 128, ow * oh / 2);
  for (int i = 0; i < repeat_; ++i) {
    libyuv::ScaleOffset(ibuf, iw, ih, obuf, ow, oh, offset, false);
  }
  if (dump_) DumpPlanarYuvTestImage("TestOffsetCopy16_9", obuf, ow, oh);
  EXPECT_EQ(-1, FindDiff(obuf + ow * offset,
                         ibuf,
                         iw * ih));
  EXPECT_EQ(-1, FindDiff(obuf + ow * oh + ow * offset / 4,
                         ibuf + iw * ih,
                         iw * ih / 4));
  EXPECT_EQ(-1, FindDiff(obuf + ow * oh * 5 / 4 + ow * offset / 4,
                         ibuf + iw * ih * 5 / 4,
                         iw * ih / 4));
}

// The following are 'cpu' flag values:
// Allow all SIMD optimizations
#define ALLFLAGS -1
// Disable SSSE3 but allow other forms of SIMD (SSE2)
#define NOSSSE3 ~libyuv::kCpuHasSSSE3
// Disable SSE2 and SSSE3
#define NOSSE ~libyuv::kCpuHasSSE2 & ~libyuv::kCpuHasSSSE3

// TEST_M scale factor with variations of opt, align, int
#define TEST_M(name, iwidth, iheight, owidth, oheight, mse) \
TEST_F(YuvScalerTest, name##Ref) { \
  double error; \
  EXPECT_TRUE(TestScale(iwidth, iheight, owidth, oheight, \
                        0, true, false, ALLFLAGS, false, 0, &error)); \
  EXPECT_LE(error, mse); \
} \
TEST_F(YuvScalerTest, name##OptAligned) { \
  double error; \
  EXPECT_TRUE(TestScale(iwidth, iheight, owidth, oheight, \
                        0, true, true, ALLFLAGS, false, 0, &error)); \
  EXPECT_LE(error, mse); \
} \
TEST_F(YuvScalerTest, name##OptUnaligned) { \
  double error; \
  EXPECT_TRUE(TestScale(iwidth, iheight, owidth, oheight, \
                        0, true, true, ALLFLAGS, false, 1, &error)); \
  EXPECT_LE(error, mse); \
} \
TEST_F(YuvScalerTest, name##OptSSE2) { \
  double error; \
  EXPECT_TRUE(TestScale(iwidth, iheight, owidth, oheight, \
                        0, true, true, NOSSSE3, false, 0, &error)); \
  EXPECT_LE(error, mse); \
} \
TEST_F(YuvScalerTest, name##OptC) { \
  double error; \
  EXPECT_TRUE(TestScale(iwidth, iheight, owidth, oheight, \
                        0, true, true, NOSSE, false, 0, &error)); \
  EXPECT_LE(error, mse); \
} \
TEST_F(YuvScalerTest, name##IntRef) { \
  double error; \
  EXPECT_TRUE(TestScale(iwidth, iheight, owidth, oheight, \
                        0, true, false, ALLFLAGS, true, 0, &error)); \
  EXPECT_LE(error, mse); \
} \
TEST_F(YuvScalerTest, name##IntOptAligned) { \
  double error; \
  EXPECT_TRUE(TestScale(iwidth, iheight, owidth, oheight, \
                        0, true, true, ALLFLAGS, true, 0, &error)); \
  EXPECT_LE(error, mse); \
} \
TEST_F(YuvScalerTest, name##IntOptUnaligned) { \
  double error; \
  EXPECT_TRUE(TestScale(iwidth, iheight, owidth, oheight, \
                        0, true, true, ALLFLAGS, true, 1, &error)); \
  EXPECT_LE(error, mse); \
} \
TEST_F(YuvScalerTest, name##IntOptSSE2) { \
  double error; \
  EXPECT_TRUE(TestScale(iwidth, iheight, owidth, oheight, \
                        0, true, true, NOSSSE3, true, 0, &error)); \
  EXPECT_LE(error, mse); \
} \
TEST_F(YuvScalerTest, name##IntOptC) { \
  double error; \
  EXPECT_TRUE(TestScale(iwidth, iheight, owidth, oheight, \
                        0, true, true, NOSSE, true, 0, &error)); \
  EXPECT_LE(error, mse); \
}

#define TEST_H(name, iwidth, iheight, owidth, oheight, opt, cpu, intr, mse) \
TEST_F(YuvScalerTest, name) { \
  double error; \
  EXPECT_TRUE(TestScale(iwidth, iheight, owidth, oheight, \
                        0, false, opt, cpu, intr, 0, &error)); \
  EXPECT_LE(error, mse); \
}

// Test 4x3 aspect ratio scaling

// Tests 1/1x scale down.
TEST_M(TestScale4by3Down11, 640, 480, 640, 480, 0)

// Tests 3/4x scale down.
TEST_M(TestScale4by3Down34, 640, 480, 480, 360, 60)

// Tests 1/2x scale down.
TEST_M(TestScale4by3Down12, 640, 480, 320, 240, 60)

// Tests 3/8x scale down.
TEST_M(TestScale4by3Down38, 640, 480, 240, 180, 60)

// Tests 1/4x scale down..
TEST_M(TestScale4by3Down14, 640, 480, 160, 120, 60)

// Tests 3/16x scale down.
TEST_M(TestScale4by3Down316, 640, 480, 120, 90, 120)

// Tests 1/8x scale down.
TEST_M(TestScale4by3Down18, 640, 480, 80, 60, 150)

// Tests 2/3x scale down.
TEST_M(TestScale4by3Down23, 480, 360, 320, 240, 60)

// Tests 4/3x scale up.
TEST_M(TestScale4by3Up43, 480, 360, 640, 480, 60)

// Tests 2/1x scale up.
TEST_M(TestScale4by3Up21, 320, 240, 640, 480, 60)

// Tests 4/1x scale up.
TEST_M(TestScale4by3Up41, 160, 120, 640, 480, 80)

// Test 16x10 aspect ratio scaling

// Tests 1/1x scale down.
TEST_M(TestScale16by10Down11, 640, 400, 640, 400, 0)

// Tests 3/4x scale down.
TEST_M(TestScale16by10Down34, 640, 400, 480, 300, 60)

// Tests 1/2x scale down.
TEST_M(TestScale16by10Down12, 640, 400, 320, 200, 60)

// Tests 3/8x scale down.
TEST_M(TestScale16by10Down38, 640, 400, 240, 150, 60)

// Tests 1/4x scale down..
TEST_M(TestScale16by10Down14, 640, 400, 160, 100, 60)

// Tests 3/16x scale down.
TEST_M(TestScale16by10Down316, 640, 400, 120, 75, 120)

// Tests 1/8x scale down.
TEST_M(TestScale16by10Down18, 640, 400, 80, 50, 150)

// Tests 2/3x scale down.
TEST_M(TestScale16by10Down23, 480, 300, 320, 200, 60)

// Tests 4/3x scale up.
TEST_M(TestScale16by10Up43, 480, 300, 640, 400, 60)

// Tests 2/1x scale up.
TEST_M(TestScale16by10Up21, 320, 200, 640, 400, 60)

// Tests 4/1x scale up.
TEST_M(TestScale16by10Up41, 160, 100, 640, 400, 80)

// Test 16x9 aspect ratio scaling

// Tests 1/1x scale down.
TEST_M(TestScaleDown11, 640, 360, 640, 360, 0)

// Tests 3/4x scale down.
TEST_M(TestScaleDown34, 640, 360, 480, 270, 60)

// Tests 1/2x scale down.
TEST_M(TestScaleDown12, 640, 360, 320, 180, 60)

// Tests 3/8x scale down.
TEST_M(TestScaleDown38, 640, 360, 240, 135, 60)

// Tests 1/4x scale down..
TEST_M(TestScaleDown14, 640, 360, 160, 90, 60)

// Tests 3/16x scale down.
TEST_M(TestScaleDown316, 640, 360, 120, 68, 120)

// Tests 1/8x scale down.
TEST_M(TestScaleDown18, 640, 360, 80, 45, 150)

// Tests 2/3x scale down.
TEST_M(TestScaleDown23, 480, 270, 320, 180, 60)

// Tests 4/3x scale up.
TEST_M(TestScaleUp43, 480, 270, 640, 360, 60)

// Tests 2/1x scale up.
TEST_M(TestScaleUp21, 320, 180, 640, 360, 60)

// Tests 4/1x scale up.
TEST_M(TestScaleUp41, 160, 90, 640, 360, 80)

// Test HD 4x3 aspect ratio scaling

// Tests 1/1x scale down.
TEST_M(TestScaleHD4x3Down11, 1280, 960, 1280, 960, 0)

// Tests 3/4x scale down.
TEST_M(TestScaleHD4x3Down34, 1280, 960, 960, 720, 60)

// Tests 1/2x scale down.
TEST_M(TestScaleHD4x3Down12, 1280, 960, 640, 480, 60)

// Tests 3/8x scale down.
TEST_M(TestScaleHD4x3Down38, 1280, 960, 480, 360, 60)

// Tests 1/4x scale down..
TEST_M(TestScaleHD4x3Down14, 1280, 960, 320, 240, 60)

// Tests 3/16x scale down.
TEST_M(TestScaleHD4x3Down316, 1280, 960, 240, 180, 120)

// Tests 1/8x scale down.
TEST_M(TestScaleHD4x3Down18, 1280, 960, 160, 120, 150)

// Tests 2/3x scale down.
TEST_M(TestScaleHD4x3Down23, 960, 720, 640, 480, 60)

// Tests 4/3x scale up.
TEST_M(TestScaleHD4x3Up43, 960, 720, 1280, 960, 60)

// Tests 2/1x scale up.
TEST_M(TestScaleHD4x3Up21, 640, 480, 1280, 960, 60)

// Tests 4/1x scale up.
TEST_M(TestScaleHD4x3Up41, 320, 240, 1280, 960, 80)

// Test HD 16x10 aspect ratio scaling

// Tests 1/1x scale down.
TEST_M(TestScaleHD16x10Down11, 1280, 800, 1280, 800, 0)

// Tests 3/4x scale down.
TEST_M(TestScaleHD16x10Down34, 1280, 800, 960, 600, 60)

// Tests 1/2x scale down.
TEST_M(TestScaleHD16x10Down12, 1280, 800, 640, 400, 60)

// Tests 3/8x scale down.
TEST_M(TestScaleHD16x10Down38, 1280, 800, 480, 300, 60)

// Tests 1/4x scale down..
TEST_M(TestScaleHD16x10Down14, 1280, 800, 320, 200, 60)

// Tests 3/16x scale down.
TEST_M(TestScaleHD16x10Down316, 1280, 800, 240, 150, 120)

// Tests 1/8x scale down.
TEST_M(TestScaleHD16x10Down18, 1280, 800, 160, 100, 150)

// Tests 2/3x scale down.
TEST_M(TestScaleHD16x10Down23, 960, 600, 640, 400, 60)

// Tests 4/3x scale up.
TEST_M(TestScaleHD16x10Up43, 960, 600, 1280, 800, 60)

// Tests 2/1x scale up.
TEST_M(TestScaleHD16x10Up21, 640, 400, 1280, 800, 60)

// Tests 4/1x scale up.
TEST_M(TestScaleHD16x10Up41, 320, 200, 1280, 800, 80)

// Test HD 16x9 aspect ratio scaling

// Tests 1/1x scale down.
TEST_M(TestScaleHDDown11, 1280, 720, 1280, 720, 0)

// Tests 3/4x scale down.
TEST_M(TestScaleHDDown34, 1280, 720, 960, 540, 60)

// Tests 1/2x scale down.
TEST_M(TestScaleHDDown12, 1280, 720, 640, 360, 60)

// Tests 3/8x scale down.
TEST_M(TestScaleHDDown38, 1280, 720, 480, 270, 60)

// Tests 1/4x scale down..
TEST_M(TestScaleHDDown14, 1280, 720, 320, 180, 60)

// Tests 3/16x scale down.
TEST_M(TestScaleHDDown316, 1280, 720, 240, 135, 120)

// Tests 1/8x scale down.
TEST_M(TestScaleHDDown18, 1280, 720, 160, 90, 150)

// Tests 2/3x scale down.
TEST_M(TestScaleHDDown23, 960, 540, 640, 360, 60)

// Tests 4/3x scale up.
TEST_M(TestScaleHDUp43, 960, 540, 1280, 720, 60)

// Tests 2/1x scale up.
TEST_M(TestScaleHDUp21, 640, 360, 1280, 720, 60)

// Tests 4/1x scale up.
TEST_M(TestScaleHDUp41, 320, 180, 1280, 720, 80)

// Tests 1366x768 resolution for comparison to chromium scaler_bench
TEST_M(TestScaleHDUp1366, 1280, 720, 1366, 768, 10)

// Tests odd source/dest sizes.  3 less to make chroma odd as well.
TEST_M(TestScaleHDUp1363, 1277, 717, 1363, 765, 10)

// Tests 1/2x scale down, using optimized algorithm.
TEST_M(TestScaleOddDown12, 180, 100, 90, 50, 50)

// Tests bilinear scale down
TEST_M(TestScaleOddDownBilin, 160, 100, 90, 50, 120)

// Test huge buffer scales that are expected to use a different code path
// that avoids stack overflow but still work using point sampling.
// Max output size is 640 wide.

// Tests interpolated 1/8x scale down, using optimized algorithm.
TEST_H(TestScaleDown18HDOptInt, 6144, 48, 768, 6, true, ALLFLAGS, true, 1)

// Tests interpolated 1/8x scale down, using c_only optimized algorithm.
TEST_H(TestScaleDown18HDCOnlyOptInt, 6144, 48, 768, 6, true, NOSSE, true, 1)

// Tests interpolated 3/8x scale down, using optimized algorithm.
TEST_H(TestScaleDown38HDOptInt, 2048, 16, 768, 6, true, ALLFLAGS, true, 1)

// Tests interpolated 3/8x scale down, using no SSSE3 optimized algorithm.
TEST_H(TestScaleDown38HDNoSSSE3OptInt, 2048, 16, 768, 6, true, NOSSSE3, true, 1)

// Tests interpolated 3/8x scale down, using c_only optimized algorithm.
TEST_H(TestScaleDown38HDCOnlyOptInt, 2048, 16, 768, 6, true, NOSSE, true, 1)

// Tests interpolated 3/16x scale down, using optimized algorithm.
TEST_H(TestScaleDown316HDOptInt, 4096, 32, 768, 6, true, ALLFLAGS, true, 1)

// Tests interpolated 3/16x scale down, using no SSSE3 optimized algorithm.
TEST_H(TestScaleDown316HDNoSSSE3OptInt, 4096, 32, 768, 6, true, NOSSSE3, true,
       1)

// Tests interpolated 3/16x scale down, using c_only optimized algorithm.
TEST_H(TestScaleDown316HDCOnlyOptInt, 4096, 32, 768, 6, true, NOSSE, true, 1)

// Test special sizes dont crash
// Tests scaling down to 1 pixel width
TEST_H(TestScaleDown1x6OptInt, 3, 24, 1, 6, true, ALLFLAGS, true, 4)

// Tests scaling down to 1 pixel height
TEST_H(TestScaleDown6x1OptInt, 24, 3, 6, 1, true, ALLFLAGS, true, 4)

// Tests scaling up from 1 pixel width
TEST_H(TestScaleUp1x6OptInt, 1, 6, 3, 24, true, ALLFLAGS, true, 4)

// Tests scaling up from 1 pixel height
TEST_H(TestScaleUp6x1OptInt, 6, 1, 24, 3, true, ALLFLAGS, true, 4)

// Test performance of a range of box filter scale sizes

// Tests interpolated 1/2x scale down, using optimized algorithm.
TEST_H(TestScaleDown2xHDOptInt, 1280, 720, 1280 / 2, 720 / 2, true, ALLFLAGS,
       true, 1)

// Tests interpolated 1/3x scale down, using optimized algorithm.
TEST_H(TestScaleDown3xHDOptInt, 1280, 720, 1280 / 3, 720 / 3, true, ALLFLAGS,
       true, 1)

// Tests interpolated 1/4x scale down, using optimized algorithm.
TEST_H(TestScaleDown4xHDOptInt, 1280, 720, 1280 / 4, 720 / 4, true, ALLFLAGS,
       true, 1)

// Tests interpolated 1/5x scale down, using optimized algorithm.
TEST_H(TestScaleDown5xHDOptInt, 1280, 720, 1280 / 5, 720 / 5, true, ALLFLAGS,
       true, 1)

// Tests interpolated 1/6x scale down, using optimized algorithm.
TEST_H(TestScaleDown6xHDOptInt, 1280, 720, 1280 / 6, 720 / 6, true, ALLFLAGS,
       true, 1)

// Tests interpolated 1/7x scale down, using optimized algorithm.
TEST_H(TestScaleDown7xHDOptInt, 1280, 720, 1280 / 7, 720 / 7, true, ALLFLAGS,
       true, 1)

// Tests interpolated 1/8x scale down, using optimized algorithm.
TEST_H(TestScaleDown8xHDOptInt, 1280, 720, 1280 / 8, 720 / 8, true, ALLFLAGS,
       true, 1)

// Tests interpolated 1/8x scale down, using optimized algorithm.
TEST_H(TestScaleDown9xHDOptInt, 1280, 720, 1280 / 9, 720 / 9, true, ALLFLAGS,
       true, 1)

// Tests interpolated 1/8x scale down, using optimized algorithm.
TEST_H(TestScaleDown10xHDOptInt, 1280, 720, 1280 / 10, 720 / 10, true, ALLFLAGS,
       true, 1)
