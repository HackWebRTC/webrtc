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

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/win32window.h"

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////
// Win32Window
///////////////////////////////////////////////////////////////////////////////

static const wchar_t kWindowBaseClassName[] = L"WindowBaseClass";
HINSTANCE Win32Window::instance_ = NULL;
ATOM Win32Window::window_class_ = 0;

Win32Window::Win32Window() : wnd_(NULL) {
}

Win32Window::~Win32Window() {
  ASSERT(NULL == wnd_);
}

bool Win32Window::Create(HWND parent, const wchar_t* title, DWORD style,
                         DWORD exstyle, int x, int y, int cx, int cy) {
  if (wnd_) {
    // Window already exists.
    return false;
  }

  if (!window_class_) {
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&Win32Window::WndProc),
                           &instance_)) {
      LOG_GLE(LS_ERROR) << "GetModuleHandleEx failed";
      return false;
    }

    // Class not registered, register it.
    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.hInstance = instance_;
    wcex.lpfnWndProc = &Win32Window::WndProc;
    wcex.lpszClassName = kWindowBaseClassName;
    window_class_ = ::RegisterClassEx(&wcex);
    if (!window_class_) {
      LOG_GLE(LS_ERROR) << "RegisterClassEx failed";
      return false;
    }
  }
  wnd_ = ::CreateWindowEx(exstyle, kWindowBaseClassName, title, style,
                          x, y, cx, cy, parent, NULL, instance_, this);
  return (NULL != wnd_);
}

void Win32Window::Destroy() {
  VERIFY(::DestroyWindow(wnd_) != FALSE);
}

void Win32Window::Shutdown() {
  if (window_class_) {
    ::UnregisterClass(MAKEINTATOM(window_class_), instance_);
    window_class_ = 0;
  }
}

bool Win32Window::OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam,
                            LRESULT& result) {
  switch (uMsg) {
  case WM_CLOSE:
    if (!OnClose()) {
      result = 0;
      return true;
    }
    break;
  }
  return false;
}

LRESULT Win32Window::WndProc(HWND hwnd, UINT uMsg,
                             WPARAM wParam, LPARAM lParam) {
  Win32Window* that = reinterpret_cast<Win32Window*>(
      ::GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (!that && (WM_CREATE == uMsg)) {
    CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
    that = static_cast<Win32Window*>(cs->lpCreateParams);
    that->wnd_ = hwnd;
    ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(that));
  }
  if (that) {
    LRESULT result;
    bool handled = that->OnMessage(uMsg, wParam, lParam, result);
    if (WM_DESTROY == uMsg) {
      for (HWND child = ::GetWindow(hwnd, GW_CHILD); child;
           child = ::GetWindow(child, GW_HWNDNEXT)) {
        LOG(LS_INFO) << "Child window: " << static_cast<void*>(child);
      }
    }
    if (WM_NCDESTROY == uMsg) {
      ::SetWindowLongPtr(hwnd, GWLP_USERDATA, NULL);
      that->wnd_ = NULL;
      that->OnNcDestroy();
    }
    if (handled) {
      return result;
    }
  }
  return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}

}  // namespace talk_base
