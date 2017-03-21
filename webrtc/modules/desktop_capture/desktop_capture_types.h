/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURE_TYPES_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURE_TYPES_H_

#include <stdint.h>

#include "webrtc/media/base/videocommon.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Type used to identify windows on the desktop. Values are platform-specific:
//   - On Windows: HWND cast to intptr_t.
//   - On Linux (with X11): X11 Window (unsigned long) type cast to intptr_t.
//   - On OSX: integer window number.
typedef intptr_t WindowId;

const WindowId kNullWindowId = 0;

// Type used to identify screens on the desktop. Values are platform-specific:
//   - On Windows: integer display device index.
//   - On OSX: CGDirectDisplayID cast to intptr_t.
//   - On Linux (with X11): TBD.
typedef intptr_t ScreenId;

// The screen id corresponds to all screen combined together.
const ScreenId kFullDesktopScreenId = -1;

const ScreenId kInvalidScreenId = -2;

// An integer to attach to each DesktopFrame to differentiate the generator of
// the frame.
namespace DesktopCapturerId {
  constexpr uint32_t kUnknown = 0;
  constexpr uint32_t kScreenCapturerWinGdi = FOURCC('G', 'D', 'I', ' ');
  constexpr uint32_t kScreenCapturerWinDirectx = FOURCC('D', 'X', 'G', 'I');
}  // namespace DesktopCapturerId

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURE_TYPES_H_
