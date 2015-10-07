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

// Common definition for video, including fourcc and VideoFormat.

#ifndef TALK_MEDIA_BASE_VIDEOCOMMON_H_  // NOLINT
#define TALK_MEDIA_BASE_VIDEOCOMMON_H_

#include <string>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/timeutils.h"

namespace cricket {

// TODO(janahan): For now, a hard-coded ssrc is used as the video ssrc.
// This is because when the video frame is passed to the mediaprocessor for
// processing, it doesn't have the correct ssrc. Since currently only Tx
// Video processing is supported, this is ok. When we switch over to trigger
// from capturer, this should be fixed and this const removed.
const uint32_t kDummyVideoSsrc = 0xFFFFFFFF;

// Minimum interval is 10k fps.
#define FPS_TO_INTERVAL(fps) \
    (fps ? rtc::kNumNanosecsPerSec / fps : \
    rtc::kNumNanosecsPerSec / 10000)

//////////////////////////////////////////////////////////////////////////////
// Definition of FourCC codes
//////////////////////////////////////////////////////////////////////////////
// Convert four characters to a FourCC code.
// Needs to be a macro otherwise the OS X compiler complains when the kFormat*
// constants are used in a switch.
#define FOURCC(a, b, c, d)                                        \
  ((static_cast<uint32_t>(a)) | (static_cast<uint32_t>(b) << 8) | \
   (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24))
// Some pages discussing FourCC codes:
//   http://www.fourcc.org/yuv.php
//   http://v4l2spec.bytesex.org/spec/book1.htm
//   http://developer.apple.com/quicktime/icefloe/dispatch020.html
//   http://msdn.microsoft.com/library/windows/desktop/dd206750.aspx#nv12
//   http://people.xiph.org/~xiphmont/containers/nut/nut4cc.txt

// FourCC codes grouped according to implementation efficiency.
// Primary formats should convert in 1 efficient step.
// Secondary formats are converted in 2 steps.
// Auxilliary formats call primary converters.
enum FourCC {
  // 9 Primary YUV formats: 5 planar, 2 biplanar, 2 packed.
  FOURCC_I420 = FOURCC('I', '4', '2', '0'),
  FOURCC_I422 = FOURCC('I', '4', '2', '2'),
  FOURCC_I444 = FOURCC('I', '4', '4', '4'),
  FOURCC_I411 = FOURCC('I', '4', '1', '1'),
  FOURCC_I400 = FOURCC('I', '4', '0', '0'),
  FOURCC_NV21 = FOURCC('N', 'V', '2', '1'),
  FOURCC_NV12 = FOURCC('N', 'V', '1', '2'),
  FOURCC_YUY2 = FOURCC('Y', 'U', 'Y', '2'),
  FOURCC_UYVY = FOURCC('U', 'Y', 'V', 'Y'),

  // 2 Secondary YUV formats: row biplanar.
  FOURCC_M420 = FOURCC('M', '4', '2', '0'),

  // 9 Primary RGB formats: 4 32 bpp, 2 24 bpp, 3 16 bpp.
  FOURCC_ARGB = FOURCC('A', 'R', 'G', 'B'),
  FOURCC_BGRA = FOURCC('B', 'G', 'R', 'A'),
  FOURCC_ABGR = FOURCC('A', 'B', 'G', 'R'),
  FOURCC_24BG = FOURCC('2', '4', 'B', 'G'),
  FOURCC_RAW  = FOURCC('r', 'a', 'w', ' '),
  FOURCC_RGBA = FOURCC('R', 'G', 'B', 'A'),
  FOURCC_RGBP = FOURCC('R', 'G', 'B', 'P'),  // bgr565.
  FOURCC_RGBO = FOURCC('R', 'G', 'B', 'O'),  // abgr1555.
  FOURCC_R444 = FOURCC('R', '4', '4', '4'),  // argb4444.

