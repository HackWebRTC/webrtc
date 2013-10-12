/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/x11/shared_x_display.h"

#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

SharedXDisplay::SharedXDisplay(Display* display)
  : display_(display) {
  assert(display_);
}

SharedXDisplay::~SharedXDisplay() {
  XCloseDisplay(display_);
}

// static
scoped_refptr<SharedXDisplay> SharedXDisplay::Create(
    const std::string& display_name) {
  Display* display =
      XOpenDisplay(display_name.empty() ? NULL : display_name.c_str());
  if (!display) {
    LOG(LS_ERROR) << "Unable to open display";
    return NULL;
  }
  return new SharedXDisplay(display);
}

// static
scoped_refptr<SharedXDisplay> SharedXDisplay::CreateDefault() {
  return Create(std::string());
}

}  // namespace webrtc
