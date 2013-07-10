// Copyright 2010 Google Inc. All Rights Reserved


#ifndef TALK_BASE_WIN32WINDOWPICKER_H_
#define TALK_BASE_WIN32WINDOWPICKER_H_

#include "talk/base/win32.h"
#include "talk/base/windowpicker.h"

namespace talk_base {

class Win32WindowPicker : public WindowPicker {
 public:
  Win32WindowPicker();
  virtual bool Init();
  virtual bool IsVisible(const WindowId& id);
  virtual bool MoveToFront(const WindowId& id);
  virtual bool GetWindowList(WindowDescriptionList* descriptions);
  virtual bool GetDesktopList(DesktopDescriptionList* descriptions);
  virtual bool GetDesktopDimensions(const DesktopId& id, int* width,
                                    int* height);

 protected:
  static BOOL CALLBACK EnumProc(HWND hwnd, LPARAM l_param);
  static BOOL CALLBACK MonitorEnumProc(HMONITOR h_monitor,
                                       HDC hdc_monitor,
                                       LPRECT lprc_monitor,
                                       LPARAM l_param);
};

}  // namespace talk_base

#endif  // TALK_BASE_WIN32WINDOWPICKER_H_
