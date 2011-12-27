/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/libyuv/include/libyuv.h"

#include <assert.h>

// LibYuv includes
#ifdef WEBRTC_ANDROID
#include "libyuv/files/include/libyuv.h"
#else
#include "third_party/libyuv/include/libyuv.h"
#endif

namespace webrtc {

VideoType RawVideoTypeToCommonVideoVideoType(RawVideoType type) {
  switch (type) {
    case kVideoI420:
      return kI420;
    case kVideoIYUV:
      return kIYUV;
    case kVideoRGB24:
      return kRGB24;
    case kVideoARGB:
      return kARGB;
    case kVideoARGB4444:
      return kARGB4444;
    case kVideoRGB565:
      return kRGB565;
    case kVideoARGB1555:
      return kARGB1555;
    case kVideoYUY2:
      return kYUY2;
    case kVideoYV12:
      return kYV12;
    case kVideoUYVY:
      return kUYVY;
    case kVideoNV21:
      return kNV21;
    case kVideoNV12:
      return kNV12;
    default:
      assert(false);
  }
  return kUnknown;
}

int CalcBufferSize(VideoType type, int width, int height) {
  int bits_per_pixel = 32;
  switch (type) {
    case kI420:
    case kNV12:
    case kNV21:
    case kIYUV:
    case kYV12:
      bits_per_pixel = 12;
      break;
    case kARGB4444:
    case kRGB565:
    case kARGB1555:
    case kYUY2:
    case kUYVY:
      bits_per_pixel = 16;
      break;
    case kRGB24:
      bits_per_pixel = 24;
      break;
    case kARGB:
      bits_per_pixel = 32;
      break;
    default:
      assert(false);
      return -1;
  }
  return (width * height * bits_per_pixel) / 8;  // bytes
}

int CalcBufferSize(VideoType src_video_type,
                   VideoType dst_video_type,
                   int length) {
  int src_bits_per_pixel = 32;
  switch (src_video_type) {
    case kI420:
    case kNV12:
    case kNV21:
    case kIYUV:
    case kYV12:
      src_bits_per_pixel = 12;
      break;
    case kARGB4444:
    case kRGB565:
    case kARGB1555:
    case kYUY2:
    case kUYVY:
      src_bits_per_pixel = 16;
      break;
    case kRGB24:
      src_bits_per_pixel = 24;
      break;
    case kARGB:
      src_bits_per_pixel = 32;
      break;
    default:
      assert(false);
      return -1;
  }

  int dst_bits_per_pixel = 32;
  switch (dst_video_type) {
    case kI420:
    case kIYUV:
    case kYV12:
      dst_bits_per_pixel = 12;
      break;
    case kARGB4444:
    case kRGB565:
    case kARGB1555:
    case kYUY2:
    case kUYVY:
      dst_bits_per_pixel = 16;
      break;
    case kRGB24:
      dst_bits_per_pixel = 24;
      break;
    case kARGB:
      dst_bits_per_pixel = 32;
      break;
    default:
      assert(false);
      return -1;
  }
  return (length * dst_bits_per_pixel) / src_bits_per_pixel;
}

int ConvertI420ToRGB24(const uint8_t* src_frame, uint8_t* dst_frame,
                       int width, int height) {
  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = yplane + width * height;
  const uint8_t* vplane = uplane + (width * height / 4);

  return libyuv::I420ToRGB24(yplane, width,
                             uplane, width / 2,
                             vplane, width / 2,
                             dst_frame, width * 3,
                             width, height);
}

int ConvertI420ToARGB(const uint8_t* src_frame, uint8_t* dst_frame,
                      int width, int height,
                      int dst_stride) {
  if (dst_stride == 0 || dst_stride == width)
    dst_stride = width * 4;
  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = src_frame + width * height;
  const uint8_t* vplane = uplane + (width * height / 4);

  return libyuv::I420ToARGB(yplane, width,
                            uplane, width / 2,
                            vplane, width / 2,
                            dst_frame, dst_stride,
                            width, height);
}

int ConvertI420ToRGBAMac(const uint8_t* src_frame,
                         uint8_t* dst_frame,
                         int width, int height,
                         int dst_stride) {
  // Equivalent to Convert YV12ToBGRA.
  // YV12 same as I420 with U and V swapped.
  if (dst_stride == 0 || dst_stride == width)
    dst_stride = 4 * width;
  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = src_frame + width * height;
  const uint8_t* vplane = uplane + (width * height / 4);

  return libyuv::I420ToBGRA(yplane, width,
                            vplane, width / 2,
                            uplane, width / 2,
                            dst_frame, dst_stride,
                            width, height);
}

int ConvertI420ToARGB4444(const uint8_t* src_frame,
                          uint8_t* dst_frame,
                          int width, int height,
                          int dst_stride) {
  if (dst_stride == 0 || dst_stride == width)
    dst_stride = 2 * width;
  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = src_frame + width * height;
  const uint8_t* vplane = uplane + (width * height / 4);

  return libyuv::I420ToARGB4444(yplane, width,
                                uplane, width / 2,
                                vplane, width / 2,
                                dst_frame, dst_stride,
                                width, height);
}

int ConvertI420ToRGB565(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width, int height) {
  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = src_frame + width * height;
  const uint8_t* vplane = uplane + (width * height / 4);

  return libyuv::I420ToRGB565(yplane, width,
                              uplane, width / 2,
                              vplane, width / 2,
                              dst_frame, width,
                              width, height);
}


// Same as ConvertI420ToRGB565 with a vertical flip.
int ConvertI420ToRGB565Android(const uint8_t* src_frame,
                               uint8_t* dst_frame,
                               int width, int height) {
  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = src_frame + width * height;
  const uint8_t* vplane = uplane + (width * height / 4);

  // Same as RGB565  + inversion - set negative height.
  height = -height;
  return libyuv::I420ToRGB565(yplane, width,
                              uplane, width / 2,
                              vplane, width / 2,
                              dst_frame, width,
                              width, height);
}

int ConvertI420ToARGB1555(const uint8_t* src_frame,
                          uint8_t* dst_frame,
                          int width, int height,
                          int dst_stride) {
  if (dst_stride == 0 || dst_stride == width)
    dst_stride = 2 * width;
  else if (dst_stride < 2 * width)
    return -1;

  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = src_frame + width * height;
  const uint8_t* vplane = uplane + (width * height / 4);

  return libyuv::I420ToARGB1555(yplane, width,
                                uplane, width / 2,
                                vplane, width / 2,
                                dst_frame, dst_stride,
                                width, height);
}

int ConvertI420ToYUY2(const uint8_t* src_frame, uint8_t* dst_frame,
                      int width, int height,
                      int dst_stride) {
  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = src_frame + width * height;
  const uint8_t* vplane = uplane + (width * height / 4);
  if (dst_stride == 0 || dst_stride == width)
    dst_stride = 2 * width;

  return libyuv::I420ToYUY2(yplane, width,
                            uplane, width / 2,
                            vplane, width / 2,
                            dst_frame, dst_stride,
                            width, height);
}

int ConvertI420ToUYVY(const uint8_t* src_frame, uint8_t* dst_frame,
                      int width, int height,
                      int dst_stride) {
  if (dst_stride == 0 || dst_stride == width)
    dst_stride = 2 * width;
  else if (dst_stride < width)
    return -1;
  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = src_frame + width * height;
  const uint8_t* vplane = uplane + (width * height / 4);

  return libyuv::I420ToUYVY(yplane, width,
                            uplane, width / 2,
                            vplane, width / 2,
                            dst_frame, dst_stride,
                            width, height);
}

int ConvertI420ToYV12(const uint8_t* src_frame, uint8_t* dst_frame,
                      int width, int height,
                      int dst_stride) {
  if (dst_stride == 0 || dst_stride == width)
    dst_stride = width;
  else if (dst_stride < width)
    return -1;

  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_frame + width * height;
  const uint8_t* src_vplane = src_uplane + (width * height / 4);
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);

