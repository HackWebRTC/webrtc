/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_RENDERER_H_
#define WEBRTC_VIDEO_RENDERER_H_

#include "webrtc/media/base/videosinkinterface.h"

namespace webrtc {

class VideoFrame;

class VideoRenderer : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  // This function returns true if WebRTC should not delay frames for
  // smoothness. In general, this case means the renderer can schedule frames to
  // optimize smoothness.
  virtual bool SmoothsRenderedFrames() const { return false; }

 protected:
  virtual ~VideoRenderer() {}
};
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_RENDERER_H_
