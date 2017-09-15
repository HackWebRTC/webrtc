/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/common_video/include/video_frame_buffer.h"

#include <string.h>

#include <algorithm>

#include "libyuv/convert.h"
#include "libyuv/planar_functions.h"
#include "libyuv/scale.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/keep_ref_until_done.h"

namespace webrtc {

WrappedI420Buffer::WrappedI420Buffer(int width,
                                     int height,
                                     const uint8_t* y_plane,
                                     int y_stride,
                                     const uint8_t* u_plane,
                                     int u_stride,
                                     const uint8_t* v_plane,
                                     int v_stride,
                                     const rtc::Callback0<void>& no_longer_used)
    : width_(width),
      height_(height),
      y_plane_(y_plane),
      u_plane_(u_plane),
      v_plane_(v_plane),
      y_stride_(y_stride),
      u_stride_(u_stride),
      v_stride_(v_stride),
      no_longer_used_cb_(no_longer_used) {
}

WrappedI420Buffer::~WrappedI420Buffer() {
  no_longer_used_cb_();
}

int WrappedI420Buffer::width() const {
  return width_;
}

int WrappedI420Buffer::height() const {
  return height_;
}

const uint8_t* WrappedI420Buffer::DataY() const {
  return y_plane_;
}
const uint8_t* WrappedI420Buffer::DataU() const {
  return u_plane_;
}
const uint8_t* WrappedI420Buffer::DataV() const {
  return v_plane_;
}

int WrappedI420Buffer::StrideY() const {
  return y_stride_;
}
int WrappedI420Buffer::StrideU() const {
  return u_stride_;
}
int WrappedI420Buffer::StrideV() const {
  return v_stride_;
}

// Template to implement a wrapped buffer for a I4??BufferInterface.
template <typename Base>
class WrappedYuvBuffer : public Base {
 public:
  WrappedYuvBuffer(int width,
                   int height,
                   const uint8_t* y_plane,
                   int y_stride,
                   const uint8_t* u_plane,
                   int u_stride,
                   const uint8_t* v_plane,
                   int v_stride,
                   const rtc::Callback0<void>& no_longer_used)
      : width_(width),
        height_(height),
        y_plane_(y_plane),
        u_plane_(u_plane),
        v_plane_(v_plane),
        y_stride_(y_stride),
        u_stride_(u_stride),
        v_stride_(v_stride),
        no_longer_used_cb_(no_longer_used) {}

  int width() const override { return width_; }

  int height() const override { return height_; }

  const uint8_t* DataY() const override { return y_plane_; }

  const uint8_t* DataU() const override { return u_plane_; }

  const uint8_t* DataV() const override { return v_plane_; }

  int StrideY() const override { return y_stride_; }

  int StrideU() const override { return u_stride_; }

  int StrideV() const override { return v_stride_; }

 private:
  friend class rtc::RefCountedObject<WrappedYuvBuffer>;

  ~WrappedYuvBuffer() override { no_longer_used_cb_(); }

  const int width_;
  const int height_;
  const uint8_t* const y_plane_;
  const uint8_t* const u_plane_;
  const uint8_t* const v_plane_;
  const int y_stride_;
  const int u_stride_;
  const int v_stride_;
  rtc::Callback0<void> no_longer_used_cb_;
};

rtc::scoped_refptr<I420BufferInterface> WrapI420Buffer(
    int width,
    int height,
    const uint8_t* y_plane,
    int y_stride,
    const uint8_t* u_plane,
    int u_stride,
    const uint8_t* v_plane,
    int v_stride,
    const rtc::Callback0<void>& no_longer_used) {
  return rtc::scoped_refptr<I420BufferInterface>(
      new rtc::RefCountedObject<WrappedYuvBuffer<I420BufferInterface>>(
          width, height, y_plane, y_stride, u_plane, u_stride, v_plane,
          v_stride, no_longer_used));
}

rtc::scoped_refptr<I444BufferInterface> WrapI444Buffer(
    int width,
    int height,
    const uint8_t* y_plane,
    int y_stride,
    const uint8_t* u_plane,
    int u_stride,
    const uint8_t* v_plane,
    int v_stride,
    const rtc::Callback0<void>& no_longer_used) {
  return rtc::scoped_refptr<I444BufferInterface>(
      new rtc::RefCountedObject<WrappedYuvBuffer<I444BufferInterface>>(
          width, height, y_plane, y_stride, u_plane, u_stride, v_plane,
          v_stride, no_longer_used));
}

rtc::scoped_refptr<PlanarYuvBuffer> WrapYuvBuffer(
    VideoFrameBuffer::Type type,
    int width,
    int height,
    const uint8_t* y_plane,
    int y_stride,
    const uint8_t* u_plane,
    int u_stride,
    const uint8_t* v_plane,
    int v_stride,
    const rtc::Callback0<void>& no_longer_used) {
  switch (type) {
    case VideoFrameBuffer::Type::kI420:
      return WrapI420Buffer(width, height, y_plane, y_stride, u_plane, u_stride,
                            v_plane, v_stride, no_longer_used);
    case VideoFrameBuffer::Type::kI444:
      return WrapI444Buffer(width, height, y_plane, y_stride, u_plane, u_stride,
                            v_plane, v_stride, no_longer_used);
    default:
      FATAL() << "Unexpected frame buffer type.";
      return nullptr;
  }
}

}  // namespace webrtc