  // YV12 is YVU => Use I420(YUV) copy and flip U and V.
  return libyuv::I420Copy(src_yplane, width,
                          src_vplane, width / 2,
                          src_uplane, width / 2,
                          dst_yplane, dst_stride,
                          dst_uplane, dst_stride / 2,
                          dst_vplane, dst_stride / 2,
                          width, height);
}

int ConvertYV12ToI420(const uint8_t* src_frame,
                      int width, int height,
                      uint8_t* dst_frame) {
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_frame + width * height;
  const uint8_t* src_vplane = src_uplane + (width * height / 4);
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);

  // YV12 is YVU => Use I420(YUV) copy and flip U and V.
  return libyuv::I420Copy(src_yplane, width,
                          src_vplane, width / 2,
                          src_uplane, width / 2,
                          dst_yplane, width,
                          dst_uplane, width / 2,
                          dst_vplane, width / 2,
                          width, height);
}

int ConvertNV12ToI420(const uint8_t* src_frame, uint8_t* dst_frame,
                      int width, int height) {
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uvplane = src_frame + width * height;
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);
  return libyuv::NV12ToI420(src_yplane, width,
                            src_uvplane, width,
                            dst_yplane, width,
                            dst_uplane, width / 2,
                            dst_vplane, width / 2,
                            width,  height);
}

