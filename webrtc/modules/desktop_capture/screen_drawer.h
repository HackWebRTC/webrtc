/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_DRAWER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_DRAWER_H_

#include <stdint.h>

#include <memory>

#include "webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

// A set of platform independent functions to draw various of shapes on the
// screen. This class is for testing ScreenCapturer* implementations only, and
// should not be used in production logic.
class ScreenDrawer {
 public:
  // Creates a ScreenDrawer for the current platform.
  static std::unique_ptr<ScreenDrawer> Create();

  ScreenDrawer() {}
  virtual ~ScreenDrawer() {}

  // Returns a rect, on which this instance can draw.
  virtual DesktopRect DrawableRegion() = 0;

  // Draws a rectangle to cover |rect| with color |rgba|. Note, rect.bottom()
  // and rect.right() two lines are not included.
  virtual void DrawRectangle(DesktopRect rect, uint32_t rgba) = 0;

  // Clears all content on the screen.
  virtual void Clear() = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_DRAWER_H_
