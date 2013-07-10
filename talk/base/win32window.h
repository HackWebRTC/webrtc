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

#ifndef TALK_BASE_WIN32WINDOW_H_
#define TALK_BASE_WIN32WINDOW_H_

#ifdef WIN32

#include "talk/base/win32.h"

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////
// Win32Window
///////////////////////////////////////////////////////////////////////////////

class Win32Window {
 public:
  Win32Window();
  virtual ~Win32Window();

  HWND handle() const { return wnd_; }

  bool Create(HWND parent, const wchar_t* title, DWORD style, DWORD exstyle,
              int x, int y, int cx, int cy);
  void Destroy();

  // Call this when your DLL unloads.
  static void Shutdown();

 protected:
  virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam,
                         LRESULT& result);

  virtual bool OnClose() { return true; }
  virtual void OnNcDestroy() { }

 private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam);

  HWND wnd_;
  static HINSTANCE instance_;
  static ATOM window_class_;
};

///////////////////////////////////////////////////////////////////////////////

}  // namespace talk_base

#endif  // WIN32

#endif  // TALK_BASE_WIN32WINDOW_H_
