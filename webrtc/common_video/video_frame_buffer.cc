/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/interface/video_frame_buffer.h"

#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

namespace webrtc {
namespace {

// Used in rtc::Bind to keep a buffer alive until destructor is called.
static void NoLongerUsedCallback(rtc::scoped_refptr<VideoFrameBuffer> dummy) {}

}  // anonymous namespace

VideoFrameBuffer::~VideoFrameBuffer() {}

I420Buffer::I420Buffer(int width, int height)
    : I420Buffer(width, height, width, (width + 1) / 2, (width + 1) / 2) {
}

I420Buffer::I420Buffer(int width,
                       int height,
                       int stride_y,
                       int stride_u,
                       int stride_v)
    : width_(width),
      height_(height),
      stride_y_(stride_y),
      stride_u_(stride_u),
      stride_v_(stride_v),
      data_(static_cast<uint8_t*>(AlignedMalloc(
          stride_y * height + (stride_u + stride_v) * ((height + 1) / 2),
          kBufferAlignment))) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
  DCHECK_GE(stride_y, width);
  DCHECK_GE(stride_u, (width + 1) / 2);
  DCHECK_GE(stride_v, (width + 1) / 2);
}

I420Buffer::~I420Buffer() {
}

int I420Buffer::width() const {
  return width_;
}

int I420Buffer::height() const {
  return height_;
}

const uint8_t* I420Buffer::data(PlaneType type) const {
  switch (type) {
    case kYPlane:
      return data_.get();
    case kUPlane:
      return data_.get() + stride_y_ * height_;
    case kVPlane:
      return data_.get() + stride_y_ * height_ +
             stride_u_ * ((height_ + 1) / 2);
    default:
      RTC_NOTREACHED();
      return nullptr;
  }
}

uint8_t* I420Buffer::data(PlaneType type) {
  DCHECK(HasOneRef());
  return const_cast<uint8_t*>(
      static_cast<const VideoFrameBuffer*>(this)->data(type));
}

int I420Buffer::stride(PlaneType type) const {
  switch (type) {
    case kYPlane:
      return stride_y_;
    case kUPlane:
      return stride_u_;
    case kVPlane:
      return stride_v_;
    default:
      RTC_NOTREACHED();
      return 0;
  }
}

void* I420Buffer::native_handle() const {
  return nullptr;
}

rtc::scoped_refptr<VideoFrameBuffer> I420Buffer::NativeToI420Buffer() {
  RTC_NOTREACHED();
  return nullptr;
}

NativeHandleBuffer::NativeHandleBuffer(void* native_handle,
                                       int width,
                                       int height)
    : native_handle_(native_handle), width_(width), height_(height) {
  DCHECK(native_handle != nullptr);
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
}

int NativeHandleBuffer::width() const {
  return width_;
}

int NativeHandleBuffer::height() const {
  return height_;
}

const uint8_t* NativeHandleBuffer::data(PlaneType type) const {
  RTC_NOTREACHED();  // Should not be called.
  return nullptr;
}

uint8_t* NativeHandleBuffer::data(PlaneType type) {
  RTC_NOTREACHED();  // Should not be called.
  return nullptr;
}

int NativeHandleBuffer::stride(PlaneType type) const {
  RTC_NOTREACHED();  // Should not be called.
  return 0;
}

void* NativeHandleBuffer::native_handle() const {
  return native_handle_;
}

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

const uint8_t* WrappedI420Buffer::data(PlaneType type) const {
  switch (type) {
    case kYPlane:
      return y_plane_;
    case kUPlane:
      return u_plane_;
    case kVPlane:
      return v_plane_;
    default:
      RTC_NOTREACHED();
      return nullptr;
  }
}

uint8_t* WrappedI420Buffer::data(PlaneType type) {
  RTC_NOTREACHED();
  return nullptr;
}

int WrappedI420Buffer::stride(PlaneType type) const {
  switch (type) {
    case kYPlane:
      return y_stride_;
    case kUPlane:
      return u_stride_;
    case kVPlane:
      return v_stride_;
    default:
      RTC_NOTREACHED();
      return 0;
  }
}

void* WrappedI420Buffer::native_handle() const {
  return nullptr;
}

rtc::scoped_refptr<VideoFrameBuffer> WrappedI420Buffer::NativeToI420Buffer() {
  RTC_NOTREACHED();
  return nullptr;
}

rtc::scoped_refptr<VideoFrameBuffer> ShallowCenterCrop(
    const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
    int cropped_width,
    int cropped_height) {
  CHECK(buffer->native_handle() == nullptr);
  CHECK_LE(cropped_width, buffer->width());
  CHECK_LE(cropped_height, buffer->height());
  if (buffer->width() == cropped_width && buffer->height() == cropped_height)
    return buffer;

  // Center crop to |cropped_width| x |cropped_height|.
  // Make sure offset is even so that u/v plane becomes aligned.
  const int uv_offset_x = (buffer->width() - cropped_width) / 4;
  const int uv_offset_y = (buffer->height() - cropped_height) / 4;
  const int offset_x = uv_offset_x * 2;
  const int offset_y = uv_offset_y * 2;

  // Const cast to call the correct const-version of data().
  const VideoFrameBuffer* const_buffer(buffer.get());
  const uint8_t* y_plane = const_buffer->data(kYPlane) +
                           buffer->stride(kYPlane) * offset_y + offset_x;
  const uint8_t* u_plane = const_buffer->data(kUPlane) +
                           buffer->stride(kUPlane) * uv_offset_y + uv_offset_x;
  const uint8_t* v_plane = const_buffer->data(kVPlane) +
                           buffer->stride(kVPlane) * uv_offset_y + uv_offset_x;
  return new rtc::RefCountedObject<WrappedI420Buffer>(
      cropped_width, cropped_height,
      y_plane, buffer->stride(kYPlane),
      u_plane, buffer->stride(kUPlane),
      v_plane, buffer->stride(kVPlane),
      rtc::Bind(&NoLongerUsedCallback, buffer));
}

}  // namespace webrtc
