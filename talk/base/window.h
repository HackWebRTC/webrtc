/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_BASE_WINDOW_H_
#define TALK_BASE_WINDOW_H_

#include "talk/base/stringencode.h"

// Define platform specific window types.
#if defined(LINUX)
typedef unsigned long Window;  // Avoid include <X11/Xlib.h>.
#elif defined(WIN32)
// We commonly include win32.h in talk/base so just include it here.
#include "talk/base/win32.h"  // Include HWND, HMONITOR.
#elif defined(OSX)
typedef unsigned int CGWindowID;
typedef unsigned int CGDirectDisplayID;
#endif

namespace talk_base {

class WindowId {
 public:
  // Define WindowT for each platform.
#if defined(LINUX)
  typedef Window WindowT;
#elif defined(WIN32)
  typedef HWND WindowT;
#elif defined(OSX)
  typedef CGWindowID WindowT;
#else
  typedef unsigned int WindowT;
#endif

  static WindowId Cast(uint64 id) {
#if defined(WIN32)
    return WindowId(reinterpret_cast<WindowId::WindowT>(id));
#else
    return WindowId(static_cast<WindowId::WindowT>(id));
#endif
  }

  static uint64 Format(const WindowT& id) {
#if defined(WIN32)
    return static_cast<uint64>(reinterpret_cast<uintptr_t>(id));
#else
    return static_cast<uint64>(id);
#endif
  }

  WindowId() : id_(0) {}
  WindowId(const WindowT& id) : id_(id) {}  // NOLINT
  const WindowT& id() const { return id_; }
  bool IsValid() const { return id_ != 0; }
  bool Equals(const WindowId& other) const {
    return id_ == other.id();
  }

 private:
  WindowT id_;
};

class DesktopId {
 public:
  // Define DesktopT for each platform.
#if defined(LINUX)
  typedef Window DesktopT;
#elif defined(WIN32)
  typedef HMONITOR DesktopT;
#elif defined(OSX)
  typedef CGDirectDisplayID DesktopT;
#else
  typedef unsigned int DesktopT;
#endif

  static DesktopId Cast(int id, int index) {
#if defined(WIN32)
    return DesktopId(reinterpret_cast<DesktopId::DesktopT>(id), index);
#else
    return DesktopId(static_cast<DesktopId::DesktopT>(id), index);
#endif
  }

  DesktopId() : id_(0), index_(-1) {}
  DesktopId(const DesktopT& id, int index)  // NOLINT
      : id_(id), index_(index) {
  }
  const DesktopT& id() const { return id_; }
  int index() const { return index_; }
  bool IsValid() const { return index_ != -1; }
  bool Equals(const DesktopId& other) const {
    return id_ == other.id() && index_ == other.index();
  }

 private:
  // Id is the platform specific desktop identifier.
  DesktopT id_;
  // Index is the desktop index as enumerated by each platform.
  // Desktop capturer typically takes the index instead of id.
  int index_;
};

// Window event types.
enum WindowEvent {
  WE_RESIZE = 0,
  WE_CLOSE = 1,
  WE_MINIMIZE = 2,
  WE_RESTORE = 3,
};

inline std::string ToString(const WindowId& window) {
  return ToString(window.id());
}

}  // namespace talk_base

#endif  // TALK_BASE_WINDOW_H_
