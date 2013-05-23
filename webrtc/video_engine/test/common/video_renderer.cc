/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/common/video_renderer.h"

#include "webrtc/modules/video_capture/include/video_capture_factory.h"
#include "webrtc/video_engine/new_include/video_send_stream.h"

#ifdef WEBRTC_TEST_XV
#include "webrtc/video_engine/test/common/linux/xv_renderer.h"
#endif  // WEBRTC_TEST_XV

// Platform-specific renderers preferred over NullRenderer
#ifdef WEBRTC_TEST_GLX
#include "webrtc/video_engine/test/common/linux/glx_renderer.h"
#endif  // WEBRTC_TEST_GLX

// TODO(pbos): Mac renderer
// TODO(pbos): Windows renderer
// TODO(pbos): Android renderer

namespace webrtc {
namespace test {

class NullRenderer : public VideoRenderer {
  virtual void RenderFrame(const I420VideoFrame& video_frame,
                           int time_to_render_ms) OVERRIDE {}
};

VideoRenderer* VideoRenderer::Create(const char* window_title,
                                     size_t width,
                                     size_t height) {
#ifdef WEBRTC_TEST_XV
  XvRenderer* xv_renderer = XvRenderer::Create(window_title, width, height);
  if (xv_renderer != NULL) {
    return xv_renderer;
  }
#endif  // WEBRTC_TEST_XV
#ifdef WEBRTC_TEST_GLX
  GlxRenderer* glx_renderer = GlxRenderer::Create(window_title, width, height);
  if (glx_renderer != NULL) {
    return glx_renderer;
  }
#endif  // WEBRTC_TEST_GLX

  // Avoid initialized-but-not-referenced errors when only building a
  // NullRenderer
  (void) width;
  (void) height;

  return new NullRenderer();
}
}  // test
}  // webrtc
