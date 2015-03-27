/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_MEDIA_BASE_NULLVIDEOFRAME_H_
#define TALK_MEDIA_BASE_NULLVIDEOFRAME_H_

#include "talk/media/base/videoframe.h"

namespace cricket {

// Simple subclass for use in mocks.
class NullVideoFrame : public VideoFrame {
 public:
  virtual bool Reset(uint32 format,
                     int w,
                     int h,
                     int dw,
                     int dh,
                     uint8* sample,
                     size_t sample_size,
                     size_t pixel_width,
                     size_t pixel_height,
                     int64 elapsed_time,
                     int64 time_stamp,
                     webrtc::VideoRotation rotation,
                     bool apply_rotation) {
    return false;
  }
  virtual bool InitToBlack(int w, int h, size_t pixel_width,
                           size_t pixel_height, int64 elapsed_time,
                           int64 time_stamp) {
    return false;
  }
  virtual size_t GetWidth() const { return 0; }
  virtual size_t GetHeight() const { return 0; }
  virtual const uint8 *GetYPlane() const { return NULL; }
  virtual const uint8 *GetUPlane() const { return NULL; }
  virtual const uint8 *GetVPlane() const { return NULL; }
  virtual uint8 *GetYPlane() { return NULL; }
  virtual uint8 *GetUPlane() { return NULL; }
  virtual uint8 *GetVPlane() { return NULL; }
  virtual int32 GetYPitch() const { return 0; }
  virtual int32 GetUPitch() const { return 0; }
  virtual int32 GetVPitch() const { return 0; }
  virtual void* GetNativeHandle() const { return NULL; }

  virtual size_t GetPixelWidth() const { return 1; }
  virtual size_t GetPixelHeight() const { return 1; }
  virtual int64 GetElapsedTime() const { return 0; }
  virtual int64 GetTimeStamp() const { return 0; }
  virtual void SetElapsedTime(int64 elapsed_time) {}
  virtual void SetTimeStamp(int64 time_stamp) {}
  virtual webrtc::VideoRotation GetVideoRotation() const {
    return webrtc::kVideoRotation_0;
  }

  virtual VideoFrame *Copy() const { return NULL; }

  virtual bool IsExclusive() const { return false; }

  virtual bool MakeExclusive() { return false; }

  virtual size_t CopyToBuffer(uint8 *buffer, size_t size) const { return 0; }

  virtual size_t ConvertToRgbBuffer(uint32 to_fourcc, uint8 *buffer,
                                    size_t size, int stride_rgb) const {
    return 0;
  }

  virtual void StretchToPlanes(
      uint8 *y, uint8 *u, uint8 *v, int32 pitchY, int32 pitchU, int32 pitchV,
      size_t width, size_t height, bool interpolate, bool crop) const {}

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> GetVideoFrameBuffer() const {
    return NULL;
  }

  virtual VideoFrame *CreateEmptyFrame(int w, int h, size_t pixel_width,
                                       size_t pixel_height, int64 elapsed_time,
                                       int64 time_stamp) const {
    return NULL;
  }

  virtual const VideoFrame* GetCopyWithRotationApplied() const { return NULL; }
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_NULLVIDEOFRAME_H_
