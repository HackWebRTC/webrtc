/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/window_finder_mac.h"

#include <CoreFoundation/CoreFoundation.h>

#include "webrtc/modules/desktop_capture/mac/window_list_utils.h"

namespace webrtc {

WindowFinderMac::WindowFinderMac() = default;
WindowFinderMac::~WindowFinderMac() = default;

WindowId WindowFinderMac::GetWindowUnderPoint(DesktopVector point) {
  WindowId id = kNullWindowId;
  GetWindowList([&id, point](CFDictionaryRef window) {
                  DesktopRect bounds = GetWindowBounds(window);
                  if (bounds.Contains(point)) {
                    id = GetWindowId(window);
                    return false;
                  }
                  return true;
                },
                true);
  return id;
}

}  // namespace webrtc
