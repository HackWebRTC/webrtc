/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/mouse_cursor.h"
#include "webrtc/modules/desktop_capture/win/cursor.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

class MouseCursorMonitorWin : public MouseCursorMonitor {
 public:
  explicit MouseCursorMonitorWin(HWND window);
  virtual ~MouseCursorMonitorWin();

  virtual void Init(Callback* callback, Mode mode) OVERRIDE;
  virtual void Capture() OVERRIDE;

 private:
  HWND window_;

  Callback* callback_;
  Mode mode_;

  HDC desktop_dc_;

  HCURSOR last_cursor_;
};

MouseCursorMonitorWin::MouseCursorMonitorWin(HWND window)
    : window_(window),
      callback_(NULL),
      mode_(SHAPE_AND_POSITION),
      desktop_dc_(NULL),
      last_cursor_(NULL) {
}

MouseCursorMonitorWin::~MouseCursorMonitorWin() {
  if (desktop_dc_)
    ReleaseDC(NULL, desktop_dc_);
}

void MouseCursorMonitorWin::Init(Callback* callback, Mode mode) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
  mode_ = mode;

  desktop_dc_ = GetDC(NULL);
}

void MouseCursorMonitorWin::Capture() {
  assert(callback_);

  CURSORINFO cursor_info;
  cursor_info.cbSize = sizeof(CURSORINFO);
  if (!GetCursorInfo(&cursor_info)) {
    LOG_F(LS_ERROR) << "Unable to get cursor info. Error = " << GetLastError();
    return;
  }

  if (last_cursor_ != cursor_info.hCursor) {
    last_cursor_ = cursor_info.hCursor;
    // Note that |cursor_info.hCursor| does not need to be freed.
    scoped_ptr<MouseCursor> cursor(
        CreateMouseCursorFromHCursor(desktop_dc_, cursor_info.hCursor));
    if (cursor.get())
      callback_->OnMouseCursor(cursor.release());
  }

  if (mode_ != SHAPE_AND_POSITION)
    return;

  DesktopVector position(cursor_info.ptScreenPos.x, cursor_info.ptScreenPos.y);
  bool inside = cursor_info.flags == CURSOR_SHOWING;

  if (window_) {
    RECT rect;
    if (!GetWindowRect(window_, &rect)) {
      position.set(0, 0);
      inside = false;
    } else {
      position = position.subtract(DesktopVector(rect.left, rect.top));
      if (inside)
        inside = (window_ == WindowFromPoint(cursor_info.ptScreenPos));
    }
  }

  callback_->OnMouseCursorPosition(inside ? INSIDE : OUTSIDE, position);
}

MouseCursorMonitor* MouseCursorMonitor::CreateForWindow(
    const DesktopCaptureOptions& options, WindowId window) {
  return new MouseCursorMonitorWin(reinterpret_cast<HWND>(window));
}

MouseCursorMonitor* MouseCursorMonitor::CreateForScreen(
    const DesktopCaptureOptions& options) {
  return new MouseCursorMonitorWin(NULL);
}

}  // namespace webrtc
