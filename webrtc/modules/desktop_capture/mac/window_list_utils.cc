/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/mac/window_list_utils.h"

#include <ApplicationServices/ApplicationServices.h>

#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/macutils.h"

static_assert(
    static_cast<webrtc::WindowId>(kCGNullWindowID) == webrtc::kNullWindowId,
    "kNullWindowId needs to equal to kCGNullWindowID.");

namespace webrtc {

bool GetWindowList(rtc::FunctionView<bool(CFDictionaryRef)> on_window,
                   bool ignore_minimized) {
  RTC_DCHECK(on_window);

  // Only get on screen, non-desktop windows.
  // According to
  // https://developer.apple.com/documentation/coregraphics/cgwindowlistoption/1454105-optiononscreenonly ,
  // when kCGWindowListOptionOnScreenOnly is used, the order of windows are in
  // decreasing z-order.
  CFArrayRef window_array = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
      kCGNullWindowID);
  if (!window_array)
    return false;

  MacDesktopConfiguration desktop_config;
  if (ignore_minimized) {
    desktop_config = MacDesktopConfiguration::GetCurrent(
        MacDesktopConfiguration::TopLeftOrigin);
  }

  // Check windows to make sure they have an id, title, and use window layer
  // other than 0.
  CFIndex count = CFArrayGetCount(window_array);
  for (CFIndex i = 0; i < count; i++) {
    CFDictionaryRef window = reinterpret_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(window_array, i));
    if (!window) {
      continue;
    }

    CFStringRef window_title = reinterpret_cast<CFStringRef>(
        CFDictionaryGetValue(window, kCGWindowName));
    if (!window_title) {
      continue;
    }

    CFNumberRef window_id = reinterpret_cast<CFNumberRef>(
        CFDictionaryGetValue(window, kCGWindowNumber));
    if (!window_id) {
      continue;
    }

    CFNumberRef window_layer = reinterpret_cast<CFNumberRef>(
        CFDictionaryGetValue(window, kCGWindowLayer));
    if (!window_layer) {
      continue;
    }

    // Skip windows with layer=0 (menu, dock).
    // TODO(zijiehe): The windows with layer != 0 are skipped, is this a bug in
    // code (not likely) or a bug in comments? What's the meaning of window
    // layer number in the first place.
    int layer;
    if (!CFNumberGetValue(window_layer, kCFNumberIntType, &layer)) {
      continue;
    }
    if (layer != 0) {
      continue;
    }

    // Skip windows that are minimized and not full screen.
    if (ignore_minimized && !IsWindowOnScreen(window) &&
        !IsWindowFullScreen(desktop_config, window)) {
      continue;
    }

    if (!on_window(window)) {
      break;
    }
  }

  CFRelease(window_array);
  return true;
}

bool GetWindowList(DesktopCapturer::SourceList* windows,
                   bool ignore_minimized) {
  return GetWindowList(
      [windows](CFDictionaryRef window) {
        WindowId id = GetWindowId(window);
        std::string title = GetWindowTitle(window);
        if (id != kNullWindowId && !title.empty()) {
          windows->push_back(DesktopCapturer::Source{ id, title });
        }
        return true;
      },
      ignore_minimized);
}

// Returns true if the window is occupying a full screen.
bool IsWindowFullScreen(
    const MacDesktopConfiguration& desktop_config,
    CFDictionaryRef window) {
  bool fullscreen = false;
  CFDictionaryRef bounds_ref = reinterpret_cast<CFDictionaryRef>(
      CFDictionaryGetValue(window, kCGWindowBounds));

  CGRect bounds;
  if (bounds_ref &&
      CGRectMakeWithDictionaryRepresentation(bounds_ref, &bounds)) {
    for (MacDisplayConfigurations::const_iterator it =
             desktop_config.displays.begin();
         it != desktop_config.displays.end(); it++) {
      if (it->bounds.equals(DesktopRect::MakeXYWH(bounds.origin.x,
                                                  bounds.origin.y,
                                                  bounds.size.width,
                                                  bounds.size.height))) {
        fullscreen = true;
        break;
      }
    }
  }

  return fullscreen;
}

bool IsWindowOnScreen(CFDictionaryRef window) {
  CFBooleanRef on_screen = reinterpret_cast<CFBooleanRef>(
      CFDictionaryGetValue(window, kCGWindowIsOnscreen));
  return on_screen == NULL || CFBooleanGetValue(on_screen);
}

bool IsWindowOnScreen(CGWindowID id) {
  CFArrayRef window_id_array =
      CFArrayCreate(NULL, reinterpret_cast<const void **>(&id), 1, NULL);
  CFArrayRef window_array =
      CGWindowListCreateDescriptionFromArray(window_id_array);
  bool on_screen = true;

  if (window_array && CFArrayGetCount(window_array)) {
    on_screen = IsWindowOnScreen(reinterpret_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(window_array, 0)));
  }

  CFRelease(window_id_array);
  CFRelease(window_array);

  return on_screen;
}

std::string GetWindowTitle(CFDictionaryRef window) {
  CFStringRef title = reinterpret_cast<CFStringRef>(
      CFDictionaryGetValue(window, kCGWindowName));
  std::string result;
  if (title && rtc::ToUtf8(title, &result)) {
    return result;
  }
  return std::string();
}

WindowId GetWindowId(CFDictionaryRef window) {
  CFNumberRef window_id = reinterpret_cast<CFNumberRef>(
      CFDictionaryGetValue(window, kCGWindowNumber));
  if (!window_id) {
    return kNullWindowId;
  }

  WindowId id;
  if (!CFNumberGetValue(window_id, kCFNumberIntType, &id)) {
    return kNullWindowId;
  }

  return id;
}

DesktopRect GetWindowBounds(CFDictionaryRef window) {
  CFDictionaryRef window_bounds = reinterpret_cast<CFDictionaryRef>(
      CFDictionaryGetValue(window, kCGWindowBounds));
  if (!window_bounds) {
    return DesktopRect();
  }

  CGRect gc_window_rect;
  if (!CGRectMakeWithDictionaryRepresentation(window_bounds, &gc_window_rect)) {
    return DesktopRect();
  }

  return DesktopRect::MakeXYWH(gc_window_rect.origin.x,
                               gc_window_rect.origin.y,
                               gc_window_rect.size.width,
                               gc_window_rect.size.height);
}

}  // namespace webrtc
