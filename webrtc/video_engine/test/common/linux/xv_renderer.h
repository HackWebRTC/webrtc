/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_LINUX_XV_RENDERER_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_LINUX_XV_RENDERER_H_

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xvlib.h>

#include "webrtc/video_engine/test/common/video_renderer.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {

class XvRenderer : public VideoRenderer {
 public:
  ~XvRenderer();

  virtual void RenderFrame(const webrtc::I420VideoFrame& frame, int delta)
      OVERRIDE;

  static XvRenderer* Create(const char *window_title, size_t width,
                            size_t height);

 private:
  XvRenderer(size_t width, size_t height);

  bool Init(const char *window_title);
  void Resize(size_t width, size_t height);
  void Destroy();

  size_t width, height;
  bool is_init;

  Display* display;
  Window window;
  GC gc;
  XvImage* image;
  XShmSegmentInfo shm_info;
  int xv_port, xv_complete;
};
}  // test
}  // webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_LINUX_XV_RENDERER_H_