  // 4 Secondary RGB formats: 4 Bayer Patterns.
  FOURCC_RGGB = FOURCC('R', 'G', 'G', 'B'),
  FOURCC_BGGR = FOURCC('B', 'G', 'G', 'R'),
  FOURCC_GRBG = FOURCC('G', 'R', 'B', 'G'),
  FOURCC_GBRG = FOURCC('G', 'B', 'R', 'G'),

  // 1 Primary Compressed YUV format.
  FOURCC_MJPG = FOURCC('M', 'J', 'P', 'G'),

  // 5 Auxiliary YUV variations: 3 with U and V planes are swapped, 1 Alias.
  FOURCC_YV12 = FOURCC('Y', 'V', '1', '2'),
  FOURCC_YV16 = FOURCC('Y', 'V', '1', '6'),
  FOURCC_YV24 = FOURCC('Y', 'V', '2', '4'),
  FOURCC_YU12 = FOURCC('Y', 'U', '1', '2'),  // Linux version of I420.
  FOURCC_J420 = FOURCC('J', '4', '2', '0'),
  FOURCC_J400 = FOURCC('J', '4', '0', '0'),

  // 14 Auxiliary aliases.  CanonicalFourCC() maps these to canonical fourcc.
  FOURCC_IYUV = FOURCC('I', 'Y', 'U', 'V'),  // Alias for I420.
  FOURCC_YU16 = FOURCC('Y', 'U', '1', '6'),  // Alias for I422.
  FOURCC_YU24 = FOURCC('Y', 'U', '2', '4'),  // Alias for I444.
  FOURCC_YUYV = FOURCC('Y', 'U', 'Y', 'V'),  // Alias for YUY2.
  FOURCC_YUVS = FOURCC('y', 'u', 'v', 's'),  // Alias for YUY2 on Mac.
  FOURCC_HDYC = FOURCC('H', 'D', 'Y', 'C'),  // Alias for UYVY.
  FOURCC_2VUY = FOURCC('2', 'v', 'u', 'y'),  // Alias for UYVY on Mac.
  FOURCC_JPEG = FOURCC('J', 'P', 'E', 'G'),  // Alias for MJPG.
  FOURCC_DMB1 = FOURCC('d', 'm', 'b', '1'),  // Alias for MJPG on Mac.
  FOURCC_BA81 = FOURCC('B', 'A', '8', '1'),  // Alias for BGGR.
  FOURCC_RGB3 = FOURCC('R', 'G', 'B', '3'),  // Alias for RAW.
  FOURCC_BGR3 = FOURCC('B', 'G', 'R', '3'),  // Alias for 24BG.
  FOURCC_CM32 = FOURCC(0, 0, 0, 32),  // Alias for BGRA kCMPixelFormat_32ARGB
  FOURCC_CM24 = FOURCC(0, 0, 0, 24),  // Alias for RAW kCMPixelFormat_24RGB

