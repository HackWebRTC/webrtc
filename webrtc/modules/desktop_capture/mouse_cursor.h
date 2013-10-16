/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_H_

#include "webrtc/modules/desktop_capture/desktop_geometry.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class DesktopFrame;

class MouseCursor {
 public:
  // Takes ownership of |image|. |hotspot| must be within |image| boundaries.
  MouseCursor(DesktopFrame* image, const DesktopVector& hotspot);
  ~MouseCursor();

  const DesktopFrame& image() { return *image_; }
  const DesktopVector& hotspot() { return hotspot_; }

 private:
  scoped_ptr<DesktopFrame> image_;
  DesktopVector hotspot_;

  DISALLOW_COPY_AND_ASSIGN(MouseCursor);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_H_
