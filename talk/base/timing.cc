/*
 * libjingle
 * Copyright 2008, Google Inc.
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

#include "talk/base/timing.h"
#include "talk/base/timeutils.h"

#if defined(POSIX)
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#if defined(OSX)
#include <mach/mach.h>
#include <mach/clock.h>
#endif
#elif defined(WIN32)
#include <sys/timeb.h>
#include "talk/base/win32.h"
#endif

namespace talk_base {

Timing::Timing() {
#if defined(WIN32)
  // This may fail, but we handle failure gracefully in the methods
  // that use it (use alternative sleep method).
  //
  // TODO: Make it possible for user to tell if IdleWait will
  // be done at lesser resolution because of this.
  timer_handle_ = CreateWaitableTimer(NULL,     // Security attributes.
                                      FALSE,    // Manual reset?
                                      NULL);    // Timer name.
#endif
}

Timing::~Timing() {
#if defined(WIN32)
  if (timer_handle_ != NULL)
    CloseHandle(timer_handle_);
#endif
}

double Timing::WallTimeNow() {
#if defined(POSIX)
  struct timeval time;
  gettimeofday(&time, NULL);
  // Convert from second (1.0) and microsecond (1e-6).
  return (static_cast<double>(time.tv_sec) +
          static_cast<double>(time.tv_usec) * 1.0e-6);

#elif defined(WIN32)
  struct _timeb time;
  _ftime(&time);
  // Convert from second (1.0) and milliseconds (1e-3).
  return (static_cast<double>(time.time) +
          static_cast<double>(time.millitm) * 1.0e-3);
#endif
}

double Timing::TimerNow() {
  return (static_cast<double>(TimeNanos()) / kNumNanosecsPerSec);
}

double Timing::BusyWait(double period) {
  double start_time = TimerNow();
  while (TimerNow() - start_time < period) {
  }
  return TimerNow() - start_time;
}

double Timing::IdleWait(double period) {
  double start_time = TimerNow();

#if defined(POSIX)
  double sec_int, sec_frac = modf(period, &sec_int);
  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(sec_int);
  ts.tv_nsec = static_cast<long>(sec_frac * 1.0e9);  // NOLINT

  // NOTE(liulk): for the NOLINT above, long is the appropriate POSIX
  // type.

  // POSIX nanosleep may be interrupted by signals.
  while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
  }

#elif defined(WIN32)
  if (timer_handle_ != NULL) {
    LARGE_INTEGER due_time;

    // Negative indicates relative time.  The unit is 100 nanoseconds.
    due_time.QuadPart = -LONGLONG(period * 1.0e7);

    SetWaitableTimer(timer_handle_, &due_time, 0, NULL, NULL, TRUE);
    WaitForSingleObject(timer_handle_, INFINITE);
  } else {
    // Still attempts to sleep with lesser resolution.
    // The unit is in milliseconds.
    Sleep(DWORD(period * 1.0e3));
  }
#endif

  return TimerNow() - start_time;
}

}  // namespace talk_base
