// Copyright 2010 Google Inc. All Rights Reserved


#include "talk/base/gunit.h"
#include "talk/base/logging.h"
#include "talk/base/macutils.h"
#include "talk/base/macwindowpicker.h"
#include "talk/base/windowpicker.h"

#ifndef OSX
#error Only for Mac OSX
#endif

namespace talk_base {

bool IsLeopardOrLater() {
  return GetOSVersionName() >= kMacOSLeopard;
}

// Test that this works on new versions and fails acceptably on old versions.
TEST(MacWindowPickerTest, TestGetWindowList) {
  MacWindowPicker picker, picker2;
  WindowDescriptionList descriptions;
  if (IsLeopardOrLater()) {
    EXPECT_TRUE(picker.Init());
    EXPECT_TRUE(picker.GetWindowList(&descriptions));
    EXPECT_TRUE(picker2.GetWindowList(&descriptions));  // Init is optional
  } else {
    EXPECT_FALSE(picker.Init());
    EXPECT_FALSE(picker.GetWindowList(&descriptions));
    EXPECT_FALSE(picker2.GetWindowList(&descriptions));
  }
}

// TODO: Add verification of the actual parsing, ie, add
// functionality to inject a fake get_window_array function which
// provide a pre-constructed list of windows.

}  // namespace talk_base
