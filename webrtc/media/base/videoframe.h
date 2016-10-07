/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(nisse): Deprecated, replace cricket::VideoFrame with
// webrtc::VideoFrame everywhere, then delete this file. See
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5682.

#ifndef WEBRTC_MEDIA_BASE_VIDEOFRAME_H_
#define WEBRTC_MEDIA_BASE_VIDEOFRAME_H_

#include "webrtc/video_frame.h"

namespace cricket {

class VideoFrame : public webrtc::VideoFrame {
 protected:
  VideoFrame() : webrtc::VideoFrame() {}
  VideoFrame(const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& buffer,
             webrtc::VideoRotation rotation,
             int64_t timestamp_us)
      : webrtc::VideoFrame(buffer, rotation, timestamp_us) {}
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_VIDEOFRAME_H_
