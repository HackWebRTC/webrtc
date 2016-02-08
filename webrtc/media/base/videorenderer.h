/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_VIDEORENDERER_H_
#define WEBRTC_MEDIA_BASE_VIDEORENDERER_H_

#include "webrtc/media/base/videosinkinterface.h"

namespace cricket {

class VideoFrame;

// Abstract interface for rendering VideoFrames.
class VideoRenderer : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  virtual ~VideoRenderer() {}
  // Called when a new frame is available for display.
  virtual bool RenderFrame(const VideoFrame *frame) = 0;
  // Intended to replace RenderFrame.
  void OnFrame(const cricket::VideoFrame& frame) override {
    // Unused return value
    RenderFrame(&frame);
  }
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_VIDEORENDERER_H_