int ConvertNV12ToI420AndRotate180(const uint8_t* src_frame,
                                  uint8_t* dst_frame,
                                  int width, int height) {
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uvplane = src_frame + width * height;
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);

  return libyuv::NV12ToI420Rotate(src_yplane, width,
                                  src_uvplane, width,
                                  dst_yplane, width,
                                  dst_uplane, width / 2,
                                  dst_vplane, width / 2,
                                  width, height,
                                  libyuv::kRotate180);
}

int ConvertNV12ToI420AndRotateClockwise(const uint8_t* src_frame,
                                        uint8_t* dst_frame,
                                        int width, int height) {
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uvplane = src_frame + width * height;
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);

  return libyuv::NV12ToI420Rotate(src_yplane, width,
                                  src_uvplane, width,
                                  dst_yplane, width,
                                  dst_uplane, width / 2,
                                  dst_vplane, width / 2,
                                  width, height,
                                  libyuv::kRotate90);
}

int ConvertNV12ToI420AndRotateAntiClockwise(const uint8_t* src_frame,
                                            uint8_t* dst_frame,
                                            int width,
                                            int height) {
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uvplane = src_frame + width * height;
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);

  return libyuv::NV12ToI420Rotate(src_yplane, width,
                                  src_uvplane, width,
                                  dst_yplane, width,
                                  dst_uplane, width / 2,
                                  dst_vplane, width / 2,
                                  width, height,
                                  libyuv::kRotate270);
}

int ConvertNV12ToRGB565(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width, int height) {
  const uint8_t* yplane = src_frame;
  const uint8_t* uvInterlaced = src_frame + (width * height);

  return libyuv::NV12ToRGB565(yplane, width,
                              uvInterlaced, width / 2,
                              dst_frame, width,
                              width, height);
}

int ConvertNV21ToI420(const uint8_t* src_frame, uint8_t* dst_frame,
                      int width, int height) {
  // NV21 = y plane followed by an interleaved V/U plane, i.e. same as NV12
  // but the U and the V are switched. Use the NV12 function and switch the U
  // and V planes.
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uvplane = src_frame + width * height;
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);
  return libyuv::NV12ToI420(src_yplane, width,
                            src_uvplane, width,
                            dst_yplane, width,
                            dst_vplane, width / 2,
                            dst_uplane, width / 2,
                            width,  height);
}

int ConvertNV21ToI420AndRotate180(const uint8_t* src_frame,
                                  uint8_t* dst_frame,
                                  int width, int height) {
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uvplane = src_frame + width * height;
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);

  return libyuv::NV12ToI420Rotate(src_yplane, width,
                                  src_uvplane, width,
                                  dst_yplane, width,
                                  dst_vplane, width / 2,
                                  dst_uplane, width / 2,
                                  width, height,
                                  libyuv::kRotate180);
}

int ConvertNV21ToI420AndRotateClockwise(const uint8_t* src_frame,
                                    uint8_t* dst_frame,
                                    int width, int height) {
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uvplane = src_frame + width * height;
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);

  return libyuv::NV12ToI420Rotate(src_yplane, width,
                                  src_uvplane, width,
                                  dst_yplane, width,
                                  dst_vplane, width / 2,
                                  dst_uplane, width / 2,
                                  width, height,
                                  libyuv::kRotate90);
}

