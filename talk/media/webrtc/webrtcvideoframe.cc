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
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videocommon.h"
#include "talk/media/webrtc/webrtctexturevideoframe.h"
#include "webrtc/base/logging.h"
#include "webrtc/video_frame.h"

#define UNIMPLEMENTED                                                 \
  LOG(LS_ERROR) << "Call to unimplemented function " << __FUNCTION__; \
  ASSERT(false)

namespace cricket {

// Class that wraps ownerhip semantics of a buffer passed to it.
// * Buffers passed using Attach() become owned by this FrameBuffer and will be
//   destroyed on FrameBuffer destruction.
// * Buffers passed using Alias() are not owned and will not be destroyed on
//   FrameBuffer destruction,  The buffer then must outlive the FrameBuffer.
class WebRtcVideoFrame::FrameBuffer {
 public:
  FrameBuffer();
  explicit FrameBuffer(size_t length);
  ~FrameBuffer();

  void Attach(uint8* data, size_t length);
  void Alias(uint8* data, size_t length);
  uint8* data();
  size_t length() const;

  webrtc::VideoFrame* frame();
  const webrtc::VideoFrame* frame() const;

 private:
  rtc::scoped_ptr<uint8[]> owned_data_;
  webrtc::VideoFrame video_frame_;
};

WebRtcVideoFrame::FrameBuffer::FrameBuffer() {}

WebRtcVideoFrame::FrameBuffer::FrameBuffer(size_t length) {
  uint8* buffer = new uint8[length];
  Attach(buffer, length);
}

WebRtcVideoFrame::FrameBuffer::~FrameBuffer() {
  // Make sure that |video_frame_| doesn't delete the buffer, as |owned_data_|
  // will release the buffer if this FrameBuffer owns it.
  uint8_t* new_memory = NULL;
  size_t new_length = 0;
  size_t new_size = 0;
  video_frame_.Swap(new_memory, new_length, new_size);
}

void WebRtcVideoFrame::FrameBuffer::Attach(uint8* data, size_t length) {
  Alias(data, length);
  owned_data_.reset(data);
}

void WebRtcVideoFrame::FrameBuffer::Alias(uint8* data, size_t length) {
  owned_data_.reset();
  uint8_t* new_memory = reinterpret_cast<uint8_t*>(data);
  size_t new_length = length;
  size_t new_size = length;
  video_frame_.Swap(new_memory, new_length, new_size);
}

uint8* WebRtcVideoFrame::FrameBuffer::data() {
  return video_frame_.Buffer();
}

size_t WebRtcVideoFrame::FrameBuffer::length() const {
  return video_frame_.Length();
}

webrtc::VideoFrame* WebRtcVideoFrame::FrameBuffer::frame() {
  return &video_frame_;
}

const webrtc::VideoFrame* WebRtcVideoFrame::FrameBuffer::frame() const {
  return &video_frame_;
}

WebRtcVideoFrame::WebRtcVideoFrame()
    : video_buffer_(new RefCountedBuffer()) {}

WebRtcVideoFrame::~WebRtcVideoFrame() {}

bool WebRtcVideoFrame::Init(uint32 format,
                            int w,
                            int h,
                            int dw,
                            int dh,
                            uint8* sample,
                            size_t sample_size,
                            size_t pixel_width,
                            size_t pixel_height,
                            int64_t elapsed_time,
                            int64_t time_stamp,
                            webrtc::VideoRotation rotation) {
  return Reset(format, w, h, dw, dh, sample, sample_size, pixel_width,
               pixel_height, elapsed_time, time_stamp, rotation);
}

bool WebRtcVideoFrame::Init(const CapturedFrame* frame, int dw, int dh) {
  return Reset(frame->fourcc, frame->width, frame->height, dw, dh,
               static_cast<uint8*>(frame->data), frame->data_size,
               frame->pixel_width, frame->pixel_height, frame->elapsed_time,
               frame->time_stamp, frame->GetRotation());
}

bool WebRtcVideoFrame::Alias(const CapturedFrame* frame,
                             int dw,
                             int dh,
                             bool apply_rotation) {
  if (CanonicalFourCC(frame->fourcc) != FOURCC_I420 ||
      (apply_rotation &&
       frame->GetRotation() != webrtc::kVideoRotation_0) ||
      frame->width != dw || frame->height != dh) {
    // TODO(fbarchard): Enable aliasing of more formats.
    return Init(frame, dw, dh);
  } else {
    Alias(static_cast<uint8*>(frame->data), frame->data_size, frame->width,
          frame->height, frame->pixel_width, frame->pixel_height,
          frame->elapsed_time, frame->time_stamp, frame->GetRotation());
    return true;
  }
}

bool WebRtcVideoFrame::InitToBlack(int w, int h, size_t pixel_width,
                                   size_t pixel_height, int64_t elapsed_time,
                                   int64_t time_stamp) {
  InitToEmptyBuffer(w, h, pixel_width, pixel_height, elapsed_time, time_stamp);
  return SetToBlack();
}

void WebRtcVideoFrame::Alias(uint8* buffer,
                             size_t buffer_size,
                             int w,
                             int h,
                             size_t pixel_width,
                             size_t pixel_height,
                             int64_t elapsed_time,
                             int64_t time_stamp,
                             webrtc::VideoRotation rotation) {
  rtc::scoped_refptr<RefCountedBuffer> video_buffer(
      new RefCountedBuffer());
  video_buffer->Alias(buffer, buffer_size);
  Attach(video_buffer.get(), buffer_size, w, h, pixel_width, pixel_height,
         elapsed_time, time_stamp, rotation);
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
  uint8* old_buffer = video_buffer_->data();
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
  const size_t length = video_buffer_->length();
  RefCountedBuffer* exclusive_buffer = new RefCountedBuffer(length);
  memcpy(exclusive_buffer->data(), video_buffer_->data(), length);
  Attach(exclusive_buffer, length, frame()->Width(), frame()->Height(),
         pixel_width_, pixel_height_, elapsed_time_, time_stamp_, rotation_);
  return true;
}

size_t WebRtcVideoFrame::ConvertToRgbBuffer(uint32 to_fourcc, uint8* buffer,
                                            size_t size, int stride_rgb) const {
  if (!frame()->Buffer()) {
    return 0;
  }
  return VideoFrame::ConvertToRgbBuffer(to_fourcc, buffer, size, stride_rgb);
}

void WebRtcVideoFrame::Attach(RefCountedBuffer* video_buffer,
                              size_t buffer_size,
                              int w,
                              int h,
                              size_t pixel_width,
                              size_t pixel_height,
                              int64_t elapsed_time,
                              int64_t time_stamp,
                              webrtc::VideoRotation rotation) {
  if (video_buffer_.get() == video_buffer) {
    return;
  }
  video_buffer_ = video_buffer;
  frame()->SetWidth(w);
  frame()->SetHeight(h);
  pixel_width_ = pixel_width;
  pixel_height_ = pixel_height;
  elapsed_time_ = elapsed_time;
  time_stamp_ = time_stamp;
  rotation_ = rotation;
}

webrtc::VideoFrame* WebRtcVideoFrame::frame() {
  return video_buffer_->frame();
}

const webrtc::VideoFrame* WebRtcVideoFrame::frame() const {
  return video_buffer_->frame();
}

bool WebRtcVideoFrame::Reset(uint32 format,
                             int w,
                             int h,
                             int dw,
                             int dh,
                             uint8* sample,
                             size_t sample_size,
                             size_t pixel_width,
                             size_t pixel_height,
                             int64_t elapsed_time,
                             int64_t time_stamp,
                             webrtc::VideoRotation rotation) {
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
  rtc::scoped_refptr<RefCountedBuffer> video_buffer(
      new RefCountedBuffer(desired_size));
  // Since the libyuv::ConvertToI420 will handle the rotation, so the
  // new frame's rotation should always be 0.
  Attach(video_buffer.get(), desired_size, new_width, new_height, pixel_width,
         pixel_height, elapsed_time, time_stamp, webrtc::kVideoRotation_0);

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
    int w, int h, size_t pixel_width, size_t pixel_height, int64_t elapsed_time,
    int64_t time_stamp) const {
  WebRtcVideoFrame* frame = new WebRtcVideoFrame();
  frame->InitToEmptyBuffer(w, h, pixel_width, pixel_height, elapsed_time,
                           time_stamp);
  return frame;
}

void WebRtcVideoFrame::InitToEmptyBuffer(int w, int h, size_t pixel_width,
                                         size_t pixel_height,
                                         int64_t elapsed_time,
                                         int64_t time_stamp) {
  size_t buffer_size = VideoFrame::SizeOf(w, h);
  rtc::scoped_refptr<RefCountedBuffer> video_buffer(
      new RefCountedBuffer(buffer_size));
  Attach(video_buffer.get(), buffer_size, w, h, pixel_width, pixel_height,
         elapsed_time, time_stamp, webrtc::kVideoRotation_0);
}

WebRtcVideoRenderFrame::WebRtcVideoRenderFrame(
    const webrtc::I420VideoFrame* frame,
    int64_t elapsed_time_ms)
    : frame_(frame), elapsed_time_ms_(elapsed_time_ms) {
}

bool WebRtcVideoRenderFrame::InitToBlack(int w,
                                         int h,
                                         size_t pixel_width,
                                         size_t pixel_height,
                                         int64_t elapsed_time,
                                         int64_t time_stamp) {
  UNIMPLEMENTED;
  return false;
}

bool WebRtcVideoRenderFrame::Reset(uint32 fourcc,
                                   int w,
                                   int h,
                                   int dw,
                                   int dh,
                                   uint8* sample,
                                   size_t sample_size,
                                   size_t pixel_width,
                                   size_t pixel_height,
                                   int64_t elapsed_time,
                                   int64_t time_stamp,
                                   webrtc::VideoRotation rotation) {
  UNIMPLEMENTED;
  return false;
}

size_t WebRtcVideoRenderFrame::GetWidth() const {
  return static_cast<size_t>(frame_->width());
}
size_t WebRtcVideoRenderFrame::GetHeight() const {
  return static_cast<size_t>(frame_->height());
}

const uint8* WebRtcVideoRenderFrame::GetYPlane() const {
  return frame_->buffer(webrtc::kYPlane);
}
const uint8* WebRtcVideoRenderFrame::GetUPlane() const {
  return frame_->buffer(webrtc::kUPlane);
}
const uint8* WebRtcVideoRenderFrame::GetVPlane() const {
  return frame_->buffer(webrtc::kVPlane);
}

uint8* WebRtcVideoRenderFrame::GetYPlane() {
  UNIMPLEMENTED;
  return NULL;
}
uint8* WebRtcVideoRenderFrame::GetUPlane() {
  UNIMPLEMENTED;
  return NULL;
}
uint8* WebRtcVideoRenderFrame::GetVPlane() {
  UNIMPLEMENTED;
  return NULL;
}

int32 WebRtcVideoRenderFrame::GetYPitch() const {
  return frame_->stride(webrtc::kYPlane);
}
int32 WebRtcVideoRenderFrame::GetUPitch() const {
  return frame_->stride(webrtc::kUPlane);
}
int32 WebRtcVideoRenderFrame::GetVPitch() const {
  return frame_->stride(webrtc::kVPlane);
}

void* WebRtcVideoRenderFrame::GetNativeHandle() const {
  return frame_->native_handle();
}

size_t WebRtcVideoRenderFrame::GetPixelWidth() const {
  return 1;
}
size_t WebRtcVideoRenderFrame::GetPixelHeight() const {
  return 1;
}

int64_t WebRtcVideoRenderFrame::GetElapsedTime() const {
  return elapsed_time_ms_ * rtc::kNumNanosecsPerMillisec;
}
int64_t WebRtcVideoRenderFrame::GetTimeStamp() const {
  return frame_->render_time_ms() * rtc::kNumNanosecsPerMillisec;
}
void WebRtcVideoRenderFrame::SetElapsedTime(int64_t elapsed_time) {
  UNIMPLEMENTED;
}
void WebRtcVideoRenderFrame::SetTimeStamp(int64_t time_stamp) {
  UNIMPLEMENTED;
}

webrtc::VideoRotation WebRtcVideoRenderFrame::GetVideoRotation() const {
  UNIMPLEMENTED;
  return webrtc::kVideoRotation_0;
}

// TODO(magjed): Make this copy shallow instead of deep, BUG=1128. There is no
// way to guarantee that the underlying webrtc::I420VideoFrame |frame_| will
// outlive the returned object. The only safe option is to make a deep copy.
// This can be fixed by making webrtc::I420VideoFrame reference counted, or
// adding a similar shallow copy function to it.
VideoFrame* WebRtcVideoRenderFrame::Copy() const {
  if (frame_->native_handle() != NULL) {
    return new WebRtcTextureVideoFrame(
        static_cast<webrtc::NativeHandle*>(frame_->native_handle()),
        static_cast<size_t>(frame_->width()),
        static_cast<size_t>(frame_->height()), GetElapsedTime(),
        GetTimeStamp());
  }
  WebRtcVideoFrame* new_frame = new WebRtcVideoFrame();
  new_frame->InitToEmptyBuffer(frame_->width(), frame_->height(), 1, 1,
                               GetElapsedTime(), GetTimeStamp());
  CopyToPlanes(new_frame->GetYPlane(), new_frame->GetUPlane(),
               new_frame->GetVPlane(), new_frame->GetYPitch(),
               new_frame->GetUPitch(), new_frame->GetVPitch());
  return new_frame;
}

bool WebRtcVideoRenderFrame::MakeExclusive() {
  UNIMPLEMENTED;
  return false;
}

size_t WebRtcVideoRenderFrame::CopyToBuffer(uint8* buffer, size_t size) const {
  UNIMPLEMENTED;
  return 0;
}

VideoFrame* WebRtcVideoRenderFrame::CreateEmptyFrame(int w,
                                                     int h,
                                                     size_t pixel_width,
                                                     size_t pixel_height,
                                                     int64_t elapsed_time,
                                                     int64_t time_stamp) const {
  WebRtcVideoFrame* frame = new WebRtcVideoFrame();
  frame->InitToBlack(w, h, pixel_width, pixel_height, elapsed_time, time_stamp);
  return frame;
}

}  // namespace cricket
