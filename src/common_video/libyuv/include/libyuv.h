/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * WebRTC's Wrapper to libyuv.
 */

#ifndef WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_LIBYUV_H_
#define WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_LIBYUV_H_

#include "common_types.h"  // RawVideoTypes.
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
  kARGBMac, // TODO (mikhal): remove
  kRGBAMac, // TODO (mikhal): remove
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

// Calculate the required buffer size.
// Input:
//   - type - The type of the designated video frame.
//   - width - frame width in pixels.
//   - height - frame height in pixels.
// Return value:  The required size in bytes to accommodate the specified
//                video frame or -1 in case of an error .
int CalcBufferSize(VideoType type, int width, int height);

// Compute required buffer size when converting from one type to another.
// Input:
//   - src_video_type - Type of the existing video frame.
//   - dst_video_type - Type of the designated video frame.
//   - length - length in bytes of the data.
// Return value: The required size in bytes to accommodate the specified
//               converted video frame or -1 in case of an error.
int CalcBufferSize(VideoType src_video_type,
                   VideoType dst_video_type,
                   int length);
// TODO (mikhal): Merge the two functions above.

// TODO(mikhal): If WebRTC doesn't switch to three plane representation,
// use helper functions for the planes and widths.

// Convert To/From I420
// The following two functions convert an image to/from a I420 type to/from
// a specified format.
//
// Input:
//   - src_video_type  : Type of input video
//   - src_frame   : Pointer to a source frame.
//   - width       : Image width in pixels.
//   - height      : Image height in pixels.
//   - dst_frame   : Pointer to a destination frame.
//   - interlaced  : Flag indicating if interlaced I420 output.
//   - rotate      : Rotation mode of output image.
// Return value: 0 if OK, < 0 otherwise.
//
// Note: the following functions includes the most common usage cases; for
// a more specific usage, refer to explicit function.
int ConvertToI420(VideoType src_video_type,
                  const uint8_t* src_frame,
                  int width,
                  int height,
                  uint8_t* dst_frame,
                  bool interlaced,
                  VideoRotationMode rotate);

// The previous function will soon be removed.
// TODO(mikhal): Remove legacy function after integration.

// Convert From/To I420
// Input:
//   - src_video_type   : Type of input video.
//   - src_frame        : Pointer to a source frame.
//   - crop_x/crop_y    : Starting positions for cropping (0 for no crop).
//   - src/dst_width    : src/dst width in pixels.
//   - src/dst_height   : src/dst height in pixels.
//   - sample_size      : Required only for the parsing of MJPG (set to 0 else).
//   - dst_stride       : Number of bytes in a row of the dst Y plane.
//   - rotate           : Rotation mode of output image.
// Output:
//   - dst_frame        : Pointer to a destination frame.
// Return value: 0 if OK, < 0 otherwise.

int ConvertToI420(VideoType src_video_type,
                  const uint8_t* src_frame,
                  int crop_x, int crop_y,
                  int src_width, int src_height,
                  int sample_size,
                  int dst_width, int dst_height, int dst_stride,
                  VideoRotationMode rotation,
                  uint8_t* dst_frame);

// TODO(andrew): return to the int width and height types. This was swapped
// temporarily to satisfy a linking error with the libjingle revision we and
// Chrome pull, due to the removed vplib.
int ConvertFromI420(VideoType dst_video_type,
                    const uint8_t* src_frame,
                    //int width,
                    //int height,
                    WebRtc_UWord32 width,
                    WebRtc_UWord32 height,
                    uint8_t* dst_frame,
                    bool interlaced,
                    VideoRotationMode rotate);
// The previous function will soon be removed.
// TODO(mikhal): Remove legacy function after integration.
// Input:
//   - src_frame        : Pointer to a source frame.
//   - src_stride       : Number of bytes in a row of the src Y plane.
//   - dst_video_type   : Type of output video.
//   - dst_sample_size  : Required only for the parsing of MJPG.
//   - width            : Width in pixels.
//   - height           : Height in pixels.
//   - dst_frame        : Pointer to a source frame.

