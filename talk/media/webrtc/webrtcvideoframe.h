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
#include "webrtc/common_video/include/video_frame_buffer.h"

namespace cricket {

struct CapturedFrame;

class WebRtcVideoFrame : public VideoFrame {
 public:
  WebRtcVideoFrame();
  WebRtcVideoFrame(const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& buffer,
                   int64_t time_stamp_ns,
                   webrtc::VideoRotation rotation);

  ~WebRtcVideoFrame();

  // Creates a frame from a raw sample with FourCC "format" and size "w" x "h".
  // "h" can be negative indicating a vertically flipped image.
  // "dh" is destination height if cropping is desired and is always positive.
  // Returns "true" if successful.
  bool Init(uint32_t format,
            int w,
            int h,
            int dw,
            int dh,
            uint8_t* sample,
            size_t sample_size,
            size_t pixel_width,
            size_t pixel_height,
            int64_t time_stamp_ns,
            webrtc::VideoRotation rotation);

  bool Init(const CapturedFrame* frame, int dw, int dh, bool apply_rotation);

  void InitToEmptyBuffer(int w, int h, size_t pixel_width, size_t pixel_height,
                         int64_t time_stamp_ns);

  bool InitToBlack(int w, int h, size_t pixel_width, size_t pixel_height,
                   int64_t time_stamp_ns) override;

  // From base class VideoFrame.
  bool Reset(uint32_t format,
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
                     bool apply_rotation) override;

  size_t GetWidth() const override;
  size_t GetHeight() const override;
  const uint8_t* GetYPlane() const override;
  const uint8_t* GetUPlane() const override;
  const uint8_t* GetVPlane() const override;
  uint8_t* GetYPlane() override;
  uint8_t* GetUPlane() override;
  uint8_t* GetVPlane() override;
  int32_t GetYPitch() const override;
  int32_t GetUPitch() const override;
  int32_t GetVPitch() const override;
  void* GetNativeHandle() const override;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> GetVideoFrameBuffer()
      const override;

  size_t GetPixelWidth() const override { return pixel_width_; }
  size_t GetPixelHeight() const override { return pixel_height_; }
  int64_t GetTimeStamp() const override { return time_stamp_ns_; }
  void SetTimeStamp(int64_t time_stamp_ns) override {
    time_stamp_ns_ = time_stamp_ns;
  }

  webrtc::VideoRotation GetVideoRotation() const override {
    return rotation_;
  }

  VideoFrame* Copy() const override;
  bool IsExclusive() const override;
  bool MakeExclusive() override;
  size_t ConvertToRgbBuffer(uint32_t to_fourcc,
                            uint8_t* buffer,
                            size_t size,
                            int stride_rgb) const override;

  const VideoFrame* GetCopyWithRotationApplied() const override;

 protected:
  void SetRotation(webrtc::VideoRotation rotation) override {
    rotation_ = rotation;
  }

 private:
  VideoFrame* CreateEmptyFrame(int w, int h, size_t pixel_width,
                               size_t pixel_height,
                               int64_t time_stamp_ns) const override;

  // An opaque reference counted handle that stores the pixel data.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer_;
  size_t pixel_width_;
  size_t pixel_height_;
  int64_t time_stamp_ns_;
  webrtc::VideoRotation rotation_;

  // This is mutable as the calculation is expensive but once calculated, it
  // remains const.
  mutable rtc::scoped_ptr<VideoFrame> rotated_frame_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTCVIDEOFRAME_H_
