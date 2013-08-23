/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_FILE_CAPTURER_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_FILE_CAPTURER_H_

#include <stdio.h>

#include "webrtc/typedefs.h"
#include "webrtc/video_engine/test/common/frame_generator.h"
#include "webrtc/video_engine/test/common/video_capturer.h"

namespace webrtc {

class Clock;

class VideoSendStreamInput;

namespace test {

class YuvFileFrameGenerator : public FrameGenerator {
 public:
  static YuvFileFrameGenerator* Create(const char* file_name,
                                       size_t width,
                                       size_t height,
                                       Clock* clock);
  virtual ~YuvFileFrameGenerator();

 private:
  YuvFileFrameGenerator(FILE* file, size_t width, size_t height, Clock* clock);
  virtual void GenerateNextFrame() OVERRIDE;

  FILE* file_;
  size_t frame_size_;
  uint8_t* frame_buffer_;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_VIDEO_CAPTURER_H_
