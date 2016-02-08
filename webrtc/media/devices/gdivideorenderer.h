/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// Definition of class GdiVideoRenderer that implements the abstract class
// cricket::VideoRenderer via GDI on Windows.

#ifndef WEBRTC_MEDIA_DEVICES_GDIVIDEORENDERER_H_
#define WEBRTC_MEDIA_DEVICES_GDIVIDEORENDERER_H_

#ifdef WIN32
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/media/base/videorenderer.h"

namespace cricket {

class GdiVideoRenderer : public VideoRenderer {
 public:
  GdiVideoRenderer(int x, int y);
  virtual ~GdiVideoRenderer();

  // Implementation of pure virtual methods of VideoRenderer.
  // These two methods may be executed in different threads.
  // SetSize is called before RenderFrame.
  virtual bool SetSize(int width, int height, int reserved);
  virtual bool RenderFrame(const VideoFrame* frame);

 private:
  class VideoWindow;  // forward declaration, defined in the .cc file
  rtc::scoped_ptr<VideoWindow> window_;
  // The initial position of the window.
  int initial_x_;
  int initial_y_;
};

}  // namespace cricket

#endif  // WIN32
#endif  // WEBRTC_MEDIA_DEVICES_GDIVIDEORENDERER_H_
