/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/libyuv/include/scaler.h"

#include "libyuv.h"

namespace webrtc {

Scaler::Scaler()
    : method_(kScaleBox),
      src_width_(0),
      src_height_(0),
      dst_width_(0),
      dst_height_(0),
      set_(false) {}

Scaler::~Scaler() {}

int Scaler::Set(int src_width, int src_height,
                int dst_width, int dst_height,
                VideoType src_video_type, VideoType dst_video_type,
                ScaleMethod method) {
  set_ = false;
  if (src_width < 1 || src_height < 1 || dst_width < 1 || dst_height < 1)
    return -1;

  if (!SupportedVideoType(src_video_type, dst_video_type))
    return -1;

  src_width_ = src_width;
  src_height_ = src_height;
  dst_width_ = dst_width;
  dst_height_ = dst_height;
  method_ = method;
  set_ = true;
  return 0;
}

int Scaler::Scale(const VideoFrame& src_frame,
                  VideoFrame* dst_frame) {
  assert(dst_frame);
  if (src_frame.Buffer() == NULL || src_frame.Length() == 0)
    return -1;
  if (!set_)
    return -2;

  // Making sure that destination frame is of sufficient size.
  int required_dst_size = CalcBufferSize(kI420, dst_width_, dst_height_);
  dst_frame->VerifyAndAllocate(required_dst_size);
  // Set destination length and dimensions.
  dst_frame->SetLength(required_dst_size);
  dst_frame->SetWidth(dst_width_);
  dst_frame->SetHeight(dst_height_);

  int src_half_width = (src_width_ + 1) >> 1;
  int src_half_height = (src_height_ + 1) >> 1;
  int dst_half_width = (dst_width_ + 1) >> 1;
  int dst_half_height = (dst_height_ + 1) >> 1;
  // Converting to planes:
  const uint8_t* src_yplane = src_frame.Buffer();
  const uint8_t* src_uplane = src_yplane + src_width_ * src_height_;
  const uint8_t* src_vplane = src_uplane + src_half_width * src_half_height;

  uint8_t* dst_yplane = dst_frame->Buffer();
  uint8_t* dst_uplane = dst_yplane + dst_width_ * dst_height_;
  uint8_t* dst_vplane = dst_uplane + dst_half_width * dst_half_height;

  return libyuv::I420Scale(src_yplane, src_width_,
                           src_uplane, src_half_width,
                           src_vplane, src_half_width,
                           src_width_, src_height_,
                           dst_yplane, dst_width_,
                           dst_uplane, dst_half_width,
                           dst_vplane, dst_half_width,
                           dst_width_, dst_height_,
                           libyuv::FilterMode(method_));
}


// TODO(mikhal): Add test to new function. Currently not used.
int Scaler::Scale(const I420VideoFrame& src_frame,
                  I420VideoFrame* dst_frame) {
  assert(dst_frame);
  // TODO(mikhal): Add isEmpty
  //  if (src_frame.Buffer() == NULL || src_frame.Length() == 0)
  //    return -1;
  if (!set_)
    return -2;

  // TODO(mikhal): Setting stride equal to width - should align.
  dst_frame->CreateEmptyFrame(dst_width_, dst_height_, dst_width_ ,
                              (dst_width_ + 1) / 2, (dst_width_ + 1) / 2);

  return libyuv::I420Scale(src_frame.buffer(kYPlane), src_frame.stride(kYPlane),
                           src_frame.buffer(kUPlane), src_frame.stride(kYPlane),
                           src_frame.buffer(kVPlane), src_frame.stride(kYPlane),
                           src_width_, src_height_,
                           dst_frame->buffer(kYPlane),
                           dst_frame->stride(kYPlane),
                           dst_frame->buffer(kYPlane),
                           dst_frame->stride(kYPlane),
                           dst_frame->buffer(kYPlane),
                           dst_frame->stride(kYPlane),
                           dst_width_, dst_height_,
                           libyuv::FilterMode(method_));
}

// TODO(mikhal): Add support for more types.
bool Scaler::SupportedVideoType(VideoType src_video_type,
                                VideoType dst_video_type) {
  if (src_video_type != dst_video_type)
    return false;

  if ((src_video_type == kI420) || (src_video_type == kIYUV) ||
      (src_video_type == kYV12))
    return true;

  return false;
}

}  // namespace webrtc
