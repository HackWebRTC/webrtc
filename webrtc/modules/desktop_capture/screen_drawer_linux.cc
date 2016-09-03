/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/base/checks.h"
#include "webrtc/modules/desktop_capture/screen_drawer.h"
#include "webrtc/modules/desktop_capture/x11/shared_x_display.h"
#include "webrtc/system_wrappers/include/sleep.h"

namespace webrtc {

namespace {

// A ScreenDrawer implementation for X11.
class ScreenDrawerLinux : public ScreenDrawer {
 public:
  ScreenDrawerLinux();
  ~ScreenDrawerLinux() override;

  // ScreenDrawer interface.
  DesktopRect DrawableRegion() override;
  void DrawRectangle(DesktopRect rect, RgbaColor color) override;
  void Clear() override;
  void WaitForPendingDraws() override;

 private:
  rtc::scoped_refptr<SharedXDisplay> display_;
  int screen_num_;
  DesktopRect rect_;
  Window window_;
  GC context_;
  Colormap colormap_;
};

ScreenDrawerLinux::ScreenDrawerLinux() {
  display_ = SharedXDisplay::CreateDefault();
  RTC_CHECK(display_.get());
  screen_num_ = DefaultScreen(display_->display());
  XWindowAttributes root_attributes;
  if (!XGetWindowAttributes(display_->display(),
                            RootWindow(display_->display(), screen_num_),
                            &root_attributes)) {
    RTC_DCHECK(false) << "Failed to get root window size.";
  }
  window_ = XCreateSimpleWindow(
      display_->display(), RootWindow(display_->display(), screen_num_), 0, 0,
      root_attributes.width, root_attributes.height, 0,
      BlackPixel(display_->display(), screen_num_),
      BlackPixel(display_->display(), screen_num_));
  XSelectInput(display_->display(), window_, StructureNotifyMask);
  XMapWindow(display_->display(), window_);
  while (true) {
    XEvent event;
    XNextEvent(display_->display(), &event);
    if (event.type == MapNotify) {
      break;
    }
  }
  XFlush(display_->display());
  Window child;
  int x, y;
  if (!XTranslateCoordinates(display_->display(), window_,
                             RootWindow(display_->display(), screen_num_), 0, 0,
                             &x, &y, &child)) {
    RTC_DCHECK(false) << "Failed to get window position.";
  }
  // Some window manager does not allow a window to cover two or more monitors.
  // So if the window is on the first monitor of a two-monitor system, the
  // second half won't be able to show up without changing configurations of WM,
  // and its DrawableRegion() is not accurate.
  rect_ = DesktopRect::MakeLTRB(x, y, root_attributes.width,
                                root_attributes.height);
  context_ = DefaultGC(display_->display(), screen_num_);
  colormap_ = DefaultColormap(display_->display(), screen_num_);
  // Wait for window animations.
  SleepMs(200);
}

ScreenDrawerLinux::~ScreenDrawerLinux() {
  XUnmapWindow(display_->display(), window_);
  XDestroyWindow(display_->display(), window_);
}

DesktopRect ScreenDrawerLinux::DrawableRegion() {
  return rect_;
}

void ScreenDrawerLinux::DrawRectangle(DesktopRect rect, RgbaColor color) {
  rect.Translate(-rect_.left(), -rect_.top());
  XColor xcolor;
  // X11 does not support Alpha.
  // X11 uses 16 bits for each primary color, so we need to slightly normalize
  // a 8 bits channel to 16 bits channel, by setting the low 8 bits as its high
  // 8 bits to avoid a mismatch of color returned by capturer.
  xcolor.red = (color.red << 8) + color.red;
  xcolor.green = (color.green << 8) + color.green;
  xcolor.blue = (color.blue << 8) + color.blue;
  xcolor.flags = DoRed | DoGreen | DoBlue;
  XAllocColor(display_->display(), colormap_, &xcolor);
  XSetForeground(display_->display(), context_, xcolor.pixel);
  XFillRectangle(display_->display(), window_, context_, rect.left(),
                 rect.top(), rect.width(), rect.height());
  XFlush(display_->display());
}

void ScreenDrawerLinux::Clear() {
  DrawRectangle(rect_, RgbaColor(0, 0, 0));
}

// TODO(zijiehe): Find the right signal from X11 to indicate the finish of all
// pending paintings.
void ScreenDrawerLinux::WaitForPendingDraws() {
  SleepMs(50);
}

}  // namespace

// static
std::unique_ptr<ScreenDrawer> ScreenDrawer::Create() {
  if (SharedXDisplay::CreateDefault().get()) {
    return std::unique_ptr<ScreenDrawer>(new ScreenDrawerLinux());
  }
  return nullptr;
}

}  // namespace webrtc
