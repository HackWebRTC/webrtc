/*
 * libjingle
 * Copyright 2010 Google Inc.
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

#ifndef TALK_BASE_WINDOWPICKERFACTORY_H_
#define TALK_BASE_WINDOWPICKERFACTORY_H_

#if defined(WIN32)
#include "talk/base/win32windowpicker.h"
#elif defined(OSX)
#include "talk/base/macutils.h"
#include "talk/base/macwindowpicker.h"
#elif defined(LINUX)
#include "talk/base/linuxwindowpicker.h"
#endif

#include "talk/base/windowpicker.h"

namespace talk_base {

class WindowPickerFactory {
 public:
  virtual ~WindowPickerFactory() {}

  // Instance method for dependency injection.
  virtual WindowPicker* Create() {
    return CreateWindowPicker();
  }

  static WindowPicker* CreateWindowPicker() {
#if defined(WIN32)
    return new Win32WindowPicker();
#elif defined(OSX)
    return new MacWindowPicker();
#elif defined(LINUX)
    return new LinuxWindowPicker();
#else
    return NULL;
#endif
  }

  static bool IsSupported() {
#ifdef OSX
    return GetOSVersionName() >= kMacOSLeopard;
#else
    return true;
#endif
  }
};

}  // namespace talk_base

#endif  // TALK_BASE_WINDOWPICKERFACTORY_H_
