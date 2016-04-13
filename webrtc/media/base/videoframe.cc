/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/base/videoframe.h"

#include <string.h>

#include "libyuv/compare.h"
#include "libyuv/planar_functions.h"
#include "libyuv/scale.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/media/base/videocommon.h"

namespace cricket {

// Round to 2 pixels because Chroma channels are half size.
#define ROUNDTO2(v) (v & ~1)

bool VideoFrame::CopyToPlanes(uint8_t* dst_y,
                              uint8_t* dst_u,
                              uint8_t* dst_v,
                              int32_t dst_pitch_y,
                              int32_t dst_pitch_u,
                              int32_t dst_pitch_v) const {
  if (!GetYPlane() || !GetUPlane() || !GetVPlane()) {
    LOG(LS_ERROR) << "NULL plane pointer.";
    return false;
  }
  int32_t src_width = width();
  int32_t src_height = height();
  return libyuv::I420Copy(GetYPlane(), GetYPitch(),
                          GetUPlane(), GetUPitch(),
                          GetVPlane(), GetVPitch(),
                          dst_y, dst_pitch_y,
                          dst_u, dst_pitch_u,
                          dst_v, dst_pitch_v,
                          src_width, src_height) == 0;
}

size_t VideoFrame::ConvertToRgbBuffer(uint32_t to_fourcc,
                                      uint8_t* buffer,
                                      size_t size,
                                      int stride_rgb) const {
  const size_t needed = std::abs(stride_rgb) * static_cast<size_t>(height());
  if (size < needed) {
    LOG(LS_WARNING) << "RGB buffer is not large enough";
    return needed;
  }

  if (libyuv::ConvertFromI420(GetYPlane(), GetYPitch(), GetUPlane(),
                              GetUPitch(), GetVPlane(), GetVPitch(), buffer,
                              stride_rgb, width(), height(), to_fourcc)) {
    LOG(LS_ERROR) << "RGB type not supported: " << to_fourcc;
    return 0;  // 0 indicates error
  }
  return needed;
}

// TODO(fbarchard): Handle odd width/height with rounding.
// TODO(nisse): If method is kept, switch to using int instead of
// size_t and int32_t.
void VideoFrame::StretchToPlanes(uint8_t* dst_y,
                                 uint8_t* dst_u,
                                 uint8_t* dst_v,
                                 int32_t dst_pitch_y,
                                 int32_t dst_pitch_u,
                                 int32_t dst_pitch_v,
                                 size_t dst_width,
                                 size_t dst_height,
                                 bool interpolate,
                                 bool vert_crop) const {
  if (!GetYPlane() || !GetUPlane() || !GetVPlane()) {
    LOG(LS_ERROR) << "NULL plane pointer.";
    return;
  }

  size_t src_width = width();
  size_t src_height = height();
  if (dst_width == src_width && dst_height == src_height) {
    CopyToPlanes(dst_y, dst_u, dst_v, dst_pitch_y, dst_pitch_u, dst_pitch_v);
    return;
  }
  const uint8_t* src_y = GetYPlane();
  const uint8_t* src_u = GetUPlane();
  const uint8_t* src_v = GetVPlane();

  if (vert_crop) {
    // Adjust the input width:height ratio to be the same as the output ratio.
    if (src_width * dst_height > src_height * dst_width) {
      // Reduce the input width, but keep size/position aligned for YuvScaler
      src_width = ROUNDTO2(src_height * dst_width / dst_height);
      int32_t iwidth_offset = ROUNDTO2((width() - src_width) / 2);
      src_y += iwidth_offset;
      src_u += iwidth_offset / 2;
      src_v += iwidth_offset / 2;
    } else if (src_width * dst_height < src_height * dst_width) {
      // Reduce the input height.
      src_height = src_width * dst_height / dst_width;
      int32_t iheight_offset =
          static_cast<int32_t>((height() - src_height) >> 2);
      iheight_offset <<= 1;  // Ensure that iheight_offset is even.
      src_y += iheight_offset * GetYPitch();
      src_u += iheight_offset / 2 * GetUPitch();
      src_v += iheight_offset / 2 * GetVPitch();
    }
  }

  // Scale to the output I420 frame.
  libyuv::Scale(src_y, src_u, src_v,
                GetYPitch(), GetUPitch(), GetVPitch(),
                static_cast<int>(src_width), static_cast<int>(src_height),
                dst_y, dst_u, dst_v, dst_pitch_y, dst_pitch_u, dst_pitch_v,
                static_cast<int>(dst_width), static_cast<int>(dst_height),
                interpolate);
}

void VideoFrame::StretchToFrame(VideoFrame* dst,
                                bool interpolate, bool vert_crop) const {
  if (!dst) {
    LOG(LS_ERROR) << "NULL dst pointer.";
    return;
  }

  StretchToPlanes(dst->GetYPlane(), dst->GetUPlane(), dst->GetVPlane(),
                  dst->GetYPitch(), dst->GetUPitch(), dst->GetVPitch(),
                  dst->width(), dst->height(),
                  interpolate, vert_crop);
  dst->SetTimeStamp(GetTimeStamp());
  // Stretched frame should have the same rotation as the source.
  dst->set_rotation(rotation());
}

VideoFrame* VideoFrame::Stretch(size_t dst_width, size_t dst_height,
                                bool interpolate, bool vert_crop) const {
  VideoFrame* dest = CreateEmptyFrame(static_cast<int>(dst_width),
                                      static_cast<int>(dst_height),
                                      GetTimeStamp());
  if (dest) {
    StretchToFrame(dest, interpolate, vert_crop);
  }
  return dest;
}

bool VideoFrame::SetToBlack() {
  return libyuv::I420Rect(GetYPlane(), GetYPitch(),
                          GetUPlane(), GetUPitch(),
                          GetVPlane(), GetVPitch(),
                          0, 0,
                          width(), height(),
                          16, 128, 128) == 0;
}

static const size_t kMaxSampleSize = 1000000000u;
// Returns whether a sample is valid.
bool VideoFrame::Validate(uint32_t fourcc,
                          int w,
                          int h,
                          const uint8_t* sample,
                          size_t sample_size) {
  if (h < 0) {
    h = -h;
  }
  // 16384 is maximum resolution for VP8 codec.
  if (w < 1 || w > 16384 || h < 1 || h > 16384) {
    LOG(LS_ERROR) << "Invalid dimensions: " << w << "x" << h;
    return false;
  }
  uint32_t format = CanonicalFourCC(fourcc);
  int expected_bpp = 8;
  switch (format) {
    case FOURCC_I400:
    case FOURCC_RGGB:
    case FOURCC_BGGR:
    case FOURCC_GRBG:
    case FOURCC_GBRG:
      expected_bpp = 8;
      break;
    case FOURCC_I420:
    case FOURCC_I411:
    case FOURCC_YU12:
    case FOURCC_YV12:
    case FOURCC_M420:
    case FOURCC_NV21:
    case FOURCC_NV12:
      expected_bpp = 12;
      break;
    case FOURCC_I422:
    case FOURCC_YV16:
    case FOURCC_YUY2:
    case FOURCC_UYVY:
    case FOURCC_RGBP:
    case FOURCC_RGBO:
    case FOURCC_R444:
      expected_bpp = 16;
      break;
    case FOURCC_I444:
    case FOURCC_YV24:
    case FOURCC_24BG:
    case FOURCC_RAW:
      expected_bpp = 24;
      break;

    case FOURCC_ABGR:
    case FOURCC_BGRA:
    case FOURCC_ARGB:
      expected_bpp = 32;
      break;

    case FOURCC_MJPG:
    case FOURCC_H264:
      expected_bpp = 0;
      break;
    default:
      expected_bpp = 8;  // Expect format is at least 8 bits per pixel.
      break;
  }
  size_t expected_size = (w * expected_bpp + 7) / 8 * h;
  // For compressed formats, expect 4 bits per 16 x 16 macro.  I420 would be
  // 6 bits, but grey can be 4 bits.
  if (expected_bpp == 0) {
    expected_size = ((w + 15) / 16) * ((h + 15) / 16) * 4 / 8;
  }
  if (sample == NULL) {
    LOG(LS_ERROR) << "NULL sample pointer."
                  << " format: " << GetFourccName(format)
                  << " bpp: " << expected_bpp
                  << " size: " << w << "x" << h
                  << " expected: " << expected_size
                  << " " << sample_size;
    return false;
  }
  // TODO(fbarchard): Make function to dump information about frames.
  uint8_t four_samples[4] = {0, 0, 0, 0};
  for (size_t i = 0; i < arraysize(four_samples) && i < sample_size; ++i) {
    four_samples[i] = sample[i];
  }
  if (sample_size < expected_size) {
    LOG(LS_ERROR) << "Size field is too small."
                  << " format: " << GetFourccName(format)
                  << " bpp: " << expected_bpp
                  << " size: " << w << "x" << h
                  << " " << sample_size
                  << " expected: " << expected_size
                  << " sample[0..3]: " << static_cast<int>(four_samples[0])
                  << ", " << static_cast<int>(four_samples[1])
                  << ", " << static_cast<int>(four_samples[2])
                  << ", " << static_cast<int>(four_samples[3]);
    return false;
  }
  if (sample_size > kMaxSampleSize) {
    LOG(LS_WARNING) << "Size field is invalid."
                    << " format: " << GetFourccName(format)
                    << " bpp: " << expected_bpp
                    << " size: " << w << "x" << h
                    << " " << sample_size
                    << " expected: " << 2 * expected_size
                    << " sample[0..3]: " << static_cast<int>(four_samples[0])
                    << ", " << static_cast<int>(four_samples[1])
                    << ", " << static_cast<int>(four_samples[2])
                    << ", " << static_cast<int>(four_samples[3]);
    return false;
  }
  // Show large size warning once every 100 frames.
  // TODO(fbarchard): Make frame counter atomic for thread safety.
  static int large_warn100 = 0;
  size_t large_expected_size = expected_size * 2;
  if (expected_bpp >= 8 &&
      (sample_size > large_expected_size || sample_size > kMaxSampleSize) &&
      large_warn100 % 100 == 0) {
    ++large_warn100;
    LOG(LS_WARNING) << "Size field is too large."
                    << " format: " << GetFourccName(format)
                    << " bpp: " << expected_bpp
                    << " size: " << w << "x" << h
                    << " bytes: " << sample_size
                    << " expected: " << large_expected_size
                    << " sample[0..3]: " << static_cast<int>(four_samples[0])
                    << ", " << static_cast<int>(four_samples[1])
                    << ", " << static_cast<int>(four_samples[2])
                    << ", " << static_cast<int>(four_samples[3]);
  }

  // TODO(fbarchard): Add duplicate pixel check.
  // TODO(fbarchard): Use frame counter atomic for thread safety.
  static bool valid_once = true;
  if (valid_once) {
    valid_once = false;
    LOG(LS_INFO) << "Validate frame passed."
                 << " format: " << GetFourccName(format)
                 << " bpp: " << expected_bpp
                 << " size: " << w << "x" << h
                 << " bytes: " << sample_size
                 << " expected: " << expected_size
                 << " sample[0..3]: " << static_cast<int>(four_samples[0])
                 << ", " << static_cast<int>(four_samples[1])
                 << ", " << static_cast<int>(four_samples[2])
                 << ", " << static_cast<int>(four_samples[3]);
  }
  return true;
}

}  // namespace cricket
