/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_CROPPING_WINDOW_CAPTURER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_CROPPING_WINDOW_CAPTURER_H_

#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/screen_capturer.h"
#include "webrtc/modules/desktop_capture/window_capturer.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

// WindowCapturer implementation that uses a screen capturer to capture the
// whole screen and crops the video frame to the window area when the captured
// window is on top.
class CroppingWindowCapturer : public WindowCapturer,
                               public DesktopCapturer::Callback {
 public:
  static WindowCapturer* Create(const DesktopCaptureOptions& options);
  virtual ~CroppingWindowCapturer();

  // DesktopCapturer implementation.
  virtual void Start(DesktopCapturer::Callback* callback) OVERRIDE;
  virtual void Capture(const DesktopRegion& region) OVERRIDE;
  virtual void SetExcludedWindow(WindowId window) OVERRIDE;

  // WindowCapturer implementation.
  virtual bool GetWindowList(WindowList* windows) OVERRIDE;
  virtual bool SelectWindow(WindowId id) OVERRIDE;
  virtual bool BringSelectedWindowToFront() OVERRIDE;

  // DesktopCapturer::Callback implementation, passed to |screen_capturer_| to
  // intercept the capture result.
  virtual SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;
  virtual void OnCaptureCompleted(DesktopFrame* frame) OVERRIDE;

 protected:
  explicit CroppingWindowCapturer(const DesktopCaptureOptions& options);

  // The platform implementation should override these methods.

  // Returns true if it is OK to capture the whole screen and crop to the
  // selected window, i.e. the selected window is opaque, rectangular, and not
  // occluded.
  virtual bool ShouldUseScreenCapturer() = 0;

  // Returns the window area relative to the top left of the virtual screen
  // within the bounds of the virtual screen.
  virtual DesktopRect GetWindowRectInVirtualScreen() = 0;

  WindowId selected_window() const { return selected_window_; }
  WindowId excluded_window() const { return excluded_window_; }

 private:
  DesktopCaptureOptions options_;
  DesktopCapturer::Callback* callback_;
  scoped_ptr<WindowCapturer> window_capturer_;
  scoped_ptr<ScreenCapturer> screen_capturer_;
  WindowId selected_window_;
  WindowId excluded_window_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_CROPPING_WINDOW_CAPTURER_H_