//   - dst_frame        : Pointer to a destination frame.
// Return value: 0 if OK, < 0 otherwise.
int ConvertFromI420(const uint8_t* src_frame, int src_stride,
                    VideoType dst_video_type, int dst_sample_size,
                    int width, int height,
                    uint8_t* dst_frame);
// ConvertFrom YV12.
// Interface - same as above.
int ConvertFromYV12(const uint8_t* src_frame, int src_stride,
                    VideoType dst_video_type, int dst_sample_size,
                    int width, int height,
                    uint8_t* dst_frame);

// The following list describes the designated conversion function which
// are called by the two prior general conversion function.
// Input and output descriptions mostly match the above descriptions, and are
// therefore omitted.
// Possible additional input value - dst_stride - stride of the dst frame.

int ConvertI420ToRGB24(const uint8_t* src_frame,
                       uint8_t* dst_frame,
                       int width, int height);
int ConvertI420ToARGB(const uint8_t* src_frame,
                      uint8_t* dst_frame,
                      int width, int height,
                      int dst_stride);
int ConvertI420ToARGB4444(const uint8_t* src_frame,
                          uint8_t* dst_frame,
                          int width,
                          int height,
                          int dst_stride);
int ConvertI420ToRGB565(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width,
                        int height);
int ConvertI420ToRGB565Android(const uint8_t* src_frame,
                               uint8_t* dst_frame,
                               int width,
                               int height);
int ConvertI420ToARGB1555(const uint8_t* src_frame,
                          uint8_t* dst_frame,
                          int width,
                          int height,
                          int dst_stride);
int ConvertI420ToARGBMac(const uint8_t* src_frame,
                         uint8_t* dst_frame,
                         int width, int height,
                         int dst_stride);
int ConvertI420ToRGBAMac(const uint8_t* src_frame,
                         uint8_t* dst_frame,
                         int width, int height,
                         int dst_stride);
int ConvertI420ToI420(const uint8_t* src_frame,
                      uint8_t* dst_frame,
                      int width, int height,
                      int dst_stride = 0);
int ConvertI420ToUYVY(const uint8_t* src_frame,
                      uint8_t* dst_frame,
                      int width, int height,
                      int dst_stride = 0);
int ConvertI420ToYUY2(const uint8_t* src_frame, uint8_t* dst_frame,
                      int width, int height,
                      int dst_stride = 0);
int ConvertI420ToYV12(const uint8_t* src_frame,
                      uint8_t* dst_frame,
                      int width, int height,
                      int dst_stride);
int ConvertYUY2ToI420(int width, int height,
                      const uint8_t* src_frame,
                      uint8_t* dst_frame);
int ConvertYV12ToI420(const uint8_t* src_frame,
                      int width, int height,
                      uint8_t* dst_frame);
int ConvertRGB24ToARGB(const uint8_t* src_frame,
                       uint8_t* dst_frame,
                       int width, int height,
                       int dst_stride);
int ConvertRGB24ToI420(int width, int height,
                       const uint8_t* src_frame,
                       uint8_t* dst_frame);

int ConvertARGBMacToI420(int width, int height,
                         const uint8_t* src_frame,
                         uint8_t* dst_frame);
int ConvertUYVYToI420(int width, int height,
                      const uint8_t* src_frame,
                      uint8_t* dst_frame);

// NV12 conversion and rotation
int ConvertNV12ToI420(const uint8_t* src_frame,
                      uint8_t* dst_frame,
                      int width, int height);
int ConvertNV12ToI420AndRotate180(const uint8_t* src_frame,
                                  uint8_t* dst_frame, int width,
                                  int height);
int ConvertNV12ToI420AndRotateAntiClockwise(const uint8_t* src_frame,
                                            uint8_t* dst_frame,
                                            int width,
                                            int height);
int ConvertNV12ToI420AndRotateClockwise(const uint8_t* src_frame,
                                        uint8_t* dst_frame,
                                        int width,
                                        int height);
