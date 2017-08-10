/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_X11_WINDOW_LIST_UTILS_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_X11_WINDOW_LIST_UTILS_H_

#include <X11/Xlib.h>

#include "webrtc/modules/desktop_capture/x11/x_atom_cache.h"
#include "webrtc/rtc_base/function_view.h"

namespace webrtc {

// Synchronously iterates all on-screen windows in |cache|.display() in
// decreasing z-order and sends them one-by-one to |on_window| function before
// GetWindowList() returns. If |on_window| returns false, this function ignores
// other windows and returns immediately. GetWindowList() returns false if
// native APIs failed. If multiple screens are attached to the |display|, this
// function returns false only when native APIs failed on all screens. Menus,
// panels and minimized windows will be ignored.
bool GetWindowList(XAtomCache* cache,
                   rtc::FunctionView<bool(::Window)> on_window);

// Returns WM_STATE property of the |window|. This function returns
// WithdrawnState if the |window| is missing.
int32_t GetWindowState(XAtomCache* cache, ::Window window);

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_X11_WINDOW_LIST_UTILS_H_