int ConvertNV21ToI420AndRotateAntiClockwise(const uint8_t* src_frame,
                                            uint8_t* dst_frame,
                                            int width,
                                            int height) {
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uvplane = src_frame + width * height;
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);

  return libyuv::NV12ToI420Rotate(src_yplane, width,
                                  src_uvplane, width,
                                  dst_yplane, width,
                                  dst_vplane, width / 2,
                                  dst_uplane, width / 2,
                                  width, height,
                                  libyuv::kRotate270);
}

int ConvertI420ToRGBAIPhone(const uint8_t* src_frame,
                            uint8_t* dst_frame,
                            int width, int height,
                            int dst_stride) {
  if (dst_stride == 0 || dst_stride == width)
    dst_stride = 4 * width;
  else if (dst_stride < 4 * width)
    return -1;

  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = src_frame + width * height;
  const uint8_t* vplane = uplane + (width * height / 4);

  // RGBAIPhone = ABGR
  return libyuv::I420ToABGR(yplane, width,
                            uplane, width / 2,
                            vplane, width / 2,
                            dst_frame, dst_stride,
                            width, height);
}

int ConvertI420ToI420(const uint8_t* src_frame, uint8_t* dst_frame,
                      int width,
                      int height, int dst_stride) {
  if (dst_stride == 0)
    dst_stride = width;
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_frame + width * height;
  const uint8_t* src_vplane = src_uplane + (width * height / 4);
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);
  return libyuv::I420Copy(src_yplane, width,
                          src_uplane, width / 2,
                          src_vplane, width / 2,
                          dst_yplane, dst_stride,
                          dst_uplane, dst_stride / 2,
                          dst_vplane, dst_stride / 2,
                          width, height);
}

int ConvertUYVYToI420(int width, int height,
                      const uint8_t* src_frame, uint8_t* dst_frame) {
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);
  return libyuv::UYVYToI420(src_frame, 2 * width,
                            dst_yplane, width,
                            dst_uplane, width / 2,
                            dst_vplane, width / 2,
                            width, height);
}

int ConvertYUY2ToI420(int width, int height,
                      const uint8_t* src_frame, uint8_t* dst_frame) {
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);
  return libyuv::YUY2ToI420(src_frame, 2 * width,
                            dst_yplane, width,
                            dst_uplane, width / 2,
                            dst_vplane, width / 2,
                            width, height);
}

int ConvertRGB24ToARGB(const uint8_t* src_frame, uint8_t* dst_frame,
                       int width, int height, int dst_stride) {
  if (dst_stride == 0 || dst_stride == width)
    dst_stride = width;
  // Stride - currently webrtc style
  return libyuv::RGB24ToARGB(src_frame, width,
                             dst_frame, dst_stride,
                             width, height);
}

int ConvertRGB24ToI420(int width, int height,
                   const uint8_t* src_frame, uint8_t* dst_frame) {
  uint8_t* yplane = dst_frame;
  uint8_t* uplane = yplane + width * height;
  uint8_t* vplane = uplane + (width * height / 4);
  // WebRtc expects a vertical flipped image.
  return libyuv::RGB24ToI420(src_frame, width * 3,
                             yplane, width,
                             uplane, width / 2,
                             vplane, width / 2,
                             width, -height);
}

int ConvertI420ToARGBMac(const uint8_t* src_frame, uint8_t* dst_frame,
                         int width, int height, int dst_stride) {
  // Equivalent to YV12ToARGB.
  // YV12 = YVU => use I420 and interchange U and V.
  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = yplane + width * height;
  const uint8_t* vplane = uplane + (width * height / 4);

  if (dst_stride == 0 || dst_stride == width)
    dst_stride = 4 * width;
  else if (dst_stride < 4 * width)
    return -1;

  return libyuv::I420ToARGB(yplane, width,
                            vplane, width / 2,
                            uplane, width / 2,
                            dst_frame, dst_stride,
                            width, height);
}

int ConvertARGBMacToI420(int width, int height,
                         const uint8_t* src_frame, uint8_t* dst_frame) {
  // Equivalent to BGRAToI420
  uint8_t* yplane = dst_frame;
  uint8_t* uplane = yplane + width * height;
  uint8_t* vplane = uplane + (width * height / 4);
  return libyuv::BGRAToI420(src_frame, width * 4,
                            yplane, width,
                            uplane, width / 2,
                            vplane, width / 2,
                            width, height);
}

