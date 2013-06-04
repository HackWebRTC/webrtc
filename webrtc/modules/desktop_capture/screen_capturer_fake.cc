/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/screen_capturer_fake.h"

#include <string.h>

#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/system_wrappers/interface/compile_assert.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {

// ScreenCapturerFake generates a white picture of size kWidth x kHeight
// with a rectangle of size kBoxWidth x kBoxHeight. The rectangle moves kSpeed
// pixels per frame along both axes, and bounces off the sides of the screen.
static const int kWidth = ScreenCapturerFake::kWidth;
static const int kHeight = ScreenCapturerFake::kHeight;
static const int kBoxWidth = 140;
static const int kBoxHeight = 140;
static const int kSpeed = 20;

ScreenCapturerFake::ScreenCapturerFake()
    : callback_(NULL),
      mouse_shape_observer_(NULL),
      bytes_per_row_(0),
      box_pos_x_(0),
      box_pos_y_(0),
      box_speed_x_(kSpeed),
      box_speed_y_(kSpeed) {

  COMPILE_ASSERT(kBoxWidth < kWidth && kBoxHeight < kHeight);
  COMPILE_ASSERT((kBoxWidth % kSpeed == 0) && (kWidth % kSpeed == 0) &&
                 (kBoxHeight % kSpeed == 0) && (kHeight % kSpeed == 0));

  ScreenConfigurationChanged();
}

ScreenCapturerFake::~ScreenCapturerFake() {
}

void ScreenCapturerFake::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);
  callback_ = callback;
}

void ScreenCapturerFake::Capture(const DesktopRegion& region) {
  TickTime capture_start_time = TickTime::Now();

  queue_.MoveToNextFrame();

  if (!queue_.current_frame()) {
    int buffer_size = size_.height() * bytes_per_row_;
    SharedMemory* shared_memory =
        callback_->CreateSharedMemory(buffer_size);
    scoped_ptr<DesktopFrame> frame;
    DesktopSize frame_size(size_.width(), size_.height());
    if (shared_memory) {
      frame.reset(new SharedMemoryDesktopFrame(
          frame_size, bytes_per_row_, shared_memory));
    } else {
      frame.reset(new BasicDesktopFrame(frame_size));
    }
    queue_.ReplaceCurrentFrame(frame.release());
  }

  assert(queue_.current_frame());
  GenerateImage();

  queue_.current_frame()->mutable_updated_region()->SetRect(
      DesktopRect::MakeSize(size_));
  queue_.current_frame()->set_capture_time_ms(
      (TickTime::Now() - capture_start_time).Milliseconds());

  callback_->OnCaptureCompleted(queue_.current_frame()->Share());
}

void ScreenCapturerFake::SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) {
  assert(!mouse_shape_observer_);
  assert(mouse_shape_observer);
  mouse_shape_observer_ = mouse_shape_observer;
}

void ScreenCapturerFake::GenerateImage() {
  DesktopFrame* frame = queue_.current_frame();

  const int kBytesPerPixel = DesktopFrame::kBytesPerPixel;

  memset(frame->data(), 0xff,
         size_.width() * size_.height() * kBytesPerPixel);

  uint8_t* row = frame->data() +
      (box_pos_y_ * size_.width() + box_pos_x_) * kBytesPerPixel;

  box_pos_x_ += box_speed_x_;
  if (box_pos_x_ + kBoxWidth >= size_.width() || box_pos_x_ == 0)
    box_speed_x_ = -box_speed_x_;

  box_pos_y_ += box_speed_y_;
  if (box_pos_y_ + kBoxHeight >= size_.height() || box_pos_y_ == 0)
    box_speed_y_ = -box_speed_y_;

  // Draw rectangle with the following colors in its corners:
  //     cyan....yellow
  //     ..............
  //     blue.......red
  for (int y = 0; y < kBoxHeight; ++y) {
    for (int x = 0; x < kBoxWidth; ++x) {
      int r = x * 255 / kBoxWidth;
      int g = y * 255 / kBoxHeight;
      int b = 255 - (x * 255 / kBoxWidth);
      row[x * kBytesPerPixel] = r;
      row[x * kBytesPerPixel + 1] = g;
      row[x * kBytesPerPixel + 2] = b;
      row[x * kBytesPerPixel + 3] = 0xff;
    }
    row += bytes_per_row_;
  }
}

void ScreenCapturerFake::ScreenConfigurationChanged() {
  size_.set(kWidth, kHeight);
  queue_.Reset();
  bytes_per_row_ = size_.width() * DesktopFrame::kBytesPerPixel;
}

}  // namespace webrtc
