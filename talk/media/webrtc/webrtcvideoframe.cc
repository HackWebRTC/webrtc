/*
 * libjingle
 * Copyright 2011 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/media/webrtc/webrtcvideoframe.h"

#include "libyuv/convert.h"
#include "libyuv/convert_from.h"
#include "libyuv/planar_functions.h"
#include "talk/base/logging.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videocommon.h"

namespace cricket {

static const int kWatermarkWidth = 8;
static const int kWatermarkHeight = 8;
static const int kWatermarkOffsetFromLeft = 8;
static const int kWatermarkOffsetFromBottom = 8;
static const unsigned char kWatermarkMaxYValue = 64;

FrameBuffer::FrameBuffer() {}

FrameBuffer::FrameBuffer(size_t length) {
  char* buffer = new char[length];
  SetData(buffer, length);
}

FrameBuffer::~FrameBuffer() {}

void FrameBuffer::SetData(char* data, size_t length) {
  uint8_t* new_memory = reinterpret_cast<uint8_t*>(data);
  uint32_t new_length = static_cast<uint32_t>(length);
  uint32_t new_size = static_cast<uint32_t>(length);
  video_frame_.Swap(new_memory, new_length, new_size);
}

void FrameBuffer::ReturnData(char** data, size_t* length) {
  *data = NULL;
  uint32_t old_length = 0;
  uint32_t old_size = 0;
  video_frame_.Swap(reinterpret_cast<uint8_t*&>(*data),
                    old_length, old_size);
  *length = old_length;
}

char* FrameBuffer::data() {
  return reinterpret_cast<char*>(video_frame_.Buffer());
}

size_t FrameBuffer::length() const {
  return static_cast<size_t>(video_frame_.Length());
}

webrtc::VideoFrame* FrameBuffer::frame() { return &video_frame_; }

const webrtc::VideoFrame* FrameBuffer::frame() const { return &video_frame_; }

WebRtcVideoFrame::WebRtcVideoFrame()
    : video_buffer_(new RefCountedBuffer()), is_black_(false) {}

WebRtcVideoFrame::~WebRtcVideoFrame() {}

bool WebRtcVideoFrame::Init(
    uint32 format, int w, int h, int dw, int dh, uint8* sample,
    size_t sample_size, size_t pixel_width, size_t pixel_height,
    int64 elapsed_time, int64 time_stamp, int rotation) {
  return Reset(format, w, h, dw, dh, sample, sample_size, pixel_width,
               pixel_height, elapsed_time, time_stamp, rotation);
}

bool WebRtcVideoFrame::Init(const CapturedFrame* frame, int dw, int dh) {
  return Reset(frame->fourcc, frame->width, frame->height, dw, dh,
               static_cast<uint8*>(frame->data), frame->data_size,
               frame->pixel_width, frame->pixel_height, frame->elapsed_time,
               frame->time_stamp, frame->rotation);
}

bool WebRtcVideoFrame::InitToBlack(int w, int h, size_t pixel_width,
                                   size_t pixel_height, int64 elapsed_time,
                                   int64 time_stamp) {
  InitToEmptyBuffer(w, h, pixel_width, pixel_height, elapsed_time, time_stamp);
  if (!is_black_) {
    return SetToBlack();
  }
  return true;
}

void WebRtcVideoFrame::Attach(
    uint8* buffer, size_t buffer_size, int w, int h, size_t pixel_width,
    size_t pixel_height, int64 elapsed_time, int64 time_stamp, int rotation) {
  talk_base::scoped_refptr<RefCountedBuffer> video_buffer(
      new RefCountedBuffer());
  video_buffer->SetData(reinterpret_cast<char*>(buffer), buffer_size);
  Attach(video_buffer.get(), buffer_size, w, h, pixel_width, pixel_height,
         elapsed_time, time_stamp, rotation);
}

void WebRtcVideoFrame::Detach(uint8** data, size_t* length) {
  video_buffer_->ReturnData(reinterpret_cast<char**>(data), length);
}

size_t WebRtcVideoFrame::GetWidth() const { return frame()->Width(); }

size_t WebRtcVideoFrame::GetHeight() const { return frame()->Height(); }

const uint8* WebRtcVideoFrame::GetYPlane() const {
  uint8_t* buffer = frame()->Buffer();
  return buffer;
}

const uint8* WebRtcVideoFrame::GetUPlane() const {
  uint8_t* buffer = frame()->Buffer();
  if (buffer) {
    buffer += (frame()->Width() * frame()->Height());
  }
  return buffer;
}

const uint8* WebRtcVideoFrame::GetVPlane() const {
  uint8_t* buffer = frame()->Buffer();
  if (buffer) {
    int uv_size = static_cast<int>(GetChromaSize());
    buffer += frame()->Width() * frame()->Height() + uv_size;
  }
  return buffer;
}

uint8* WebRtcVideoFrame::GetYPlane() {
  uint8_t* buffer = frame()->Buffer();
  return buffer;
}

uint8* WebRtcVideoFrame::GetUPlane() {
  uint8_t* buffer = frame()->Buffer();
  if (buffer) {
    buffer += (frame()->Width() * frame()->Height());
  }
  return buffer;
}

uint8* WebRtcVideoFrame::GetVPlane() {
  uint8_t* buffer = frame()->Buffer();
  if (buffer) {
    int uv_size = static_cast<int>(GetChromaSize());
    buffer += frame()->Width() * frame()->Height() + uv_size;
  }
  return buffer;
}

VideoFrame* WebRtcVideoFrame::Copy() const {
  const char* old_buffer = video_buffer_->data();
  if (!old_buffer)
    return NULL;
  size_t new_buffer_size = video_buffer_->length();

  WebRtcVideoFrame* ret_val = new WebRtcVideoFrame();
  ret_val->Attach(video_buffer_.get(), new_buffer_size, frame()->Width(),
                  frame()->Height(), pixel_width_, pixel_height_, elapsed_time_,
                  time_stamp_, rotation_);
  return ret_val;
}

bool WebRtcVideoFrame::MakeExclusive() {
  const int length = static_cast<int>(video_buffer_->length());
  RefCountedBuffer* exclusive_buffer = new RefCountedBuffer(length);
  memcpy(exclusive_buffer->data(), video_buffer_->data(), length);
  Attach(exclusive_buffer, length, frame()->Width(), frame()->Height(),
         pixel_width_, pixel_height_, elapsed_time_, time_stamp_, rotation_);
  return true;
}

size_t WebRtcVideoFrame::CopyToBuffer(uint8* buffer, size_t size) const {
  if (!frame()->Buffer()) {
    return 0;
  }

  size_t needed = frame()->Length();
  if (needed <= size) {
    memcpy(buffer, frame()->Buffer(), needed);
  }
  return needed;
}

// TODO(fbarchard): Refactor into base class and share with lmi
size_t WebRtcVideoFrame::ConvertToRgbBuffer(uint32 to_fourcc, uint8* buffer,
                                            size_t size, int stride_rgb) const {
  if (!frame()->Buffer()) {
    return 0;
  }
  size_t width = frame()->Width();
  size_t height = frame()->Height();
  size_t needed = (stride_rgb >= 0 ? stride_rgb : -stride_rgb) * height;
  if (size < needed) {
    LOG(LS_WARNING) << "RGB buffer is not large enough";
    return needed;
  }

  if (libyuv::ConvertFromI420(GetYPlane(), GetYPitch(), GetUPlane(),
                              GetUPitch(), GetVPlane(), GetVPitch(), buffer,
                              stride_rgb,
                              static_cast<int>(width),
                              static_cast<int>(height),
                              to_fourcc)) {
    LOG(LS_WARNING) << "RGB type not supported: " << to_fourcc;
    return 0;  // 0 indicates error
  }
  return needed;
}

void WebRtcVideoFrame::Attach(
    RefCountedBuffer* video_buffer, size_t buffer_size, int w, int h,
    size_t pixel_width, size_t pixel_height, int64 elapsed_time,
    int64 time_stamp, int rotation) {
  if (video_buffer_.get() == video_buffer) {
    return;
  }
  is_black_ = false;
  video_buffer_ = video_buffer;
  frame()->SetWidth(w);
  frame()->SetHeight(h);
  pixel_width_ = pixel_width;
  pixel_height_ = pixel_height;
  elapsed_time_ = elapsed_time;
  time_stamp_ = time_stamp;
  rotation_ = rotation;
}

// Add a square watermark near the left-low corner. clamp Y.
// Returns false on error.
bool WebRtcVideoFrame::AddWatermark() {
  size_t w = GetWidth();
  size_t h = GetHeight();

  if (w < kWatermarkWidth + kWatermarkOffsetFromLeft ||
      h < kWatermarkHeight + kWatermarkOffsetFromBottom) {
    return false;
  }

  uint8* buffer = GetYPlane();
  for (size_t x = kWatermarkOffsetFromLeft;
       x < kWatermarkOffsetFromLeft + kWatermarkWidth; ++x) {
    for (size_t y = h - kWatermarkOffsetFromBottom - kWatermarkHeight;
         y < h - kWatermarkOffsetFromBottom; ++y) {
      buffer[y * w + x] =
          talk_base::_min(buffer[y * w + x], kWatermarkMaxYValue);
    }
  }
  return true;
}

bool WebRtcVideoFrame::Reset(
    uint32 format, int w, int h, int dw, int dh, uint8* sample,
    size_t sample_size, size_t pixel_width, size_t pixel_height,
    int64 elapsed_time, int64 time_stamp, int rotation) {
  if (!Validate(format, w, h, sample, sample_size)) {
    return false;
  }
  // Translate aliases to standard enums (e.g., IYUV -> I420).
  format = CanonicalFourCC(format);

  // Round display width and height down to multiple of 4, to avoid webrtc
  // size calculation error on odd sizes.
  // TODO(Ronghua): Remove this once the webrtc allocator is fixed.
  dw = (dw > 4) ? (dw & ~3) : dw;
  dh = (dh > 4) ? (dh & ~3) : dh;

  // Set up a new buffer.
  // TODO(fbarchard): Support lazy allocation.
  int new_width = dw;
  int new_height = dh;
  if (rotation == 90 || rotation == 270) {  // If rotated swap width, height.
    new_width = dh;
    new_height = dw;
  }

  size_t desired_size = SizeOf(new_width, new_height);
  talk_base::scoped_refptr<RefCountedBuffer> video_buffer(
      new RefCountedBuffer(desired_size));
  // Since the libyuv::ConvertToI420 will handle the rotation, so the
  // new frame's rotation should always be 0.
  Attach(video_buffer.get(), desired_size, new_width, new_height, pixel_width,
         pixel_height, elapsed_time, time_stamp, 0);

  int horiz_crop = ((w - dw) / 2) & ~1;
  // ARGB on Windows has negative height.
  // The sample's layout in memory is normal, so just correct crop.
  int vert_crop = ((abs(h) - dh) / 2) & ~1;
  // Conversion functions expect negative height to flip the image.
  int idh = (h < 0) ? -dh : dh;
  uint8* y = GetYPlane();
  int y_stride = GetYPitch();
  uint8* u = GetUPlane();
  int u_stride = GetUPitch();
  uint8* v = GetVPlane();
  int v_stride = GetVPitch();
  int r = libyuv::ConvertToI420(
      sample, sample_size, y, y_stride, u, u_stride, v, v_stride, horiz_crop,
      vert_crop, w, h, dw, idh, static_cast<libyuv::RotationMode>(rotation),
      format);
  if (r) {
    LOG(LS_ERROR) << "Error parsing format: " << GetFourccName(format)
                  << " return code : " << r;
    return false;
  }
  return true;
}

VideoFrame* WebRtcVideoFrame::CreateEmptyFrame(
    int w, int h, size_t pixel_width, size_t pixel_height, int64 elapsed_time,
    int64 time_stamp) const {
  WebRtcVideoFrame* frame = new WebRtcVideoFrame();
  frame->InitToEmptyBuffer(w, h, pixel_width, pixel_height, elapsed_time,
                           time_stamp);
  return frame;
}

void WebRtcVideoFrame::InitToEmptyBuffer(int w, int h, size_t pixel_width,
                                         size_t pixel_height,
                                         int64 elapsed_time, int64 time_stamp) {
  size_t buffer_size = VideoFrame::SizeOf(w, h);
  talk_base::scoped_refptr<RefCountedBuffer> video_buffer(
      new RefCountedBuffer(buffer_size));
  Attach(video_buffer.get(), buffer_size, w, h, pixel_width, pixel_height,
         elapsed_time, time_stamp, 0);
}

}  // namespace cricket
