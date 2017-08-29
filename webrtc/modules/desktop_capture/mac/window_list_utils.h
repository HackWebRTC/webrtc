/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_MAC_WINDOW_LIST_UTILS_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_MAC_WINDOW_LIST_UTILS_H_

#include <ApplicationServices/ApplicationServices.h>

#include "webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"
#include "webrtc/modules/desktop_capture/mac/desktop_configuration.h"
#include "webrtc/rtc_base/function_view.h"

namespace webrtc {

// Iterates all on-screen windows in decreasing z-order and sends them
// one-by-one to |on_window| function. If |on_window| returns false, this
// function returns immediately. GetWindowList() returns false if native APIs
// failed. Menus, dock, minimized windows (if |ignore_minimized| is true) and
// any windows which do not have a valid window id or title will be ignored.
bool GetWindowList(rtc::FunctionView<bool(CFDictionaryRef)> on_window,
                   bool ignore_minimized);

// Another helper function to get the on-screen windows.
bool GetWindowList(DesktopCapturer::SourceList* windows, bool ignore_minimized);

// Returns true if the window is occupying a full screen.
bool IsWindowFullScreen(const MacDesktopConfiguration& desktop_config,
                        CFDictionaryRef window);

// Returns true if the |window| is minimized.
bool IsWindowMinimized(CFDictionaryRef window);

// Returns true if the window is minimized.
bool IsWindowMinimized(CGWindowID id);

// Returns utf-8 encoded title of |window|. If |window| is not a window or no
// valid title can be retrieved, this function returns an empty string.
std::string GetWindowTitle(CFDictionaryRef window);

// Returns id of |window|. If |window| is not a window or the window id cannot
// be retrieved, this function returns kNullWindowId.
WindowId GetWindowId(CFDictionaryRef window);

// Returns the bounds of |window|. If |window| is not a window or the bounds
// cannot be retrieved, this function returns an empty DesktopRect. The returned
// DesktopRect is in system coordinate, i.e. the primary monitor always starts
// from (0, 0).
DesktopRect GetWindowBounds(CFDictionaryRef window);

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_MAC_WINDOW_LIST_UTILS_H_
