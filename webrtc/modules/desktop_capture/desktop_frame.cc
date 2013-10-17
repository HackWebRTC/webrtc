/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/desktop_frame.h"

#include <string.h>

namespace webrtc {

DesktopFrame::DesktopFrame(DesktopSize size,
                           int stride,
                           uint8_t* data,
                           SharedMemory* shared_memory)
    : size_(size),
      stride_(stride),
      data_(data),
      shared_memory_(shared_memory),
      capture_time_ms_(0) {
}

DesktopFrame::~DesktopFrame() {}

BasicDesktopFrame::BasicDesktopFrame(DesktopSize size)
    : DesktopFrame(size, kBytesPerPixel * size.width(),
                   new uint8_t[kBytesPerPixel * size.width() * size.height()],
                   NULL) {
}

BasicDesktopFrame::~BasicDesktopFrame() {
  delete[] data_;
}

DesktopFrame* BasicDesktopFrame::CopyOf(const DesktopFrame& frame) {
  DesktopFrame* result = new BasicDesktopFrame(frame.size());
  for (int y = 0; y < frame.size().height(); ++y) {
    memcpy(result->data() + y * result->stride(),
           frame.data() + y * frame.stride(),
           frame.size().width() * kBytesPerPixel);
  }
  result->set_dpi(frame.dpi());
  result->set_capture_time_ms(frame.capture_time_ms());
  *result->mutable_updated_region() = frame.updated_region();
  return result;
}


SharedMemoryDesktopFrame::SharedMemoryDesktopFrame(
    DesktopSize size,
    int stride,
    SharedMemory* shared_memory)
    : DesktopFrame(size, stride,
                   reinterpret_cast<uint8_t*>(shared_memory->data()),
                   shared_memory) {
}

SharedMemoryDesktopFrame::~SharedMemoryDesktopFrame() {
  delete shared_memory_;
}

}  // namespace webrtc