  // 1 Auxiliary compressed YUV format set aside for capturer.
  FOURCC_H264 = FOURCC('H', '2', '6', '4'),
};

// Match any fourcc.

// We move this out of the enum because using it in many places caused
// the compiler to get grumpy, presumably since the above enum is
// backed by an int.
static const uint32_t FOURCC_ANY = 0xFFFFFFFF;

// Converts fourcc aliases into canonical ones.
uint32_t CanonicalFourCC(uint32_t fourcc);

// Get FourCC code as a string.
inline std::string GetFourccName(uint32_t fourcc) {
  std::string name;
  name.push_back(static_cast<char>(fourcc & 0xFF));
  name.push_back(static_cast<char>((fourcc >> 8) & 0xFF));
  name.push_back(static_cast<char>((fourcc >> 16) & 0xFF));
  name.push_back(static_cast<char>((fourcc >> 24) & 0xFF));
  return name;
}

// Computes a scale less to fit in max_pixels while maintaining aspect ratio.
void ComputeScaleMaxPixels(int frame_width, int frame_height, int max_pixels,
                           int* scaled_width, int* scaled_height);

// For low fps, max pixels limit is set to Retina MacBookPro 15" resolution of
// 2880 x 1800 as of 4/18/2013.
// For high fps, maximum pixels limit is set based on common 24" monitor
// resolution of 2048 x 1280 as of 6/13/2013. The Retina resolution is
// therefore reduced to 1440 x 900.
void ComputeScale(int frame_width, int frame_height, int fps,
                  int* scaled_width, int* scaled_height);

// Compute the frame size that conversion should crop to based on aspect ratio.
// Ensures size is multiple of 2 due to I420 and conversion limitations.
void ComputeCrop(int cropped_format_width, int cropped_format_height,
                 int frame_width, int frame_height,
                 int pixel_width, int pixel_height,
                 int rotation,
                 int* cropped_width, int* cropped_height);

// Compute the frame size that makes pixels square pixel aspect ratio.
void ComputeScaleToSquarePixels(int in_width, int in_height,
                                int pixel_width, int pixel_height,
                                int* scaled_width, int* scaled_height);

//////////////////////////////////////////////////////////////////////////////
// Definition of VideoFormat.
//////////////////////////////////////////////////////////////////////////////

// VideoFormat with Plain Old Data for global variables.
struct VideoFormatPod {
  int width;  // Number of pixels.
  int height;  // Number of pixels.
  int64_t interval;  // Nanoseconds.
  uint32_t fourcc;  // Color space. FOURCC_ANY means that any color space is OK.
};

struct VideoFormat : VideoFormatPod {
  static const int64_t kMinimumInterval =
      rtc::kNumNanosecsPerSec / 10000;  // 10k fps.

  VideoFormat() {
    Construct(0, 0, 0, 0);
  }

  VideoFormat(int w, int h, int64_t interval_ns, uint32_t cc) {
    Construct(w, h, interval_ns, cc);
  }

  explicit VideoFormat(const VideoFormatPod& format) {
    Construct(format.width, format.height, format.interval, format.fourcc);
  }

  void Construct(int w, int h, int64_t interval_ns, uint32_t cc) {
    width = w;
    height = h;
    interval = interval_ns;
    fourcc = cc;
  }

  static int64_t FpsToInterval(int fps) {
    return fps ? rtc::kNumNanosecsPerSec / fps : kMinimumInterval;
  }

  static int IntervalToFps(int64_t interval) {
    if (!interval) {
      return 0;
    }
    return static_cast<int>(rtc::kNumNanosecsPerSec / interval);
  }

  static float IntervalToFpsFloat(int64_t interval) {
    if (!interval) {
      return 0.f;
    }
    return static_cast<float>(rtc::kNumNanosecsPerSec) /
        static_cast<float>(interval);
  }

  bool operator==(const VideoFormat& format) const {
    return width == format.width && height == format.height &&
        interval == format.interval && fourcc == format.fourcc;
  }

  bool operator!=(const VideoFormat& format) const {
    return !(*this == format);
  }

  bool operator<(const VideoFormat& format) const {
    return (fourcc < format.fourcc) ||
        (fourcc == format.fourcc && width < format.width) ||
        (fourcc == format.fourcc && width == format.width &&
            height < format.height) ||
        (fourcc == format.fourcc && width == format.width &&
            height == format.height && interval > format.interval);
  }

  int framerate() const { return IntervalToFps(interval); }

  // Check if both width and height are 0.
  bool IsSize0x0() const { return 0 == width && 0 == height; }

  // Check if this format is less than another one by comparing the resolution
  // and frame rate.
  bool IsPixelRateLess(const VideoFormat& format) const {
    return width * height * framerate() <
        format.width * format.height * format.framerate();
  }

  // Get a string presentation in the form of "fourcc width x height x fps"
  std::string ToString() const;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_VIDEOCOMMON_H_  // NOLINT
