/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_FAKE_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_FAKE_H_

#include "webrtc/modules/desktop_capture/desktop_geometry.h"
#include "webrtc/modules/desktop_capture/screen_capture_frame_queue.h"
#include "webrtc/modules/desktop_capture/screen_capturer.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

// A ScreenCapturerFake generates artificial image for testing purpose.
//
// ScreenCapturerFake is double-buffered as required by ScreenCapturer.
class ScreenCapturerFake : public ScreenCapturer {
 public:
  // ScreenCapturerFake generates a picture of size kWidth x kHeight.
  static const int kWidth = 800;
  static const int kHeight = 600;

  ScreenCapturerFake();
  virtual ~ScreenCapturerFake();

  // DesktopCapturer interface.
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const DesktopRegion& rect) OVERRIDE;

  // ScreenCapturer interface.
  virtual void SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) OVERRIDE;

 private:
  // Generates an image in the front buffer.
  void GenerateImage();

  // Called when the screen configuration is changed.
  void ScreenConfigurationChanged();

  Callback* callback_;
  MouseShapeObserver* mouse_shape_observer_;

  DesktopSize size_;
  int bytes_per_row_;
  int box_pos_x_;
  int box_pos_y_;
  int box_speed_x_;
  int box_speed_y_;

  ScreenCaptureFrameQueue queue_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCapturerFake);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_FAKE_H_
