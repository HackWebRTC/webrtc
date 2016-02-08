/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_WEBRTC_WEBRTCVIDEOFRAME_H_
#define WEBRTC_MEDIA_WEBRTC_WEBRTCVIDEOFRAME_H_

#include "webrtc/base/buffer.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/media/base/videoframe.h"

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
            int64_t time_stamp_ns,
            webrtc::VideoRotation rotation);

  bool Init(const CapturedFrame* frame, int dw, int dh, bool apply_rotation);

  void InitToEmptyBuffer(int w, int h, int64_t time_stamp_ns);

  bool InitToBlack(int w, int h, int64_t time_stamp_ns) override;

  // From base class VideoFrame.
  bool Reset(uint32_t format,
                     int w,
                     int h,
                     int dw,
                     int dh,
                     uint8_t* sample,
                     size_t sample_size,
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
  VideoFrame* CreateEmptyFrame(int w, int h,
                               int64_t time_stamp_ns) const override;

  // An opaque reference counted handle that stores the pixel data.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer_;
  int64_t time_stamp_ns_;
  webrtc::VideoRotation rotation_;

  // This is mutable as the calculation is expensive but once calculated, it
  // remains const.
  mutable rtc::scoped_ptr<VideoFrame> rotated_frame_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_WEBRTC_WEBRTCVIDEOFRAME_H_
