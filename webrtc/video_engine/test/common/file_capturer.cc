/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/common/file_capturer.h"

#include <stdio.h>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

namespace webrtc {
namespace test {

YuvFileFrameGenerator* YuvFileFrameGenerator::Create(const char* file,
                                                     size_t width,
                                                     size_t height,
                                                     Clock* clock) {
  FILE* file_handle = fopen(file, "r");
  if (file_handle == NULL) {
    return NULL;
  }
  return new YuvFileFrameGenerator(file_handle, width, height, clock);
}

YuvFileFrameGenerator::YuvFileFrameGenerator(FILE* file,
                                             size_t width,
                                             size_t height,
                                             Clock* clock)
    : FrameGenerator(width, height, clock), file_(file) {
  frame_size_ = CalcBufferSize(kI420, width_, height_);
  frame_buffer_ = new uint8_t[frame_size_];
}

YuvFileFrameGenerator::~YuvFileFrameGenerator() {
  fclose(file_);
  delete[] frame_buffer_;
}

void YuvFileFrameGenerator::GenerateNextFrame() {
  size_t count = fread(frame_buffer_, 1, frame_size_, file_);
  if (count < frame_size_) {
    rewind(file_);
    return;
  }

  ConvertToI420(kI420, frame_buffer_, 0, 0, width_, height_, frame_size_,
                kRotateNone, &frame_);
}
}  // namespace test
}  // namespace webrtc
