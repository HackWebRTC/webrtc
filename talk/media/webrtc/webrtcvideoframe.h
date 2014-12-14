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

#ifndef TALK_MEDIA_WEBRTCVIDEOFRAME_H_
#define TALK_MEDIA_WEBRTCVIDEOFRAME_H_

#include "talk/media/base/videoframe.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/interface/module_common_types.h"

namespace webrtc {
class I420VideoFrame;
};

namespace cricket {

struct CapturedFrame;

class WebRtcVideoFrame : public VideoFrame {
 public:
  WebRtcVideoFrame();
  ~WebRtcVideoFrame();

  // Creates a frame from a raw sample with FourCC "format" and size "w" x "h".
  // "h" can be negative indicating a vertically flipped image.
  // "dh" is destination height if cropping is desired and is always positive.
  // Returns "true" if successful.
  bool Init(uint32 format, int w, int h, int dw, int dh, uint8* sample,
            size_t sample_size, size_t pixel_width, size_t pixel_height,
            int64_t elapsed_time, int64_t time_stamp, int rotation);

  bool Init(const CapturedFrame* frame, int dw, int dh);

  // Aliases this WebRtcVideoFrame to a CapturedFrame. |frame| must outlive
  // this WebRtcVideoFrame.
  bool Alias(const CapturedFrame* frame, int dw, int dh);

  bool InitToBlack(int w, int h, size_t pixel_width, size_t pixel_height,
                   int64_t elapsed_time, int64_t time_stamp);

  // Aliases this WebRtcVideoFrame to a memory buffer. |buffer| must outlive
  // this WebRtcVideoFrame.
  void Alias(uint8* buffer, size_t buffer_size, int w, int h,
             size_t pixel_width, size_t pixel_height, int64_t elapsed_time,
             int64_t time_stamp, int rotation);

  webrtc::VideoFrame* frame();
  const webrtc::VideoFrame* frame() const;

  // From base class VideoFrame.
  virtual bool Reset(uint32 format, int w, int h, int dw, int dh, uint8* sample,
                     size_t sample_size, size_t pixel_width,
                     size_t pixel_height, int64_t elapsed_time,
                     int64_t time_stamp, int rotation);

  virtual size_t GetWidth() const;
  virtual size_t GetHeight() const;
  virtual const uint8* GetYPlane() const;
  virtual const uint8* GetUPlane() const;
  virtual const uint8* GetVPlane() const;
  virtual uint8* GetYPlane();
  virtual uint8* GetUPlane();
  virtual uint8* GetVPlane();
  virtual int32 GetYPitch() const { return frame()->Width(); }
  virtual int32 GetUPitch() const { return (frame()->Width() + 1) / 2; }
  virtual int32 GetVPitch() const { return (frame()->Width() + 1) / 2; }
  virtual void* GetNativeHandle() const { return NULL; }

  virtual size_t GetPixelWidth() const { return pixel_width_; }
  virtual size_t GetPixelHeight() const { return pixel_height_; }
  virtual int64_t GetElapsedTime() const { return elapsed_time_; }
  virtual int64_t GetTimeStamp() const { return time_stamp_; }
  virtual void SetElapsedTime(int64_t elapsed_time) {
    elapsed_time_ = elapsed_time;
  }
  virtual void SetTimeStamp(int64_t time_stamp) { time_stamp_ = time_stamp; }

  virtual int GetRotation() const { return rotation_; }

  virtual VideoFrame* Copy() const;
  virtual bool MakeExclusive();
  virtual size_t CopyToBuffer(uint8* buffer, size_t size) const;
  virtual size_t ConvertToRgbBuffer(uint32 to_fourcc, uint8* buffer,
                                    size_t size, int stride_rgb) const;

 private:
  class FrameBuffer;
  typedef rtc::RefCountedObject<FrameBuffer> RefCountedBuffer;

  void Attach(RefCountedBuffer* video_buffer, size_t buffer_size, int w, int h,
              size_t pixel_width, size_t pixel_height, int64_t elapsed_time,
              int64_t time_stamp, int rotation);

  virtual VideoFrame* CreateEmptyFrame(int w, int h, size_t pixel_width,
                                       size_t pixel_height,
                                       int64_t elapsed_time,
                                       int64_t time_stamp) const;
  void InitToEmptyBuffer(int w, int h, size_t pixel_width, size_t pixel_height,
                         int64_t elapsed_time, int64_t time_stamp);

  rtc::scoped_refptr<RefCountedBuffer> video_buffer_;
  size_t pixel_width_;
  size_t pixel_height_;
  int64_t elapsed_time_;
  int64_t time_stamp_;
  int rotation_;
};

// Thin map between VideoFrame and an existing webrtc::I420VideoFrame
// to avoid having to copy the rendered VideoFrame prematurely.
// This implementation is only safe to use in a const context and should never
// be written to.
class WebRtcVideoRenderFrame : public VideoFrame {
 public:
  explicit WebRtcVideoRenderFrame(const webrtc::I420VideoFrame* frame);

  virtual bool InitToBlack(int w,
                           int h,
                           size_t pixel_width,
                           size_t pixel_height,
                           int64_t elapsed_time,
                           int64_t time_stamp) OVERRIDE;
  virtual bool Reset(uint32 fourcc,
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
                     int rotation) OVERRIDE;
  virtual size_t GetWidth() const OVERRIDE;
  virtual size_t GetHeight() const OVERRIDE;
  virtual const uint8* GetYPlane() const OVERRIDE;
  virtual const uint8* GetUPlane() const OVERRIDE;
  virtual const uint8* GetVPlane() const OVERRIDE;
  virtual uint8* GetYPlane() OVERRIDE;
  virtual uint8* GetUPlane() OVERRIDE;
  virtual uint8* GetVPlane() OVERRIDE;
  virtual int32 GetYPitch() const OVERRIDE;
  virtual int32 GetUPitch() const OVERRIDE;
  virtual int32 GetVPitch() const OVERRIDE;
  virtual void* GetNativeHandle() const OVERRIDE;
  virtual size_t GetPixelWidth() const OVERRIDE;
  virtual size_t GetPixelHeight() const OVERRIDE;
  virtual int64_t GetElapsedTime() const OVERRIDE;
  virtual int64_t GetTimeStamp() const OVERRIDE;
  virtual void SetElapsedTime(int64_t elapsed_time) OVERRIDE;
  virtual void SetTimeStamp(int64_t time_stamp) OVERRIDE;
  virtual int GetRotation() const OVERRIDE;
  virtual VideoFrame* Copy() const OVERRIDE;
  virtual bool MakeExclusive() OVERRIDE;
  virtual size_t CopyToBuffer(uint8* buffer, size_t size) const OVERRIDE;

 protected:
  virtual VideoFrame* CreateEmptyFrame(int w,
                                       int h,
                                       size_t pixel_width,
                                       size_t pixel_height,
                                       int64_t elapsed_time,
                                       int64_t time_stamp) const OVERRIDE;

 private:
  const webrtc::I420VideoFrame* const frame_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTCVIDEOFRAME_H_
