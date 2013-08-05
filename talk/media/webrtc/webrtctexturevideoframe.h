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

#ifndef TALK_MEDIA_WEBRTC_WEBRTCTEXTUREVIDEOFRAME_H_
#define TALK_MEDIA_WEBRTC_WEBRTCTEXTUREVIDEOFRAME_H_

#include "talk/base/refcount.h"
#include "talk/base/scoped_ref_ptr.h"
#include "talk/media/base/videoframe.h"
#ifdef USE_WEBRTC_DEV_BRANCH
#include "webrtc/common_video/interface/native_handle.h"
#else
#include "webrtc/common_video/interface/i420_video_frame.h"
// Define NativeHandle to an existing type so we don't need to add lots of
// USE_WEBRTC_DEV_BRANCH.
#define NativeHandle I420VideoFrame
#endif

namespace cricket {

// A video frame backed by the texture via a native handle.
class WebRtcTextureVideoFrame : public VideoFrame {
 public:
  WebRtcTextureVideoFrame(webrtc::NativeHandle* handle, int width, int height,
                          int64 elapsed_time, int64 time_stamp);
  virtual ~WebRtcTextureVideoFrame();

  // From base class VideoFrame.
  virtual bool InitToBlack(int w, int h, size_t pixel_width,
                           size_t pixel_height, int64 elapsed_time,
                           int64 time_stamp);
  virtual bool Reset(uint32 fourcc, int w, int h, int dw, int dh, uint8* sample,
                     size_t sample_size, size_t pixel_width,
                     size_t pixel_height, int64 elapsed_time, int64 time_stamp,
                     int rotation);
  virtual size_t GetWidth() const { return width_; }
  virtual size_t GetHeight() const { return height_; }
  virtual const uint8* GetYPlane() const;
  virtual const uint8* GetUPlane() const;
  virtual const uint8* GetVPlane() const;
  virtual uint8* GetYPlane();
  virtual uint8* GetUPlane();
  virtual uint8* GetVPlane();
  virtual int32 GetYPitch() const;
  virtual int32 GetUPitch() const;
  virtual int32 GetVPitch() const;
  virtual size_t GetPixelWidth() const { return 1; }
  virtual size_t GetPixelHeight() const { return 1; }
  virtual int64 GetElapsedTime() const { return elapsed_time_; }
  virtual int64 GetTimeStamp() const { return time_stamp_; }
  virtual void SetElapsedTime(int64 elapsed_time) {
    elapsed_time_ = elapsed_time;
  }
  virtual void SetTimeStamp(int64 time_stamp) { time_stamp_ = time_stamp; }
  virtual int GetRotation() const { return 0; }
  virtual VideoFrame* Copy() const;
  virtual bool MakeExclusive();
  virtual size_t CopyToBuffer(uint8* buffer, size_t size) const;
  virtual size_t ConvertToRgbBuffer(uint32 to_fourcc, uint8* buffer,
                                    size_t size, int stride_rgb) const;
  virtual void* GetNativeHandle() const { return handle_.get(); }

  virtual bool CopyToPlanes(
      uint8* dst_y, uint8* dst_u, uint8* dst_v,
      int32 dst_pitch_y, int32 dst_pitch_u, int32 dst_pitch_v) const;
  virtual void CopyToFrame(VideoFrame* target) const;
  virtual talk_base::StreamResult Write(talk_base::StreamInterface* stream,
                                        int* error);
  virtual void StretchToPlanes(
      uint8* y, uint8* u, uint8* v, int32 pitchY, int32 pitchU, int32 pitchV,
      size_t width, size_t height, bool interpolate, bool crop) const;
  virtual size_t StretchToBuffer(size_t w, size_t h, uint8* buffer, size_t size,
                                 bool interpolate, bool crop) const;
  virtual void StretchToFrame(VideoFrame* target, bool interpolate,
                              bool crop) const;
  virtual VideoFrame* Stretch(size_t w, size_t h, bool interpolate,
                              bool crop) const;
  virtual bool SetToBlack();

 protected:
  virtual VideoFrame* CreateEmptyFrame(int w, int h, size_t pixel_width,
                                       size_t pixel_height, int64 elapsed_time,
                                       int64 time_stamp) const;

 private:
  // The handle of the underlying video frame.
  talk_base::scoped_refptr<webrtc::NativeHandle> handle_;
  int width_;
  int height_;
  int64 elapsed_time_;
  int64 time_stamp_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTC_WEBRTCTEXTUREVIDEOFRAME_H_