int ConvertNV12ToRGB565(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width, int height);

// NV21 Conversion/Rotation
int ConvertNV21ToI420(const uint8_t* src_frame,
                      uint8_t* dst_frame,
                      int width, int height);
int ConvertNV21ToI420AndRotate180(const uint8_t* src_frame,
                                  uint8_t* dst_frame,
                                  int width, int height);
// TODO (mikhal): Rename to counterClockwise.
int ConvertNV21ToI420AndRotateAntiClockwise(const uint8_t* src_frame,
                                            uint8_t* dst_frame,
                                            int width,
                                            int height);
int ConvertNV21ToI420AndRotateClockwise(const uint8_t* src_frame,
                                        uint8_t* dst_frame,
                                        int width,
                                        int height);

// IPhone
int ConvertI420ToRGBAIPhone(const uint8_t* src_frame,
                            uint8_t* dst_frame,
                            int width, int height,
                            int dst_stride);

int I420Rotate(const uint8_t* src_frame,
               uint8_t* dst_frame,
               int width, int height,
               VideoRotationMode rotation_mode);
// Following three functions:
// Convert from I420/YV12 to I420 and rotate.
// Input:
//    - src_frame       : Pointer to a source frame.
//    - src_width       : Width of source frame in pixels.
//    - src_height      : Height of source frame in pixels.
//    - dst_frame       : Pointer to a destination frame.
//    - dst_width       : Width of destination frame in pixels.
//    - dst_height      : Height of destination frame in pixels.
//    - src_color_space : Input color space.
// Return value: 0 if OK, < 0 otherwise.
int ConvertToI420AndRotateClockwise(const uint8_t* src_frame,
                                    int src_width,
                                    int src_height,
                                    uint8_t* dst_frame,
                                    int dst_width,
                                    int dst_height,
                                    VideoType src_video_type);

int ConvertToI420AndRotateAntiClockwise(const uint8_t* src_frame,
                                        int src_width,
                                        int src_height,
                                        uint8_t* dst_frame,
                                        int dst_width,
                                        int dst_height,
                                        VideoType src_video_type);

int ConvertToI420AndRotate180(const uint8_t* srcBuffer,
                              int srcWidth,
                              int srcHeight,
                              uint8_t* dstBuffer,
                              int dst_width,
                              int dst_height,
                              VideoType src_video_type);

// Mirror functions
// The following 2 functions perform mirroring on a given image
// (LeftRight/UpDown).
// Input:
//    - width       : Image width in pixels.
//    - height      : Image height in pixels.
//    - src_frame   : Pointer to a source frame.
//    - dst_frame   : Pointer to a destination frame.
// Return value: 0 if OK, < 0 otherwise.
int MirrorI420LeftRight(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width, int height);
int MirrorI420UpDown(const uint8_t* src_frame,
                     uint8_t* dst_frame,
                     int width, int height);

// Mirror functions + conversion
// Input:
//    - src_frame       : Pointer to source frame.
//    - dst_frame       : Pointer to destination frame.
//    - src_width       : Width of input buffer.
//    - src_height      : Height of input buffer.
//    - src_color_space : Color space to convert from, I420 if no
//                        conversion should be done.
// Return value: 0 if OK, < 0 otherwise.
int ConvertToI420AndMirrorUpDown(const uint8_t* src_frame,
                                 uint8_t* dst_frame,
                                 int src_width,
                                 int src_height,
                                 VideoType src_video_type);

int ConvertToI420AndRotate(const uint8_t* src_frame,
                           uint8_t* dst_frame,
                           int height,
                           int width,
                           VideoType src_video_type,
                           VideoRotationMode mode);
// Compute PSNR for an I420 frame (all planes).
double I420PSNR(const uint8_t* ref_frame,
                const uint8_t* test_frame,
                int width, int height);
// Compute SSIM for an I420 frame (all planes).
double I420SSIM(const uint8_t* ref_frame,
                const uint8_t* test_frame,
                int width, int height);
}

#endif  // WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_LIBYUV_H_
