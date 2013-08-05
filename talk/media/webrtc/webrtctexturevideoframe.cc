/*
 * libjingle
 * Copyright 2013 Google Inc.
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

#include "talk/media/webrtc/webrtctexturevideoframe.h"

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/stream.h"

#define UNIMPLEMENTED \
  LOG(LS_ERROR) << "Call to unimplemented function "<< __FUNCTION__; \
  ASSERT(false)

namespace cricket {

WebRtcTextureVideoFrame::WebRtcTextureVideoFrame(
    webrtc::NativeHandle* handle, int width, int height, int64 elapsed_time,
    int64 time_stamp)
    : handle_(handle), width_(width), height_(height),
      elapsed_time_(elapsed_time), time_stamp_(time_stamp) {}

WebRtcTextureVideoFrame::~WebRtcTextureVideoFrame() {}

bool WebRtcTextureVideoFrame::InitToBlack(
    int w, int h, size_t pixel_width, size_t pixel_height, int64 elapsed_time,
    int64 time_stamp) {
  UNIMPLEMENTED;
  return false;
}

bool WebRtcTextureVideoFrame::Reset(
    uint32 fourcc, int w, int h, int dw, int dh, uint8* sample,
    size_t sample_size, size_t pixel_width, size_t pixel_height,
    int64 elapsed_time, int64 time_stamp, int rotation) {
  UNIMPLEMENTED;
  return false;
}

const uint8* WebRtcTextureVideoFrame::GetYPlane() const {
  UNIMPLEMENTED;
  return NULL;
}

const uint8* WebRtcTextureVideoFrame::GetUPlane() const {
  UNIMPLEMENTED;
  return NULL;
}

const uint8* WebRtcTextureVideoFrame::GetVPlane() const {
  UNIMPLEMENTED;
  return NULL;
}

uint8* WebRtcTextureVideoFrame::GetYPlane() {
  UNIMPLEMENTED;
  return NULL;
}

uint8* WebRtcTextureVideoFrame::GetUPlane() {
  UNIMPLEMENTED;
  return NULL;
}

uint8* WebRtcTextureVideoFrame::GetVPlane() {
  UNIMPLEMENTED;
  return NULL;
}

int32 WebRtcTextureVideoFrame::GetYPitch() const {
  UNIMPLEMENTED;
  return width_;
}

int32 WebRtcTextureVideoFrame::GetUPitch() const {
  UNIMPLEMENTED;
  return (width_ + 1) / 2;
}

int32 WebRtcTextureVideoFrame::GetVPitch() const {
  UNIMPLEMENTED;
  return (width_ + 1) / 2;
}

VideoFrame* WebRtcTextureVideoFrame::Copy() const {
  return new WebRtcTextureVideoFrame(
      handle_, width_, height_, elapsed_time_, time_stamp_);
}

bool WebRtcTextureVideoFrame::MakeExclusive() {
  UNIMPLEMENTED;
  return false;
}

size_t WebRtcTextureVideoFrame::CopyToBuffer(uint8* buffer, size_t size) const {
  UNIMPLEMENTED;
  return 0;
}

size_t WebRtcTextureVideoFrame::ConvertToRgbBuffer(
    uint32 to_fourcc, uint8* buffer, size_t size, int stride_rgb) const {
  UNIMPLEMENTED;
  return 0;
}

bool WebRtcTextureVideoFrame::CopyToPlanes(
    uint8* dst_y, uint8* dst_u, uint8* dst_v, int32 dst_pitch_y,
    int32 dst_pitch_u, int32 dst_pitch_v) const {
  UNIMPLEMENTED;
  return false;
}

void WebRtcTextureVideoFrame::CopyToFrame(VideoFrame* dst) const {
  UNIMPLEMENTED;
}

talk_base::StreamResult WebRtcTextureVideoFrame::Write(
    talk_base::StreamInterface* stream, int* error) {
  UNIMPLEMENTED;
  return talk_base::SR_ERROR;
}
void WebRtcTextureVideoFrame::StretchToPlanes(
    uint8* dst_y, uint8* dst_u, uint8* dst_v, int32 dst_pitch_y,
    int32 dst_pitch_u, int32 dst_pitch_v, size_t width, size_t height,
    bool interpolate, bool vert_crop) const {
  UNIMPLEMENTED;
}

size_t WebRtcTextureVideoFrame::StretchToBuffer(
    size_t dst_width, size_t dst_height, uint8* dst_buffer, size_t size,
    bool interpolate, bool vert_crop) const {
  UNIMPLEMENTED;
  return 0;
}

void WebRtcTextureVideoFrame::StretchToFrame(
    VideoFrame* dst, bool interpolate, bool vert_crop) const {
  UNIMPLEMENTED;
}

VideoFrame* WebRtcTextureVideoFrame::Stretch(
    size_t dst_width, size_t dst_height, bool interpolate,
    bool vert_crop) const {
  UNIMPLEMENTED;
  return NULL;
}

bool WebRtcTextureVideoFrame::SetToBlack() {
  UNIMPLEMENTED;
  return false;
}

VideoFrame* WebRtcTextureVideoFrame::CreateEmptyFrame(
    int w, int h, size_t pixel_width, size_t pixel_height, int64 elapsed_time,
    int64 time_stamp) const {
  UNIMPLEMENTED;
  return NULL;
}

}  // namespace cricket
