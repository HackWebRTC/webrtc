/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/frame_utils.h"
#include "webrtc/video_frame.h"

namespace webrtc {
namespace test {

bool EqualPlane(const uint8_t* data1,
                const uint8_t* data2,
                int stride,
                int width,
                int height) {
  for (int y = 0; y < height; ++y) {
    if (memcmp(data1, data2, width) != 0)
      return false;
    data1 += stride;
    data2 += stride;
  }
  return true;
}
bool FramesEqual(const webrtc::VideoFrame& f1, const webrtc::VideoFrame& f2) {
  if (f1.width() != f2.width() || f1.height() != f2.height() ||
      f1.stride(webrtc::kYPlane) != f2.stride(webrtc::kYPlane) ||
      f1.stride(webrtc::kUPlane) != f2.stride(webrtc::kUPlane) ||
      f1.stride(webrtc::kVPlane) != f2.stride(webrtc::kVPlane) ||
      f1.timestamp() != f2.timestamp() ||
      f1.ntp_time_ms() != f2.ntp_time_ms() ||
      f1.render_time_ms() != f2.render_time_ms()) {
    return false;
  }
  const int half_width = (f1.width() + 1) / 2;
  const int half_height = (f1.height() + 1) / 2;
  return EqualPlane(f1.buffer(webrtc::kYPlane), f2.buffer(webrtc::kYPlane),
                    f1.stride(webrtc::kYPlane), f1.width(), f1.height()) &&
         EqualPlane(f1.buffer(webrtc::kUPlane), f2.buffer(webrtc::kUPlane),
                    f1.stride(webrtc::kUPlane), half_width, half_height) &&
         EqualPlane(f1.buffer(webrtc::kVPlane), f2.buffer(webrtc::kVPlane),
                    f1.stride(webrtc::kVPlane), half_width, half_height);
}

}  // namespace test
}  // namespace webrtc
