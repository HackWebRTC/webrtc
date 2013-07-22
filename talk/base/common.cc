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

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#if WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif  // WIN32

#if OSX
#include <CoreServices/CoreServices.h>
#endif  // OSX

#include <algorithm>
#include "talk/base/common.h"
#include "talk/base/logging.h"

//////////////////////////////////////////////////////////////////////
// Assertions
//////////////////////////////////////////////////////////////////////

namespace talk_base {

void Break() {
#if WIN32
  ::DebugBreak();
#else  // !WIN32
  // On POSIX systems, SIGTRAP signals debuggers to break without killing the
  // process. If a debugger isn't attached, the uncaught SIGTRAP will crash the
  // app.
  raise(SIGTRAP);
#endif
  // If a debugger wasn't attached, we will have crashed by this point. If a
  // debugger is attached, we'll continue from here.
}

static AssertLogger custom_assert_logger_ = NULL;

void SetCustomAssertLogger(AssertLogger logger) {
  custom_assert_logger_ = logger;
}

void LogAssert(const char* function, const char* file, int line,
               const char* expression) {
  if (custom_assert_logger_) {
    custom_assert_logger_(function, file, line, expression);
  } else {
    LOG(LS_ERROR) << file << "(" << line << ")" << ": ASSERT FAILED: "
                  << expression << " @ " << function;
  }
}

} // namespace talk_base
