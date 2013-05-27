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

#ifdef WEBRTC_TEST_XV
#include "webrtc/video_engine/test/common/linux/xv_renderer.h"
#endif  // WEBRTC_TEST_XV
#ifdef WEBRTC_TEST_GLX
#include "webrtc/video_engine/test/common/linux/glx_renderer.h"
#endif  // WEBRTC_TEST_GLX

namespace webrtc {
namespace test {

VideoRenderer* VideoRenderer::CreatePlatformRenderer(const char* window_title,
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
  return NULL;
}
}  // test
}  // webrtc
