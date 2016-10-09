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

// TODO(nisse): This class duplicates webrtc::VideoFrame. There's
// ongoing work to merge the classes. See
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5682.
class VideoFrame {
 public:
  VideoFrame() {}
  virtual ~VideoFrame() {}

  // Basic accessors.
  // Note this is the width and height without rotation applied.
  virtual int width() const = 0;
  virtual int height() const = 0;

  // Returns the underlying video frame buffer. This function is ok to call
  // multiple times, but the returned object will refer to the same memory.
  virtual const rtc::scoped_refptr<webrtc::VideoFrameBuffer>&
  video_frame_buffer() const = 0;

  // Frame ID. Normally RTP timestamp when the frame was received using RTP.
  virtual uint32_t transport_frame_id() const = 0;

  // System monotonic clock, same timebase as rtc::TimeMicros().
  virtual int64_t timestamp_us() const = 0;
  virtual void set_timestamp_us(int64_t time_us) = 0;

  // Indicates the rotation angle in degrees.
  virtual webrtc::VideoRotation rotation() const = 0;

  // Tests if sample is valid. Returns true if valid.

  // TODO(nisse): Deprecated. Should be deleted in the cricket::VideoFrame and
  // webrtc::VideoFrame merge. Validation of sample_size possibly moved to
  // libyuv::ConvertToI420. As an initial step, demote this method to protected
  // status. Used only by WebRtcVideoFrame::Reset.
  static bool Validate(uint32_t fourcc,
                       int w,
                       int h,
                       const uint8_t* sample,
                       size_t sample_size);
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_VIDEOFRAME_H_