int ConvertToI420(VideoType src_video_type,
                  const uint8_t* src_frame,
                  int width,
                  int height,
                  uint8_t* dst_frame,
                  bool interlaced,
                  VideoRotationMode rotate /* =  kRotateNone  */) {
  switch (src_video_type) {
    case kRGB24:
      return ConvertRGB24ToI420(width, height, src_frame,
                                dst_frame);
    case kARGB:
      return ConvertARGBMacToI420(width, height, src_frame,
                                  dst_frame);
    case kI420:
      return I420Rotate(src_frame,
                        dst_frame,
                        width, height,
                        rotate);
    case kYUY2:
      return ConvertYUY2ToI420(width, height,
                               src_frame, dst_frame);
    case kUYVY:
      return ConvertUYVYToI420(width, height, src_frame,
                               dst_frame);
    case kYV12:
      switch (rotate) {
        case kRotateNone:
          return ConvertYV12ToI420(src_frame,
                                   width, height,
                                   dst_frame);
        case kRotate90:
          return ConvertToI420AndRotateClockwise(src_frame,
                                                 width,
                                                 height,
                                                 dst_frame,
                                                 height, width,
                                                 kYV12);
        case kRotate270:
          return ConvertToI420AndRotateAntiClockwise(src_frame,
                                                     width, height,
                                                     dst_frame,
                                                     height, width,
                                                     kYV12);
        case kRotate180:
          return ConvertToI420AndRotate180(src_frame,
                                           width, height,
                                           dst_frame,
                                           height, width,
                                           kYV12);
      }
    case kNV12:
      switch (rotate) {
        case kRotateNone:
          return ConvertNV12ToI420(src_frame, dst_frame,
                                   width, height);
        case kRotate90:
          return ConvertNV12ToI420AndRotateClockwise(src_frame,
                                                     dst_frame,
                                                     width, height);
        case kRotate270:
          return ConvertNV12ToI420AndRotateAntiClockwise(src_frame,
                                                         dst_frame,
                                                         width, height);
        case kRotate180:
          return ConvertNV12ToI420AndRotate180(src_frame,
                                               dst_frame,
                                               width, height);
      }
    case kNV21:
      switch (rotate) {
        case kRotateNone:
          return ConvertNV21ToI420(src_frame,
                                   dst_frame,
                                   width, height);
        case kRotate90:
          return ConvertNV21ToI420AndRotateClockwise(src_frame,
                                                     dst_frame,
                                                     width, height);
        case kRotate270:
          return ConvertNV21ToI420AndRotateAntiClockwise(src_frame,
                                                         dst_frame,
                                                         width, height);
        case kRotate180:
          return ConvertNV21ToI420AndRotate180(src_frame,
                                               dst_frame,
                                               width, height);
      }
      break;
    default:
      return -1;
  }
  return -1;
}

int ConvertFromI420(VideoType dst_video_type,
                    const uint8_t* src_frame,
                    //int width,
                    //int height,
                    WebRtc_UWord32 width,
                    WebRtc_UWord32 height,
                    uint8_t* dst_frame,
                    bool interlaced,
                    VideoRotationMode rotate) {
  switch (dst_video_type) {
    case kRGB24:
      return ConvertI420ToRGB24(src_frame, dst_frame, width, height);
    case kARGB:
      return ConvertI420ToARGB(src_frame, dst_frame, width, height, 0);
    case kARGB4444:
      return ConvertI420ToARGB4444(src_frame, dst_frame, width, height, 0);
    case kARGB1555:
      return ConvertI420ToARGB1555(src_frame, dst_frame, width, height, 0);
    case kRGB565:
      return ConvertI420ToRGB565(src_frame, dst_frame, width, height);
    case kI420:
      return ConvertI420ToI420(src_frame, dst_frame, width, height, width);
    case kUYVY:
      return ConvertI420ToUYVY(src_frame, dst_frame, width, height);
    case kYUY2:
      return ConvertI420ToYUY2(src_frame, dst_frame, width, height, 0);
    case kYV12:
      return ConvertI420ToYV12(src_frame, dst_frame, width, height, 0);
    case kRGBAMac:
      return ConvertI420ToRGBAMac(src_frame, dst_frame, width, height, 0);
    case kARGBMac:
      return ConvertI420ToARGBMac(src_frame, dst_frame, width, height, 0);
    default:
      return -1;
  }
}

