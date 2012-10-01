/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * WebRTC's wrapper to libyuv.
 */

#ifndef WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_WEBRTC_LIBYUV_H_
#define WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_WEBRTC_LIBYUV_H_

#include "common_types.h"  // RawVideoTypes.
#include "modules/interface/module_common_types.h"  // VideoFrame
#include "typedefs.h"

namespace webrtc {

// TODO(mikhal): 1. Sync libyuv and WebRtc meaning of stride.
//               2. Reorder parameters for consistency.

// Supported video types.
enum VideoType {
  kUnknown,
  kI420,
  kIYUV,
  kRGB24,
  kABGR,
  kARGB,
  kARGB4444,
  kRGB565,
  kARGB1555,
  kYUY2,
  kYV12,
  kUYVY,
  kMJPG,
  kNV21,
  kNV12,
  kBGRA,
};

// Conversion between the RawVideoType and the LibYuv videoType.
// TODO(wu): Consolidate types into one type throughout WebRtc.
VideoType RawVideoTypeToCommonVideoVideoType(RawVideoType type);

// Supported rotation
// Direction of rotation - clockwise.
enum VideoRotationMode {
  kRotateNone = 0,
  kRotate90 = 90,
  kRotate180 = 180,
  kRotate270 = 270,
};

// Align integer values.
// Input:
//   - value     : Input value to be aligned.
//   - alignment : Alignment basis (power of 2).
// Return value: An aligned form of the input value.
int AlignInt(int value, int alignment);

// Calculate the required buffer size.
// Input:
//   - type         :The type of the designated video frame.
//   - width        :frame width in pixels.
//   - height       :frame height in pixels.
// Return value:    :The required size in bytes to accommodate the specified
//                   video frame or -1 in case of an error .
int CalcBufferSize(VideoType type, int width, int height);

// Convert To I420
// Input:
//   - src_video_type   : Type of input video.
//   - src_frame        : Pointer to a source frame.
//   - crop_x/crop_y    : Starting positions for cropping (0 for no crop).
//   - src_width        : src width in pixels.
//   - src_height       : src height in pixels.
//   - sample_size      : Required only for the parsing of MJPG (set to 0 else).
//   - rotate           : Rotation mode of output image.
// Output:
//   - dst_frame        : Reference to a destination frame.
// Return value: 0 if OK, < 0 otherwise.

int ConvertToI420(VideoType src_video_type,
                  const uint8_t* src_frame,
                  int crop_x, int crop_y,
                  int src_width, int src_height,
                  int sample_size,
                  VideoRotationMode rotation,
                  VideoFrame* dst_frame);

// Convert From I420
// Input:
//   - src_frame        : Pointer to a source frame.
//   - src_stride       : Number of bytes in a row of the src Y plane.
//   - dst_video_type   : Type of output video.
//   - dst_sample_size  : Required only for the parsing of MJPG.
//   - dst_frame        : Pointer to a destination frame.
// Return value: 0 if OK, < 0 otherwise.
// It is assumed that source and destination have equal height.
int ConvertFromI420(const VideoFrame& src_frame, int src_stride,
                    VideoType dst_video_type, int dst_sample_size,
                    uint8_t* dst_frame);
// ConvertFrom YV12.
// Interface - same as above.
int ConvertFromYV12(const uint8_t* src_frame, int src_stride,
                    VideoType dst_video_type, int dst_sample_size,
                    int width, int height,
                    uint8_t* dst_frame);

// The following list describes designated conversion functions which
// are not covered by the previous general functions.
// Input and output descriptions mostly match the above descriptions, and are
// therefore omitted.
int ConvertRGB24ToARGB(const uint8_t* src_frame,
                       uint8_t* dst_frame,
                       int width, int height,
                       int dst_stride);
int ConvertNV12ToRGB565(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width, int height);

// Mirror functions
// The following 2 functions perform mirroring on a given image
// (LeftRight/UpDown).
// Input:
//    - src_frame   : Pointer to a source frame.
//    - dst_frame   : Pointer to a destination frame.
// Return value: 0 if OK, < 0 otherwise.
// It is assumed that src and dst frames have equal dimensions.
int MirrorI420LeftRight(const VideoFrame* src_frame,
                        VideoFrame* dst_frame);
int MirrorI420UpDown(const VideoFrame* src_frame,
                     VideoFrame* dst_frame);

// Compute PSNR for an I420 frame (all planes).
double I420PSNR(const uint8_t* ref_frame,
                const uint8_t* test_frame,
                int width, int height);
// Compute SSIM for an I420 frame (all planes).
double I420SSIM(const uint8_t* ref_frame,
                const uint8_t* test_frame,
                int width, int height);
}

#endif  // WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_WEBRTC_LIBYUV_H_
