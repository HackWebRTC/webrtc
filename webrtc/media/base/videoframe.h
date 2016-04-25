/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_VIDEOFRAME_H_
#define WEBRTC_MEDIA_BASE_VIDEOFRAME_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/base/stream.h"
#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/common_video/rotation.h"

namespace cricket {

// Represents a YUV420 (a.k.a. I420) video frame.
class VideoFrame {
 public:
  VideoFrame() {}
  virtual ~VideoFrame() {}

  // Basic accessors.
  // Note this is the width and height without rotation applied.
  virtual int width() const = 0;
  virtual int height() const = 0;

  // Deprecated methods, for backwards compatibility.
  // TODO(nisse): Delete when usage in Chrome and other applications
  // have been replaced by width() and height().
  virtual size_t GetWidth() const final { return width(); }
  virtual size_t GetHeight() const final { return height(); }

  // Returns the handle of the underlying video frame. This is used when the
  // frame is backed by a texture. The object should be destroyed when it is no
  // longer in use, so the underlying resource can be freed.
  virtual void* GetNativeHandle() const = 0;

  // Returns the underlying video frame buffer. This function is ok to call
  // multiple times, but the returned object will refer to the same memory.
  virtual const rtc::scoped_refptr<webrtc::VideoFrameBuffer>&
  video_frame_buffer() const = 0;

  // System monotonic clock, same timebase as rtc::TimeMicros().
  virtual int64_t timestamp_us() const = 0;
  virtual void set_timestamp_us(int64_t time_us) = 0;

  // Deprecated methods, for backwards compatibility.
  // TODO(nisse): Delete when usage in Chrome and other applications
  // have been replaced.
  virtual int64_t GetTimeStamp() const {
    return rtc::kNumNanosecsPerMicrosec * timestamp_us();
  }
  virtual void SetTimeStamp(int64_t time_ns) {
    set_timestamp_us(time_ns / rtc::kNumNanosecsPerMicrosec);
  }

  // Indicates the rotation angle in degrees.
  virtual webrtc::VideoRotation rotation() const = 0;

  // Make a shallow copy of the frame. The frame buffer itself is not copied.
  // Both the current and new VideoFrame will share a single reference-counted
  // frame buffer.
  virtual VideoFrame *Copy() const = 0;

  // Since VideoFrame supports shallow copy and the internal frame buffer might
  // be shared, this function can be used to check exclusive ownership.
  virtual bool IsExclusive() const = 0;

  // Return a copy of frame which has its pending rotation applied. The
  // ownership of the returned frame is held by this frame.
  virtual const VideoFrame* GetCopyWithRotationApplied() const = 0;

  // Converts the I420 data to RGB of a certain type such as ARGB and ABGR.
  // Returns the frame's actual size, regardless of whether it was written or
  // not (like snprintf). Parameters size and stride_rgb are in units of bytes.
  // If there is insufficient space, nothing is written.
  virtual size_t ConvertToRgbBuffer(uint32_t to_fourcc,
                                    uint8_t* buffer,
                                    size_t size,
                                    int stride_rgb) const;

  // Writes the frame into the given planes, stretched to the given width and
  // height. The parameter "interpolate" controls whether to interpolate or just
  // take the nearest-point. The parameter "crop" controls whether to crop this
  // frame to the aspect ratio of the given dimensions before stretching.
  virtual void StretchToPlanes(uint8_t* y,
                               uint8_t* u,
                               uint8_t* v,
                               int32_t pitchY,
                               int32_t pitchU,
                               int32_t pitchV,
                               size_t width,
                               size_t height,
                               bool interpolate,
                               bool crop) const;

  // Writes the frame into the target VideoFrame, stretched to the size of that
  // frame. The parameter "interpolate" controls whether to interpolate or just
  // take the nearest-point. The parameter "crop" controls whether to crop this
  // frame to the aspect ratio of the target frame before stretching.
  virtual void StretchToFrame(VideoFrame *target, bool interpolate,
                              bool crop) const;

  // Stretches the frame to the given size, creating a new VideoFrame object to
  // hold it. The parameter "interpolate" controls whether to interpolate or
  // just take the nearest-point. The parameter "crop" controls whether to crop
  // this frame to the aspect ratio of the given dimensions before stretching.
  virtual VideoFrame *Stretch(size_t w, size_t h, bool interpolate,
                              bool crop) const;

  // Sets the video frame to black.
  virtual bool SetToBlack();

  // Tests if sample is valid.  Returns true if valid.
  static bool Validate(uint32_t fourcc,
                       int w,
                       int h,
                       const uint8_t* sample,
                       size_t sample_size);

 protected:
  // Writes the frame into the given planes, stretched to the given width and
  // height. The parameter "interpolate" controls whether to interpolate or just
  // take the nearest-point. The parameter "crop" controls whether to crop this
  // frame to the aspect ratio of the given dimensions before stretching.
  virtual bool CopyToPlanes(uint8_t* dst_y,
                            uint8_t* dst_u,
                            uint8_t* dst_v,
                            int32_t dst_pitch_y,
                            int32_t dst_pitch_u,
                            int32_t dst_pitch_v) const;

  // Creates an empty frame.
  virtual VideoFrame* CreateEmptyFrame(int w,
                                       int h,
                                       int64_t timestamp_us) const = 0;
  virtual void set_rotation(webrtc::VideoRotation rotation) = 0;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_VIDEOFRAME_H_
