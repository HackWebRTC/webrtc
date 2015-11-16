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

#ifndef TALK_MEDIA_BASE_VIDEOFRAME_H_
#define TALK_MEDIA_BASE_VIDEOFRAME_H_

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

  virtual bool InitToBlack(int w, int h, size_t pixel_width,
                           size_t pixel_height, int64_t time_stamp) = 0;
  // Creates a frame from a raw sample with FourCC |format| and size |w| x |h|.
  // |h| can be negative indicating a vertically flipped image.
  // |dw| is destination width; can be less than |w| if cropping is desired.
  // |dh| is destination height, like |dw|, but must be a positive number.
  // Returns whether the function succeeded or failed.

  virtual bool Reset(uint32_t fourcc,
                     int w,
                     int h,
                     int dw,
                     int dh,
                     uint8_t* sample,
                     size_t sample_size,
                     size_t pixel_width,
                     size_t pixel_height,
                     int64_t time_stamp,
                     webrtc::VideoRotation rotation,
                     bool apply_rotation) = 0;

  // Basic accessors.
  // Note this is the width and height without rotation applied.
  virtual size_t GetWidth() const = 0;
  virtual size_t GetHeight() const = 0;

  size_t GetChromaWidth() const { return (GetWidth() + 1) / 2; }
  size_t GetChromaHeight() const { return (GetHeight() + 1) / 2; }
  size_t GetChromaSize() const { return GetUPitch() * GetChromaHeight(); }
  // These can return NULL if the object is not backed by a buffer.
  virtual const uint8_t* GetYPlane() const = 0;
  virtual const uint8_t* GetUPlane() const = 0;
  virtual const uint8_t* GetVPlane() const = 0;
  virtual uint8_t* GetYPlane() = 0;
  virtual uint8_t* GetUPlane() = 0;
  virtual uint8_t* GetVPlane() = 0;

  virtual int32_t GetYPitch() const = 0;
  virtual int32_t GetUPitch() const = 0;
  virtual int32_t GetVPitch() const = 0;

  // Returns the handle of the underlying video frame. This is used when the
  // frame is backed by a texture. The object should be destroyed when it is no
  // longer in use, so the underlying resource can be freed.
  virtual void* GetNativeHandle() const = 0;

  // Returns the underlying video frame buffer. This function is ok to call
  // multiple times, but the returned object will refer to the same memory.
  virtual rtc::scoped_refptr<webrtc::VideoFrameBuffer> GetVideoFrameBuffer()
      const = 0;

  // For retrieving the aspect ratio of each pixel. Usually this is 1x1, but
  // the aspect_ratio_idc parameter of H.264 can specify non-square pixels.
  virtual size_t GetPixelWidth() const = 0;
  virtual size_t GetPixelHeight() const = 0;

  virtual int64_t GetTimeStamp() const = 0;
  virtual void SetTimeStamp(int64_t time_stamp) = 0;

  // Indicates the rotation angle in degrees.
  // TODO(guoweis): Remove this function, rename GetVideoRotation and remove the
  // skeleton implementation of GetRotation once chrome is updated.
  virtual int GetRotation() const { return GetVideoRotation(); }
  virtual webrtc::VideoRotation GetVideoRotation() const {
    return webrtc::kVideoRotation_0;
  }

  // Make a shallow copy of the frame. The frame buffer itself is not copied.
  // Both the current and new VideoFrame will share a single reference-counted
  // frame buffer.
  virtual VideoFrame *Copy() const = 0;

  // Since VideoFrame supports shallow copy and the internal frame buffer might
  // be shared, this function can be used to check exclusive ownership.
  virtual bool IsExclusive() const = 0;

  // In case VideoFrame needs exclusive access of the frame buffer, user can
  // call MakeExclusive() to make sure the frame buffer is exclusively
  // accessible to the current object.  This might mean a deep copy of the frame
  // buffer if it is currently shared by other objects.
  virtual bool MakeExclusive() = 0;

  // Writes the frame into the given frame buffer, provided that it is of
  // sufficient size. Returns the frame's actual size, regardless of whether
  // it was written or not (like snprintf). If there is insufficient space,
  // nothing is written.
  virtual size_t CopyToBuffer(uint8_t* buffer, size_t size) const;

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

  // Writes the frame into the target VideoFrame.
  virtual void CopyToFrame(VideoFrame* target) const;

  // Return a copy of frame which has its pending rotation applied. The
  // ownership of the returned frame is held by this frame.
  virtual const VideoFrame* GetCopyWithRotationApplied() const = 0;

  // Writes the frame into the given stream and returns the StreamResult.
  // See webrtc/base/stream.h for a description of StreamResult and error.
  // Error may be NULL. If a non-success value is returned from
  // StreamInterface::Write(), we immediately return with that value.
  virtual rtc::StreamResult Write(rtc::StreamInterface* stream,
                                  int* error) const;

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

  // Size of an I420 image of given dimensions when stored as a frame buffer.
  static size_t SizeOf(size_t w, size_t h) {
    return w * h + ((w + 1) / 2) * ((h + 1) / 2) * 2;
  }

 protected:
  // Creates an empty frame.
  virtual VideoFrame *CreateEmptyFrame(int w, int h, size_t pixel_width,
                                       size_t pixel_height,
                                       int64_t time_stamp) const = 0;
  virtual void SetRotation(webrtc::VideoRotation rotation) = 0;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_VIDEOFRAME_H_