int MirrorI420LeftRight(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width, int height) {
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_yplane + width * height;
  const uint8_t* src_vplane = src_uplane + (width * height / 4);
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_yplane + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);
  return libyuv::I420Mirror(src_yplane, width,
                            src_uplane, width / 2,
                            src_vplane, width / 2,
                            dst_yplane, width,
                            dst_uplane, width / 2,
                            dst_vplane, width / 2,
                            width, height);
}

int MirrorI420UpDown(const uint8_t* src_frame, uint8_t* dst_frame,
                     int width, int height) {
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_frame + width * height;
  const uint8_t* src_vplane = src_uplane + (width * height / 4);
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);

  // Inserting negative height flips the frame.
  return libyuv::I420Copy(src_yplane, width,
                          src_uplane, width / 2,
                          src_vplane, width / 2,
                          dst_yplane, width,
                          dst_uplane, width / 2,
                          dst_vplane, width / 2,
                          width, -height);
}

int ConvertToI420AndMirrorUpDown(const uint8_t* src_frame,
                                 uint8_t* dst_frame,
                                 int src_width, int src_height,
                                 VideoType src_video_type) {
  if (src_video_type != kI420 && src_video_type != kYV12)
    return -1;
  // TODO(mikhal): Use a more general convert function - with negative height
  // Inserting negative height flips the frame.
  // Using I420Copy with a negative height.
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_frame + src_width * src_height;
  const uint8_t* src_vplane = src_uplane + (src_width * src_height / 4);
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + src_width * src_height;
  uint8_t* dst_vplane = dst_uplane + (src_width * src_height / 4);
  if (src_video_type == kYV12) {
    // Switch U and V
    dst_vplane = dst_frame + src_width * src_height;
    dst_uplane = dst_vplane + (src_width * src_height / 4);
  }
  // Inserting negative height flips the frame.
  return libyuv::I420Copy(src_yplane, src_width,
                          src_uplane, src_width / 2,
                          src_vplane, src_width / 2,
                          dst_yplane, src_width,
                          dst_uplane, src_width / 2,
                          dst_vplane, src_width / 2,
                          src_width, -src_height);
}

int I420Rotate(const uint8_t* src_frame,
               uint8_t* dst_frame,
               int width, int height,
               VideoRotationMode rotation_mode) {
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_frame + width * height;
  const uint8_t* src_vplane = src_uplane + (width * height / 4);
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);
  return libyuv::I420Rotate(src_yplane, width,
                            src_uplane, width / 2,
                            src_vplane, width / 2,
                            dst_yplane, width,
                            dst_uplane, width / 2,
                            dst_vplane, width / 2,
                            width, height,
                            static_cast<libyuv::RotationMode>(rotation_mode));
}

// TODO(mikhal): modify API to use only the general function.
int ConvertToI420AndRotateClockwise(const uint8_t* src_frame,
                                    int src_width,
                                    int src_height,
                                    uint8_t* dst_frame,
                                    int dst_width,
                                    int dst_height,
                                    VideoType src_video_type) {
  if (src_video_type != kI420 && src_video_type != kYV12)
     return -1;
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_frame + src_width * src_height;
  const uint8_t* src_vplane = src_uplane + (src_width * src_height / 4);
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + src_width * src_height;
  uint8_t* dst_vplane = dst_uplane + (src_width * src_height / 4);
  if (src_video_type == kYV12) {
    // Switch U and V
    dst_vplane = dst_frame + src_width * src_height;
    dst_uplane = dst_vplane + (src_width * src_height / 4);
  }
  return libyuv::I420Rotate(src_yplane, src_width,
                            src_uplane, src_width / 2,
                            src_vplane, src_width / 2,
                            dst_yplane, src_width,
                            dst_uplane, src_width / 2,
                            dst_vplane, src_width / 2,
                            src_width, src_height,
                            libyuv::kRotate90);
}

