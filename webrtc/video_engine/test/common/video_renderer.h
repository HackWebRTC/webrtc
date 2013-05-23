/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_VIDEO_RENDERER_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_VIDEO_RENDERER_H_

#include "webrtc/video_engine/new_include/common.h"

namespace webrtc {
namespace test {

class VideoRenderer : public newapi::VideoRenderer {
 public:
  static VideoRenderer* Create(const char* window_title,
                               size_t width,
                               size_t height);
  virtual ~VideoRenderer() {}
 protected:
  VideoRenderer() {}
};
}  // test
}  // webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_VIDEO_RENDERER_H_
