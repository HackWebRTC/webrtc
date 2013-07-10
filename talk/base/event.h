/*
 * libjingle
 * Copyright 2004--2008, Google Inc.
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

#ifndef TALK_BASE_EVENT_H__
#define TALK_BASE_EVENT_H__

#if defined(WIN32)
#include "talk/base/win32.h"  // NOLINT: consider this a system header.
#elif defined(POSIX)
#include <pthread.h>
#else
#error "Must define either WIN32 or POSIX."
#endif

#include "talk/base/basictypes.h"
#include "talk/base/common.h"

namespace talk_base {

class Event {
 public:
  Event(bool manual_reset, bool initially_signaled);
  ~Event();

  void Set();
  void Reset();
  bool Wait(int cms);

 private:
  bool is_manual_reset_;

#if defined(WIN32)
  bool is_initially_signaled_;
  HANDLE event_handle_;
#elif defined(POSIX)
  bool event_status_;
  pthread_mutex_t event_mutex_;
  pthread_cond_t event_cond_;
#endif
};

}  // namespace talk_base

#endif  // TALK_BASE_EVENT_H__
