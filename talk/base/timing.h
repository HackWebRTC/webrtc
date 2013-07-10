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

#ifndef TALK_BASE_TIMING_H_
#define TALK_BASE_TIMING_H_

#if defined(WIN32)
#include "talk/base/win32.h"
#endif

namespace talk_base {

class Timing {
 public:
  Timing();
  virtual ~Timing();

  // WallTimeNow() returns the current wall-clock time in seconds,
  // within 10 milliseconds resolution.
  virtual double WallTimeNow();

  // TimerNow() is like WallTimeNow(), but is monotonically
  // increasing.  It returns seconds in resolution of 10 microseconds
  // or better.  Although timer and wall-clock time have the same
  // timing unit, they do not necessarily correlate because wall-clock
  // time may be adjusted backwards, hence not monotonic.
  // Made virtual so we can make a fake one.
  virtual double TimerNow();

  // BusyWait() exhausts CPU as long as the time elapsed is less than
  // the specified interval in seconds.  Returns the actual waiting
  // time based on TimerNow() measurement.
  double BusyWait(double period);

  // IdleWait() relinquishes control of CPU for specified period in
  // seconds.  It uses highest resolution sleep mechanism as possible,
  // but does not otherwise guarantee the accuracy.  Returns the
  // actual waiting time based on TimerNow() measurement.
  //
  // This function is not re-entrant for an object.  Create a fresh
  // Timing object for each thread.
  double IdleWait(double period);

 private:
#if defined(WIN32)
  HANDLE timer_handle_;
#endif
};

}  // namespace talk_base

#endif // TALK_BASE_TIMING_H_
