/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/base/videocommon.h"

#include <limits.h>  // For INT_MAX
#include <math.h>
#include <sstream>

#include "webrtc/base/arraysize.h"
#include "webrtc/base/common.h"

namespace cricket {

struct FourCCAliasEntry {
  uint32_t alias;
  uint32_t canonical;
};

static const FourCCAliasEntry kFourCCAliases[] = {
  {FOURCC_IYUV, FOURCC_I420},
  {FOURCC_YU16, FOURCC_I422},
  {FOURCC_YU24, FOURCC_I444},
  {FOURCC_YUYV, FOURCC_YUY2},
  {FOURCC_YUVS, FOURCC_YUY2},
  {FOURCC_HDYC, FOURCC_UYVY},
  {FOURCC_2VUY, FOURCC_UYVY},
  {FOURCC_JPEG, FOURCC_MJPG},  // Note: JPEG has DHT while MJPG does not.
  {FOURCC_DMB1, FOURCC_MJPG},
  {FOURCC_BA81, FOURCC_BGGR},
  {FOURCC_RGB3, FOURCC_RAW},
  {FOURCC_BGR3, FOURCC_24BG},
  {FOURCC_CM32, FOURCC_BGRA},
  {FOURCC_CM24, FOURCC_RAW},
};

uint32_t CanonicalFourCC(uint32_t fourcc) {
  for (uint32_t i = 0; i < arraysize(kFourCCAliases); ++i) {
    if (kFourCCAliases[i].alias == fourcc) {
      return kFourCCAliases[i].canonical;
    }
  }
  // Not an alias, so return it as-is.
  return fourcc;
}

static float kScaleFactors[] = {
  1.f / 1.f,  // Full size.
  1.f / 2.f,  // 1/2 scale.
  1.f / 4.f,  // 1/4 scale.
  1.f / 8.f,  // 1/8 scale.
  1.f / 16.f  // 1/16 scale.
};

static const int kNumScaleFactors = arraysize(kScaleFactors);

// Finds the scale factor that, when applied to width and height, produces
// fewer than num_pixels.
static float FindLowerScale(int width, int height, int target_num_pixels) {
  if (!target_num_pixels) {
    return 0.f;
  }
  int best_distance = INT_MAX;
  int best_index = kNumScaleFactors - 1;  // Default to max scale.
  for (int i = 0; i < kNumScaleFactors; ++i) {
    int test_num_pixels = static_cast<int>(width * kScaleFactors[i] *
                                           height * kScaleFactors[i]);
    int diff = target_num_pixels - test_num_pixels;
    if (diff >= 0 && diff < best_distance) {
      best_distance = diff;
      best_index = i;
      if (best_distance == 0) {  // Found exact match.
        break;
      }
    }
  }
  return kScaleFactors[best_index];
}

// Computes a scale less to fit in max_pixels while maintaining aspect ratio.
void ComputeScaleMaxPixels(int frame_width, int frame_height, int max_pixels,
    int* scaled_width, int* scaled_height) {
  ASSERT(scaled_width != NULL);
  ASSERT(scaled_height != NULL);
  ASSERT(max_pixels > 0);
  const int kMaxWidth = 4096;
  const int kMaxHeight = 3072;
  int new_frame_width = frame_width;
  int new_frame_height = frame_height;

  // Limit width.
  if (new_frame_width > kMaxWidth) {
    new_frame_height = new_frame_height * kMaxWidth / new_frame_width;
    new_frame_width = kMaxWidth;
  }
  // Limit height.
  if (new_frame_height > kMaxHeight) {
    new_frame_width = new_frame_width * kMaxHeight / new_frame_height;
    new_frame_height = kMaxHeight;
  }
  // Limit number of pixels.
  if (new_frame_width * new_frame_height > max_pixels) {
    // Compute new width such that width * height is less than maximum but
    // maintains original captured frame aspect ratio.
    new_frame_width = static_cast<int>(sqrtf(static_cast<float>(
        max_pixels) * new_frame_width / new_frame_height));
    new_frame_height = max_pixels / new_frame_width;
  }
  // Snap to a scale factor that is less than or equal to target pixels.
  float scale = FindLowerScale(frame_width, frame_height,
                               new_frame_width * new_frame_height);
  *scaled_width = static_cast<int>(frame_width * scale + .5f);
  *scaled_height = static_cast<int>(frame_height * scale + .5f);
}

// Compute a size to scale frames to that is below maximum compression
// and rendering size with the same aspect ratio.
void ComputeScale(int frame_width, int frame_height, int fps,
                  int* scaled_width, int* scaled_height) {
  // Maximum pixels limit is set to Retina MacBookPro 15" resolution of
  // 2880 x 1800 as of 4/18/2013.
  // For high fps, maximum pixels limit is set based on common 24" monitor
  // resolution of 2048 x 1280 as of 6/13/2013. The Retina resolution is
  // therefore reduced to 1440 x 900.
  int max_pixels = (fps > 5) ? 2048 * 1280 : 2880 * 1800;
  ComputeScaleMaxPixels(
      frame_width, frame_height, max_pixels, scaled_width, scaled_height);
}

// Compute size to crop video frame to.
// If cropped_format_* is 0, return the frame_* size as is.
void ComputeCrop(int cropped_format_width, int cropped_format_height,
                 int frame_width, int frame_height,
                 int pixel_width, int pixel_height,
                 int rotation,
                 int* cropped_width, int* cropped_height) {
  // Transform screen crop to camera space if rotated.
  if (rotation == 90 || rotation == 270) {
    std::swap(cropped_format_width, cropped_format_height);
  }
  ASSERT(cropped_format_width >= 0);
  ASSERT(cropped_format_height >= 0);
  ASSERT(frame_width > 0);
  ASSERT(frame_height > 0);
  ASSERT(pixel_width >= 0);
  ASSERT(pixel_height >= 0);
  ASSERT(rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270);
  ASSERT(cropped_width != NULL);
  ASSERT(cropped_height != NULL);
  if (!pixel_width) {
    pixel_width = 1;
  }
  if (!pixel_height) {
    pixel_height = 1;
  }
  // if cropped_format is 0x0 disable cropping.
  if (!cropped_format_height) {
    cropped_format_height = 1;
  }
  float frame_aspect = static_cast<float>(frame_width * pixel_width) /
      static_cast<float>(frame_height * pixel_height);
  float crop_aspect = static_cast<float>(cropped_format_width) /
      static_cast<float>(cropped_format_height);
  // kAspectThresh is the maximum aspect ratio difference that we'll accept
  // for cropping.  The value 1.34 allows cropping from 4:3 to 16:9.
  // Set to zero to disable cropping entirely.
  // TODO(fbarchard): crop to multiple of 16 width for better performance.
  const float kAspectThresh = 1.34f;
  // Wide aspect - crop horizontally
  if (frame_aspect > crop_aspect &&
      frame_aspect < crop_aspect * kAspectThresh) {
    // Round width down to multiple of 4 to avoid odd chroma width.
    // Width a multiple of 4 allows a half size image to have chroma channel
    // that avoids rounding errors.
    frame_width = static_cast<int>((crop_aspect * frame_height *
        pixel_height) / pixel_width + 0.5f) & ~3;
  } else if (frame_aspect < crop_aspect &&
             frame_aspect > crop_aspect / kAspectThresh) {
    frame_height = static_cast<int>((frame_width * pixel_width) /
        (crop_aspect * pixel_height) + 0.5f) & ~1;
  }
  *cropped_width = frame_width;
  *cropped_height = frame_height;
}

// Compute the frame size that makes pixels square pixel aspect ratio.
void ComputeScaleToSquarePixels(int in_width, int in_height,
                                int pixel_width, int pixel_height,
                                int* scaled_width, int* scaled_height) {
  *scaled_width = in_width;  // Keep width the same.
  *scaled_height = in_height * pixel_height / pixel_width;
}

// The C++ standard requires a namespace-scope definition of static const
// integral types even when they are initialized in the declaration (see
// [class.static.data]/4), but MSVC with /Ze is non-conforming and treats that
// as a multiply defined symbol error. See Also:
// http://msdn.microsoft.com/en-us/library/34h23df8.aspx
#ifndef _MSC_EXTENSIONS
const int64_t VideoFormat::kMinimumInterval;  // Initialized in header.
#endif

std::string VideoFormat::ToString() const {
  std::string fourcc_name = GetFourccName(fourcc) + " ";
  for (std::string::const_iterator i = fourcc_name.begin();
      i < fourcc_name.end(); ++i) {
    // Test character is printable; Avoid isprint() which asserts on negatives.
    if (*i < 32 || *i >= 127) {
      fourcc_name = "";
      break;
    }
  }

  std::ostringstream ss;
  ss << fourcc_name << width << "x" << height << "x"
     << IntervalToFpsFloat(interval);
  return ss.str();
}

}  // namespace cricket
