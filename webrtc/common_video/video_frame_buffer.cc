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

#include "webrtc/base/checks.h"
#include "webrtc/base/keep_ref_until_done.h"
#include "libyuv/convert.h"

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

namespace webrtc {

namespace {

int I420DataSize(int height, int stride_y, int stride_u, int stride_v) {
  return stride_y * height + (stride_u + stride_v) * ((height + 1) / 2);
}

}  // namespace

const uint8_t* VideoFrameBuffer::data(PlaneType type) const {
  switch (type) {
    case kYPlane:
      return DataY();
    case kUPlane:
      return DataU();
    case kVPlane:
      return DataV();
    default:
      RTC_NOTREACHED();
      return nullptr;
  }
}

const uint8_t* VideoFrameBuffer::DataY() const {
  return data(kYPlane);
}
const uint8_t* VideoFrameBuffer::DataU() const {
  return data(kUPlane);
}
const uint8_t* VideoFrameBuffer::DataV() const {
  return data(kVPlane);
}

int VideoFrameBuffer::stride(PlaneType type) const {
  switch (type) {
    case kYPlane:
      return StrideY();
    case kUPlane:
      return StrideU();
    case kVPlane:
      return StrideV();
    default:
      RTC_NOTREACHED();
      return 0;
  }
}

int VideoFrameBuffer::StrideY() const {
  return stride(kYPlane);
}
int VideoFrameBuffer::StrideU() const {
  return stride(kUPlane);
}
int VideoFrameBuffer::StrideV() const {
  return stride(kVPlane);
}

uint8_t* VideoFrameBuffer::MutableDataY() {
  RTC_NOTREACHED();
  return nullptr;
}
uint8_t* VideoFrameBuffer::MutableDataU() {
  RTC_NOTREACHED();
  return nullptr;
}
uint8_t* VideoFrameBuffer::MutableDataV() {
  RTC_NOTREACHED();
  return nullptr;
}

uint8_t* VideoFrameBuffer::MutableData(PlaneType type) {
  switch (type) {
    case kYPlane:
      return MutableDataY();
    case kUPlane:
      return MutableDataU();
    case kVPlane:
      return MutableDataV();
    default:
      RTC_NOTREACHED();
      return nullptr;
  }
}

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
          I420DataSize(height, stride_y, stride_u, stride_v),
          kBufferAlignment))) {
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
  RTC_DCHECK_GE(stride_y, width);
  RTC_DCHECK_GE(stride_u, (width + 1) / 2);
  RTC_DCHECK_GE(stride_v, (width + 1) / 2);
}

I420Buffer::~I420Buffer() {
}

void I420Buffer::InitializeData() {
  memset(data_.get(), 0,
         I420DataSize(height_, stride_y_, stride_u_, stride_v_));
}

int I420Buffer::width() const {
  return width_;
}

int I420Buffer::height() const {
  return height_;
}

const uint8_t* I420Buffer::DataY() const {
  return data_.get();
}
const uint8_t* I420Buffer::DataU() const {
  return data_.get() + stride_y_ * height_;
}
const uint8_t* I420Buffer::DataV() const {
  return data_.get() + stride_y_ * height_ + stride_u_ * ((height_ + 1) / 2);
}

bool I420Buffer::IsMutable() {
  return HasOneRef();
}

uint8_t* I420Buffer::MutableDataY() {
  RTC_DCHECK(IsMutable());
  return const_cast<uint8_t*>(DataY());
}
uint8_t* I420Buffer::MutableDataU() {
  RTC_DCHECK(IsMutable());
  return const_cast<uint8_t*>(DataU());
}
uint8_t* I420Buffer::MutableDataV() {
  RTC_DCHECK(IsMutable());
  return const_cast<uint8_t*>(DataV());
}

int I420Buffer::StrideY() const {
  return stride_y_;
}
int I420Buffer::StrideU() const {
  return stride_u_;
}
int I420Buffer::StrideV() const {
  return stride_v_;
}

void* I420Buffer::native_handle() const {
  return nullptr;
}

rtc::scoped_refptr<VideoFrameBuffer> I420Buffer::NativeToI420Buffer() {
  RTC_NOTREACHED();
  return nullptr;
}

rtc::scoped_refptr<I420Buffer> I420Buffer::Copy(
    const rtc::scoped_refptr<VideoFrameBuffer>& buffer) {
  int width = buffer->width();
  int height = buffer->height();
  rtc::scoped_refptr<I420Buffer> copy =
      new rtc::RefCountedObject<I420Buffer>(width, height);
  RTC_CHECK(libyuv::I420Copy(buffer->DataY(), buffer->StrideY(),
                             buffer->DataU(), buffer->StrideU(),
                             buffer->DataV(), buffer->StrideV(),
                             copy->MutableDataY(), copy->StrideY(),
                             copy->MutableDataU(), copy->StrideU(),
                             copy->MutableDataV(), copy->StrideV(),
                             width, height) == 0);

  return copy;
}

NativeHandleBuffer::NativeHandleBuffer(void* native_handle,
                                       int width,
                                       int height)
    : native_handle_(native_handle), width_(width), height_(height) {
  RTC_DCHECK(native_handle != nullptr);
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
}

bool NativeHandleBuffer::IsMutable() {
  return false;
}

int NativeHandleBuffer::width() const {
  return width_;
}

int NativeHandleBuffer::height() const {
  return height_;
}

const uint8_t* NativeHandleBuffer::DataY() const {
  RTC_NOTREACHED();  // Should not be called.
  return nullptr;
}
const uint8_t* NativeHandleBuffer::DataU() const {
  RTC_NOTREACHED();  // Should not be called.
  return nullptr;
}
const uint8_t* NativeHandleBuffer::DataV() const {
  RTC_NOTREACHED();  // Should not be called.
  return nullptr;
}

int NativeHandleBuffer::StrideY() const {
  RTC_NOTREACHED();  // Should not be called.
  return 0;
}
int NativeHandleBuffer::StrideU() const {
  RTC_NOTREACHED();  // Should not be called.
  return 0;
}
int NativeHandleBuffer::StrideV() const {
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

// Data owned by creator; never mutable.
bool WrappedI420Buffer::IsMutable() {
  return false;
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
  RTC_CHECK(buffer->native_handle() == nullptr);
  RTC_CHECK_LE(cropped_width, buffer->width());
  RTC_CHECK_LE(cropped_height, buffer->height());
  if (buffer->width() == cropped_width && buffer->height() == cropped_height)
    return buffer;

  // Center crop to |cropped_width| x |cropped_height|.
  // Make sure offset is even so that u/v plane becomes aligned.
  const int uv_offset_x = (buffer->width() - cropped_width) / 4;
  const int uv_offset_y = (buffer->height() - cropped_height) / 4;
  const int offset_x = uv_offset_x * 2;
  const int offset_y = uv_offset_y * 2;

  const uint8_t* y_plane = buffer->DataY() +
                           buffer->StrideY() * offset_y + offset_x;
  const uint8_t* u_plane = buffer->DataU() +
                           buffer->StrideU() * uv_offset_y + uv_offset_x;
  const uint8_t* v_plane = buffer->DataV() +
                           buffer->StrideV() * uv_offset_y + uv_offset_x;
  return new rtc::RefCountedObject<WrappedI420Buffer>(
      cropped_width, cropped_height,
      y_plane, buffer->StrideY(),
      u_plane, buffer->StrideU(),
      v_plane, buffer->StrideV(),
      rtc::KeepRefUntilDone(buffer));
}

}  // namespace webrtc
