/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_frame_buffer.h"

#include "libyuv/convert.h"
#include "api/video/i420_buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {

rtc::scoped_refptr<I420BufferInterface> VideoFrameBuffer::GetI420() {
  RTC_CHECK(type() == Type::kI420);
  return static_cast<I420BufferInterface*>(this);
}

rtc::scoped_refptr<const I420BufferInterface> VideoFrameBuffer::GetI420()
    const {
  RTC_CHECK(type() == Type::kI420);
  return static_cast<const I420BufferInterface*>(this);
}

I444BufferInterface* VideoFrameBuffer::GetI444() {
  RTC_CHECK(type() == Type::kI444);
  return static_cast<I444BufferInterface*>(this);
}

const I444BufferInterface* VideoFrameBuffer::GetI444() const {
  RTC_CHECK(type() == Type::kI444);
  return static_cast<const I444BufferInterface*>(this);
}

VideoFrameBuffer::Type I420BufferInterface::type() const {
  return Type::kI420;
}

int I420BufferInterface::ChromaWidth() const {
  return (width() + 1) / 2;
}

int I420BufferInterface::ChromaHeight() const {
  return (height() + 1) / 2;
}

rtc::scoped_refptr<I420BufferInterface> I420BufferInterface::ToI420() {
  return this;
}

VideoFrameBuffer::Type I444BufferInterface::type() const {
  return Type::kI444;
}

int I444BufferInterface::ChromaWidth() const {
  return width();
}

int I444BufferInterface::ChromaHeight() const {
  return height();
}

rtc::scoped_refptr<I420BufferInterface> I444BufferInterface::ToI420() {
  rtc::scoped_refptr<I420Buffer> i420_buffer =
      I420Buffer::Create(width(), height());
  libyuv::I444ToI420(DataY(), StrideY(), DataU(), StrideU(), DataV(), StrideV(),
                     i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                     i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                     i420_buffer->MutableDataV(), i420_buffer->StrideV(),
                     width(), height());
  return i420_buffer;
}

}  // namespace webrtc
