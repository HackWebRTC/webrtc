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
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videocommon.h"
#include "webrtc/base/logging.h"
#include "webrtc/video_frame.h"

using webrtc::kYPlane;
using webrtc::kUPlane;
using webrtc::kVPlane;

namespace cricket {

WebRtcVideoFrame::WebRtcVideoFrame():
    pixel_width_(0),
    pixel_height_(0),
    time_stamp_ns_(0),
    rotation_(webrtc::kVideoRotation_0) {}

WebRtcVideoFrame::WebRtcVideoFrame(
    const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& buffer,
    int64_t time_stamp_ns,
    webrtc::VideoRotation rotation)
    : video_frame_buffer_(buffer),
      pixel_width_(1),
      pixel_height_(1),
      time_stamp_ns_(time_stamp_ns),
      rotation_(rotation) {
}

WebRtcVideoFrame::WebRtcVideoFrame(
    const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& buffer,
    int64_t elapsed_time_ns,
    int64_t time_stamp_ns)
    : video_frame_buffer_(buffer),
      pixel_width_(1),
      pixel_height_(1),
      time_stamp_ns_(time_stamp_ns),
      rotation_(webrtc::kVideoRotation_0) {
}

WebRtcVideoFrame::~WebRtcVideoFrame() {}

bool WebRtcVideoFrame::Init(uint32_t format,
                            int w,
                            int h,
                            int dw,
                            int dh,
                            uint8_t* sample,
                            size_t sample_size,
                            size_t pixel_width,
                            size_t pixel_height,
                            int64_t time_stamp_ns,
                            webrtc::VideoRotation rotation) {
  return Reset(format, w, h, dw, dh, sample, sample_size, pixel_width,
               pixel_height, time_stamp_ns, rotation,
               true /*apply_rotation*/);
}

bool WebRtcVideoFrame::Init(const CapturedFrame* frame, int dw, int dh,
                            bool apply_rotation) {
  return Reset(frame->fourcc, frame->width, frame->height, dw, dh,
               static_cast<uint8_t*>(frame->data), frame->data_size,
               frame->pixel_width, frame->pixel_height, frame->time_stamp,
               frame->GetRotation(), apply_rotation);
}

bool WebRtcVideoFrame::InitToBlack(int w, int h, size_t pixel_width,
                                   size_t pixel_height, int64_t,
                                   int64_t time_stamp_ns) {
  return InitToBlack(w, h, pixel_width, pixel_height, time_stamp_ns);
}

bool WebRtcVideoFrame::InitToBlack(int w, int h, size_t pixel_width,
                                   size_t pixel_height, int64_t time_stamp_ns) {
  InitToEmptyBuffer(w, h, pixel_width, pixel_height, time_stamp_ns);
  return SetToBlack();
}

size_t WebRtcVideoFrame::GetWidth() const {
  return video_frame_buffer_ ? video_frame_buffer_->width() : 0;
}

size_t WebRtcVideoFrame::GetHeight() const {
  return video_frame_buffer_ ? video_frame_buffer_->height() : 0;
}

const uint8_t* WebRtcVideoFrame::GetYPlane() const {
  return video_frame_buffer_ ? video_frame_buffer_->data(kYPlane) : nullptr;
}

const uint8_t* WebRtcVideoFrame::GetUPlane() const {
  return video_frame_buffer_ ? video_frame_buffer_->data(kUPlane) : nullptr;
}

const uint8_t* WebRtcVideoFrame::GetVPlane() const {
  return video_frame_buffer_ ? video_frame_buffer_->data(kVPlane) : nullptr;
}

uint8_t* WebRtcVideoFrame::GetYPlane() {
  return video_frame_buffer_ ? video_frame_buffer_->MutableData(kYPlane)
                             : nullptr;
}

uint8_t* WebRtcVideoFrame::GetUPlane() {
  return video_frame_buffer_ ? video_frame_buffer_->MutableData(kUPlane)
                             : nullptr;
}

uint8_t* WebRtcVideoFrame::GetVPlane() {
  return video_frame_buffer_ ? video_frame_buffer_->MutableData(kVPlane)
                             : nullptr;
}

int32_t WebRtcVideoFrame::GetYPitch() const {
  return video_frame_buffer_ ? video_frame_buffer_->stride(kYPlane) : 0;
}

int32_t WebRtcVideoFrame::GetUPitch() const {
  return video_frame_buffer_ ? video_frame_buffer_->stride(kUPlane) : 0;
}

int32_t WebRtcVideoFrame::GetVPitch() const {
  return video_frame_buffer_ ? video_frame_buffer_->stride(kVPlane) : 0;
}

bool WebRtcVideoFrame::IsExclusive() const {
  return video_frame_buffer_->HasOneRef();
}

void* WebRtcVideoFrame::GetNativeHandle() const {
  return video_frame_buffer_ ? video_frame_buffer_->native_handle() : nullptr;
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
WebRtcVideoFrame::GetVideoFrameBuffer() const {
  return video_frame_buffer_;
}

VideoFrame* WebRtcVideoFrame::Copy() const {
  WebRtcVideoFrame* new_frame = new WebRtcVideoFrame(
      video_frame_buffer_, time_stamp_ns_, rotation_);
  new_frame->pixel_width_ = pixel_width_;
  new_frame->pixel_height_ = pixel_height_;
  return new_frame;
}

bool WebRtcVideoFrame::MakeExclusive() {
  RTC_DCHECK(video_frame_buffer_->native_handle() == nullptr);
  if (IsExclusive())
    return true;

  // Not exclusive already, need to copy buffer.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> new_buffer =
      new rtc::RefCountedObject<webrtc::I420Buffer>(
          video_frame_buffer_->width(), video_frame_buffer_->height(),
          video_frame_buffer_->stride(kYPlane),
          video_frame_buffer_->stride(kUPlane),
          video_frame_buffer_->stride(kVPlane));

  if (!CopyToPlanes(
          new_buffer->MutableData(kYPlane), new_buffer->MutableData(kUPlane),
          new_buffer->MutableData(kVPlane), new_buffer->stride(kYPlane),
          new_buffer->stride(kUPlane), new_buffer->stride(kVPlane))) {
    return false;
  }

  video_frame_buffer_ = new_buffer;
  return true;
}

size_t WebRtcVideoFrame::ConvertToRgbBuffer(uint32_t to_fourcc,
                                            uint8_t* buffer,
                                            size_t size,
                                            int stride_rgb) const {
  RTC_CHECK(video_frame_buffer_);
  RTC_CHECK(video_frame_buffer_->native_handle() == nullptr);
  return VideoFrame::ConvertToRgbBuffer(to_fourcc, buffer, size, stride_rgb);
}

bool WebRtcVideoFrame::Reset(uint32_t format,
                             int w,
                             int h,
                             int dw,
                             int dh,
                             uint8_t* sample,
                             size_t sample_size,
                             size_t pixel_width,
                             size_t pixel_height,
                             int64_t time_stamp_ns,
                             webrtc::VideoRotation rotation,
                             bool apply_rotation) {
  if (!Validate(format, w, h, sample, sample_size)) {
    return false;
  }
  // Translate aliases to standard enums (e.g., IYUV -> I420).
  format = CanonicalFourCC(format);

  // Set up a new buffer.
  // TODO(fbarchard): Support lazy allocation.
  int new_width = dw;
  int new_height = dh;
  // If rotated swap width, height.
  if (apply_rotation && (rotation == 90 || rotation == 270)) {
    new_width = dh;
    new_height = dw;
  }

  InitToEmptyBuffer(new_width, new_height, pixel_width, pixel_height,
                    time_stamp_ns);
  rotation_ = apply_rotation ? webrtc::kVideoRotation_0 : rotation;

  int horiz_crop = ((w - dw) / 2) & ~1;
  // ARGB on Windows has negative height.
  // The sample's layout in memory is normal, so just correct crop.
  int vert_crop = ((abs(h) - dh) / 2) & ~1;
  // Conversion functions expect negative height to flip the image.
  int idh = (h < 0) ? -dh : dh;
  int r = libyuv::ConvertToI420(
      sample, sample_size,
      GetYPlane(), GetYPitch(),
      GetUPlane(), GetUPitch(),
      GetVPlane(), GetVPitch(),
      horiz_crop, vert_crop,
      w, h,
      dw, idh,
      static_cast<libyuv::RotationMode>(
          apply_rotation ? rotation : webrtc::kVideoRotation_0),
      format);
  if (r) {
    LOG(LS_ERROR) << "Error parsing format: " << GetFourccName(format)
                  << " return code : " << r;
    return false;
  }
  return true;
}

VideoFrame* WebRtcVideoFrame::CreateEmptyFrame(
    int w, int h, size_t pixel_width, size_t pixel_height,
    int64_t time_stamp_ns) const {
  WebRtcVideoFrame* frame = new WebRtcVideoFrame();
  frame->InitToEmptyBuffer(w, h, pixel_width, pixel_height, time_stamp_ns);
  return frame;
}

void WebRtcVideoFrame::InitToEmptyBuffer(int w, int h, size_t pixel_width,
                                         size_t pixel_height,
                                         int64_t time_stamp_ns) {
  video_frame_buffer_ = new rtc::RefCountedObject<webrtc::I420Buffer>(w, h);
  pixel_width_ = pixel_width;
  pixel_height_ = pixel_height;
  time_stamp_ns_ = time_stamp_ns;
  rotation_ = webrtc::kVideoRotation_0;
}

const VideoFrame* WebRtcVideoFrame::GetCopyWithRotationApplied() const {
  // If the frame is not rotated, the caller should reuse this frame instead of
  // making a redundant copy.
  if (GetVideoRotation() == webrtc::kVideoRotation_0) {
    return this;
  }

  // If the video frame is backed up by a native handle, it resides in the GPU
  // memory which we can't rotate here. The assumption is that the renderers
  // which uses GPU to render should be able to rotate themselves.
  RTC_DCHECK(!GetNativeHandle());

  if (rotated_frame_) {
    return rotated_frame_.get();
  }

  int width = static_cast<int>(GetWidth());
  int height = static_cast<int>(GetHeight());

  int rotated_width = width;
  int rotated_height = height;
  if (GetVideoRotation() == webrtc::kVideoRotation_90 ||
      GetVideoRotation() == webrtc::kVideoRotation_270) {
    rotated_width = height;
    rotated_height = width;
  }

  rotated_frame_.reset(CreateEmptyFrame(rotated_width, rotated_height,
                                        GetPixelWidth(), GetPixelHeight(),
                                        GetTimeStamp()));

  // TODO(guoweis): Add a function in webrtc_libyuv.cc to convert from
  // VideoRotation to libyuv::RotationMode.
  int ret = libyuv::I420Rotate(
      GetYPlane(), GetYPitch(), GetUPlane(), GetUPitch(), GetVPlane(),
      GetVPitch(), rotated_frame_->GetYPlane(), rotated_frame_->GetYPitch(),
      rotated_frame_->GetUPlane(), rotated_frame_->GetUPitch(),
      rotated_frame_->GetVPlane(), rotated_frame_->GetVPitch(), width, height,
      static_cast<libyuv::RotationMode>(GetVideoRotation()));
  if (ret == 0) {
    return rotated_frame_.get();
  }
  return nullptr;
}

}  // namespace cricket
