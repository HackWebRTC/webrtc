/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/interface/i420_video_frame.h"

#include <string.h>

#include <algorithm>  // swap

#include "webrtc/base/checks.h"

namespace webrtc {

I420VideoFrame::I420VideoFrame() {
  // Intentionally using Reset instead of initializer list so that any missed
  // fields in Reset will be caught by memory checkers.
  Reset();
}

I420VideoFrame::I420VideoFrame(
    const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
    uint32_t timestamp,
    int64_t render_time_ms,
    VideoRotation rotation)
    : video_frame_buffer_(buffer),
      timestamp_(timestamp),
      ntp_time_ms_(0),
      render_time_ms_(render_time_ms),
      rotation_(rotation) {
}

I420VideoFrame::I420VideoFrame(NativeHandle* handle,
                               int width,
                               int height,
                               uint32_t timestamp,
                               int64_t render_time_ms)
    : video_frame_buffer_(
          new rtc::RefCountedObject<TextureBuffer>(handle, width, height)),
      timestamp_(timestamp),
      ntp_time_ms_(0),
      render_time_ms_(render_time_ms),
      rotation_(kVideoRotation_0) {
}

int I420VideoFrame::CreateEmptyFrame(int width, int height,
                                     int stride_y, int stride_u, int stride_v) {
  const int half_width = (width + 1) / 2;
  if (width <= 0 || height <= 0 || stride_y < width || stride_u < half_width ||
      stride_v < half_width) {
    return -1;
  }
  // Creating empty frame - reset all values.
  timestamp_ = 0;
  ntp_time_ms_ = 0;
  render_time_ms_ = 0;
  rotation_ = kVideoRotation_0;

  // Check if it's safe to reuse allocation.
  if (video_frame_buffer_ &&
      video_frame_buffer_->HasOneRef() &&
      !video_frame_buffer_->native_handle() &&
      width == video_frame_buffer_->width() &&
      height == video_frame_buffer_->height() &&
      stride_y == stride(kYPlane) &&
      stride_u == stride(kUPlane) &&
      stride_v == stride(kVPlane)) {
    return 0;
  }

  // Need to allocate new buffer.
  video_frame_buffer_ = new rtc::RefCountedObject<I420Buffer>(
      width, height, stride_y, stride_u, stride_v);
  return 0;
}

int I420VideoFrame::CreateFrame(int size_y, const uint8_t* buffer_y,
                                int size_u, const uint8_t* buffer_u,
                                int size_v, const uint8_t* buffer_v,
                                int width, int height,
                                int stride_y, int stride_u, int stride_v) {
  return CreateFrame(size_y, buffer_y, size_u, buffer_u, size_v, buffer_v,
                     width, height, stride_y, stride_u, stride_v,
                     kVideoRotation_0);
}

int I420VideoFrame::CreateFrame(int size_y,
                                const uint8_t* buffer_y,
                                int size_u,
                                const uint8_t* buffer_u,
                                int size_v,
                                const uint8_t* buffer_v,
                                int width,
                                int height,
                                int stride_y,
                                int stride_u,
                                int stride_v,
                                VideoRotation rotation) {
  const int half_height = (height + 1) / 2;
  const int expected_size_y = height * stride_y;
  const int expected_size_u = half_height * stride_u;
  const int expected_size_v = half_height * stride_v;
  CHECK_GE(size_y, expected_size_y);
  CHECK_GE(size_u, expected_size_u);
  CHECK_GE(size_v, expected_size_v);
  if (CreateEmptyFrame(width, height, stride_y, stride_u, stride_v) < 0)
    return -1;
  memcpy(buffer(kYPlane), buffer_y, expected_size_y);
  memcpy(buffer(kUPlane), buffer_u, expected_size_u);
  memcpy(buffer(kVPlane), buffer_v, expected_size_v);
  rotation_ = rotation;
  return 0;
}

int I420VideoFrame::CopyFrame(const I420VideoFrame& videoFrame) {
  if (videoFrame.native_handle()) {
    video_frame_buffer_ = videoFrame.video_frame_buffer();
  } else {
    int ret = CreateFrame(
        videoFrame.allocated_size(kYPlane), videoFrame.buffer(kYPlane),
        videoFrame.allocated_size(kUPlane), videoFrame.buffer(kUPlane),
        videoFrame.allocated_size(kVPlane), videoFrame.buffer(kVPlane),
        videoFrame.width(), videoFrame.height(), videoFrame.stride(kYPlane),
        videoFrame.stride(kUPlane), videoFrame.stride(kVPlane));
    if (ret < 0)
      return ret;
  }
  timestamp_ = videoFrame.timestamp_;
  ntp_time_ms_ = videoFrame.ntp_time_ms_;
  render_time_ms_ = videoFrame.render_time_ms_;
  rotation_ = videoFrame.rotation_;
  return 0;
}

I420VideoFrame* I420VideoFrame::CloneFrame() const {
  rtc::scoped_ptr<I420VideoFrame> new_frame(new I420VideoFrame());
  if (new_frame->CopyFrame(*this) == -1) {
    // CopyFrame failed.
    return NULL;
  }
  return new_frame.release();
}

void I420VideoFrame::SwapFrame(I420VideoFrame* videoFrame) {
  video_frame_buffer_.swap(videoFrame->video_frame_buffer_);
  std::swap(timestamp_, videoFrame->timestamp_);
  std::swap(ntp_time_ms_, videoFrame->ntp_time_ms_);
  std::swap(render_time_ms_, videoFrame->render_time_ms_);
  std::swap(rotation_, videoFrame->rotation_);
}

void I420VideoFrame::Reset() {
  video_frame_buffer_ = nullptr;
  timestamp_ = 0;
  ntp_time_ms_ = 0;
  render_time_ms_ = 0;
  rotation_ = kVideoRotation_0;
}

uint8_t* I420VideoFrame::buffer(PlaneType type) {
  return video_frame_buffer_ ? video_frame_buffer_->data(type) : nullptr;
}

const uint8_t* I420VideoFrame::buffer(PlaneType type) const {
  // Const cast to call the correct const-version of data.
  const VideoFrameBuffer* const_buffer = video_frame_buffer_.get();
  return const_buffer ? const_buffer->data(type) : nullptr;
}

int I420VideoFrame::allocated_size(PlaneType type) const {
  const int plane_height = (type == kYPlane) ? height() : (height() + 1) / 2;
  return plane_height * stride(type);
}

int I420VideoFrame::stride(PlaneType type) const {
  return video_frame_buffer_ ? video_frame_buffer_->stride(type) : 0;
}

int I420VideoFrame::width() const {
  return video_frame_buffer_ ? video_frame_buffer_->width() : 0;
}

int I420VideoFrame::height() const {
  return video_frame_buffer_ ? video_frame_buffer_->height() : 0;
}

bool I420VideoFrame::IsZeroSize() const {
  return !video_frame_buffer_;
}

void* I420VideoFrame::native_handle() const {
  return video_frame_buffer_ ? video_frame_buffer_->native_handle() : nullptr;
}

rtc::scoped_refptr<VideoFrameBuffer> I420VideoFrame::video_frame_buffer()
    const {
  return video_frame_buffer_;
}

}  // namespace webrtc
