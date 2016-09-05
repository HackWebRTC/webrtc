/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <windows.h>

#include <memory>

#include "webrtc/modules/desktop_capture/screen_drawer.h"

namespace webrtc {

namespace {

DesktopRect GetScreenRect() {
  HDC hdc = GetDC(NULL);
  DesktopRect rect = DesktopRect::MakeWH(GetDeviceCaps(hdc, HORZRES),
                                         GetDeviceCaps(hdc, VERTRES));
  ReleaseDC(NULL, hdc);
  return rect;
}

HWND CreateDrawerWindow(DesktopRect rect) {
  HWND hwnd = CreateWindowA(
      "STATIC", "DrawerWindow", WS_POPUPWINDOW | WS_VISIBLE, rect.left(),
      rect.top(), rect.width(), rect.height(), NULL, NULL, NULL, NULL);
  SetForegroundWindow(hwnd);
  return hwnd;
}

// A ScreenDrawer implementation for Windows.
class ScreenDrawerWin : public ScreenDrawer {
 public:
  ScreenDrawerWin();
  ~ScreenDrawerWin() override;

  // ScreenDrawer interface.
  DesktopRect DrawableRegion() override;
  void DrawRectangle(DesktopRect rect, uint32_t rgba) override;
  void Clear() override;

 private:
  const DesktopRect rect_;
  HWND window_;
  HDC hdc_;
};

ScreenDrawerWin::ScreenDrawerWin()
    : ScreenDrawer(),
      rect_(GetScreenRect()),
      window_(CreateDrawerWindow(rect_)),
      hdc_(GetWindowDC(window_)) {
  // We do not need to handle any messages for the |window_|, so disable Windows
  // process windows ghosting feature.
  DisableProcessWindowsGhosting();
}

ScreenDrawerWin::~ScreenDrawerWin() {
  ReleaseDC(NULL, hdc_);
  DestroyWindow(window_);
  // Unfortunately there is no EnableProcessWindowsGhosting() API.
}

DesktopRect ScreenDrawerWin::DrawableRegion() {
  return rect_;
}

void ScreenDrawerWin::DrawRectangle(DesktopRect rect, uint32_t rgba) {
  int r = (rgba & 0xff00) >> 8;
  int g = (rgba & 0xff0000) >> 16;
  int b = (rgba & 0xff000000) >> 24;
  // Windows device context does not support Alpha.
  SelectObject(hdc_, GetStockObject(DC_PEN));
  SelectObject(hdc_, GetStockObject(DC_BRUSH));
  SetDCBrushColor(hdc_, RGB(r, g, b));
  SetDCPenColor(hdc_, RGB(r, g, b));
  Rectangle(hdc_, rect.left(), rect.top(), rect.right(), rect.bottom());
}

void ScreenDrawerWin::Clear() {
  DrawRectangle(DrawableRegion(), 0);
}

}  // namespace

// static
std::unique_ptr<ScreenDrawer> ScreenDrawer::Create() {
  return std::unique_ptr<ScreenDrawer>(new ScreenDrawerWin());
}

}  // namespace webrtc