// TODO(mikhal): modify API to use only the general function.
int ConvertToI420AndRotateAntiClockwise(const uint8_t* src_frame,
                                        int src_width,
                                        int src_height,
                                        uint8_t* dst_frame,
                                        int dst_width,
                                        int dst_height,
                                        VideoType src_video_type) {
  if (src_video_type != kI420 && src_video_type != kYV12)
    return -1;
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_frame + src_width * src_height;
  const uint8_t* src_vplane = src_uplane + (src_width * src_height / 4);
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + src_width * src_height;
  uint8_t* dst_vplane = dst_uplane + (src_width * src_height / 4);
  if (src_video_type == kYV12) {
    // Switch U and V
    dst_vplane = dst_frame + src_width * src_height;
    dst_uplane = dst_vplane + (src_width * src_height / 4);
  }
  return libyuv::I420Rotate(src_yplane, src_width,
                            src_uplane, src_width / 2,
                            src_vplane, src_width / 2,
                            dst_yplane, src_width,
                            dst_uplane, src_width / 2,
                            dst_vplane, src_width / 2,
                            src_width, src_height,
                            libyuv::kRotate270);
}

// TODO(mikhal): modify API to use only the general function.
int ConvertToI420AndRotate180(const uint8_t* src_frame,
                              int src_width,
                              int src_height,
                              uint8_t* dst_frame,
                              int dst_width,
                              int dst_height,
                              VideoType src_video_type) {
  if (src_video_type != kI420 && src_video_type != kYV12)
    return -1;
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_frame + src_width * src_height;
  const uint8_t* src_vplane = src_uplane + (src_width * src_height / 4);
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + src_width * src_height;
  uint8_t* dst_vplane = dst_uplane + (src_width * src_height / 4);
  if (src_video_type == kYV12) {
    // Switch U and V
    dst_vplane = dst_frame + src_width * src_height;
    dst_uplane = dst_vplane + (src_width * src_height / 4);
  }
  return libyuv::I420Rotate(src_yplane, src_width,
                            src_uplane, src_width / 2,
                            src_vplane, src_width / 2,
                            dst_yplane, src_width,
                            dst_uplane, src_width / 2,
                            dst_vplane, src_width / 2,
                            src_width, src_height,
                            libyuv::kRotate180);
}

// Compute PSNR for an I420 frame (all planes)
double I420PSNR(const uint8_t* ref_frame,
                const uint8_t* test_frame,
                int width, int height) {
  if (!ref_frame || !test_frame)
    return -1;
  else if (height < 0 || width < 0)
    return -1;
  const uint8_t* src_y_a = ref_frame;
  const uint8_t* src_u_a = src_y_a + width * height;
  const uint8_t* src_v_a = src_u_a + (width * height / 4);
  const uint8_t* src_y_b = test_frame;
  const uint8_t* src_u_b = src_y_b + width * height;
  const uint8_t* src_v_b = src_u_b + (width * height / 4);
  int stride_y = width;
  int stride_uv = (width + 1) / 2;
  double psnr = libyuv::I420Psnr(src_y_a, stride_y,
                                 src_u_a, stride_uv,
                                 src_v_a, stride_uv,
                                 src_y_b, stride_y,
                                 src_u_b, stride_uv,
                                 src_v_b, stride_uv,
                                 width, height);
  // LibYuv sets the max psnr value to 128, we restrict it to 48.
  // In case of 0 mse in one frame, 128 can skew the results significantly.
  return (psnr > 48.0) ? 48.0 : psnr;
}
// Compute SSIM for an I420 frame (all planes)
double I420SSIM(const uint8_t* ref_frame,
                const uint8_t* test_frame,
                int width, int height) {
  if (!ref_frame || !test_frame)
     return -1;
  else if (height < 0 || width < 0)
     return -1;
  const uint8_t* src_y_a = ref_frame;
  const uint8_t* src_u_a = src_y_a + width * height;
  const uint8_t* src_v_a = src_u_a + (width * height / 4);
  const uint8_t* src_y_b = test_frame;
  const uint8_t* src_u_b = src_y_b + width * height;
  const uint8_t* src_v_b = src_u_b + (width * height / 4);
  int stride_y = width;
  int stride_uv = (width + 1) / 2;
  return libyuv::I420Ssim(src_y_a, stride_y,
                          src_u_a, stride_uv,
                          src_v_a, stride_uv,
                          src_y_b, stride_y,
                          src_u_b, stride_uv,
                          src_v_b, stride_uv,
                          width, height);

}

}  // namespace webrtc
