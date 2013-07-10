// Copyright 2010 Google Inc. All Rights Reserved


#ifndef TALK_BASE_MACWINDOWPICKER_H_
#define TALK_BASE_MACWINDOWPICKER_H_

#include "talk/base/windowpicker.h"

namespace talk_base {

class MacWindowPicker : public WindowPicker {
 public:
  MacWindowPicker();
  ~MacWindowPicker();
  virtual bool Init();
  virtual bool IsVisible(const WindowId& id);
  virtual bool MoveToFront(const WindowId& id);
  virtual bool GetWindowList(WindowDescriptionList* descriptions);
  virtual bool GetDesktopList(DesktopDescriptionList* descriptions);
  virtual bool GetDesktopDimensions(const DesktopId& id, int* width,
                                    int* height);

 private:
  void* lib_handle_;
  void* get_window_list_;
  void* get_window_list_desc_;
};

}  // namespace talk_base

#endif  // TALK_BASE_MACWINDOWPICKER_H_
