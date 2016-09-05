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

namespace webrtc {

namespace {

// A ScreenDrawer implementation for X11.
class ScreenDrawerLinux : public ScreenDrawer {
 public:
  ScreenDrawerLinux();
  ~ScreenDrawerLinux() override;

  // ScreenDrawer interface.
  DesktopRect DrawableRegion() override;
  void DrawRectangle(DesktopRect rect, uint32_t rgba) override;
  void Clear() override;

 private:
  rtc::scoped_refptr<SharedXDisplay> display_;
  Screen* screen_;
  int screen_num_;
  DesktopRect rect_;
  Window window_;
  GC context_;
  Colormap colormap_;
};

ScreenDrawerLinux::ScreenDrawerLinux() {
  display_ = SharedXDisplay::CreateDefault();
  RTC_CHECK(display_.get());
  screen_ = DefaultScreenOfDisplay(display_->display());
  RTC_CHECK(screen_);
  screen_num_ = DefaultScreen(display_->display());
  rect_ = DesktopRect::MakeWH(screen_->width, screen_->height);
  window_ = XCreateSimpleWindow(display_->display(),
                                RootWindow(display_->display(), screen_num_), 0,
                                0, rect_.width(), rect_.height(), 0,
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
  context_ = DefaultGC(display_->display(), screen_num_);
  colormap_ = DefaultColormap(display_->display(), screen_num_);
}

ScreenDrawerLinux::~ScreenDrawerLinux() {
  XUnmapWindow(display_->display(), window_);
  XDestroyWindow(display_->display(), window_);
}

DesktopRect ScreenDrawerLinux::DrawableRegion() {
  return rect_;
}

void ScreenDrawerLinux::DrawRectangle(DesktopRect rect, uint32_t rgba) {
  int r = (rgba & 0xff00) >> 8;
  int g = (rgba & 0xff0000) >> 16;
  int b = (rgba & 0xff000000) >> 24;
  // X11 does not support Alpha.
  XColor color;
  // X11 uses 16 bits for each primary color.
  color.red = r * 256;
  color.green = g * 256;
  color.blue = b * 256;
  color.flags = DoRed | DoGreen | DoBlue;
  XAllocColor(display_->display(), colormap_, &color);
  XSetForeground(display_->display(), context_, color.pixel);
  XFillRectangle(display_->display(), window_, context_, rect.left(),
                 rect.top(), rect.width(), rect.height());
  XFlush(display_->display());
}

void ScreenDrawerLinux::Clear() {
  DrawRectangle(DrawableRegion(), 0);
}

}  // namespace

// static
std::unique_ptr<ScreenDrawer> ScreenDrawer::Create() {
  return std::unique_ptr<ScreenDrawer>(new ScreenDrawerLinux());
}

}  // namespace webrtc
