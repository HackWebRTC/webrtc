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

#ifdef POSIX
#include <sys/time.h>
#if defined(OSX) || defined(IOS)
#include <mach/mach_time.h>
#endif
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

#include "talk/base/common.h"
#include "talk/base/timeutils.h"

#define EFFICIENT_IMPLEMENTATION 1

namespace talk_base {

const uint32 LAST = 0xFFFFFFFF;
const uint32 HALF = 0x80000000;

uint64 TimeNanos() {
  int64 ticks = 0;
#if defined(OSX) || defined(IOS)
  static mach_timebase_info_data_t timebase;
  if (timebase.denom == 0) {
    // Get the timebase if this is the first time we run.
    // Recommended by Apple's QA1398.
    VERIFY(KERN_SUCCESS == mach_timebase_info(&timebase));
  }
  // Use timebase to convert absolute time tick units into nanoseconds.
  ticks = mach_absolute_time() * timebase.numer / timebase.denom;
#elif defined(POSIX)
  struct timespec ts;
  // TODO: Do we need to handle the case when CLOCK_MONOTONIC
  // is not supported?
  clock_gettime(CLOCK_MONOTONIC, &ts);
  ticks = kNumNanosecsPerSec * static_cast<int64>(ts.tv_sec) +
      static_cast<int64>(ts.tv_nsec);
#elif defined(WIN32)
  static volatile LONG last_timegettime = 0;
  static volatile int64 num_wrap_timegettime = 0;
  volatile LONG* last_timegettime_ptr = &last_timegettime;
  DWORD now = timeGetTime();
  // Atomically update the last gotten time
  DWORD old = InterlockedExchange(last_timegettime_ptr, now);
  if (now < old) {
    // If now is earlier than old, there may have been a race between
    // threads.
    // 0x0fffffff ~3.1 days, the code will not take that long to execute
    // so it must have been a wrap around.
    if (old > 0xf0000000 && now < 0x0fffffff) {
      num_wrap_timegettime++;
    }
  }
  ticks = now + (num_wrap_timegettime << 32);
  // TODO: Calculate with nanosecond precision.  Otherwise, we're just
  // wasting a multiply and divide when doing Time() on Windows.
  ticks = ticks * kNumNanosecsPerMillisec;
#endif
  return ticks;
}

uint32 Time() {
  return static_cast<uint32>(TimeNanos() / kNumNanosecsPerMillisec);
}

uint64 TimeMicros() {
  return static_cast<uint64>(TimeNanos() / kNumNanosecsPerMicrosec);
}

#if defined(WIN32)
static const uint64 kFileTimeToUnixTimeEpochOffset = 116444736000000000ULL;

struct timeval {
  long tv_sec, tv_usec;  // NOLINT
};

// Emulate POSIX gettimeofday().
// Based on breakpad/src/third_party/glog/src/utilities.cc
static int gettimeofday(struct timeval *tv, void *tz) {
  // FILETIME is measured in tens of microseconds since 1601-01-01 UTC.
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);

  LARGE_INTEGER li;
  li.LowPart = ft.dwLowDateTime;
  li.HighPart = ft.dwHighDateTime;

  // Convert to seconds and microseconds since Unix time Epoch.
  int64 micros = (li.QuadPart - kFileTimeToUnixTimeEpochOffset) / 10;
  tv->tv_sec = static_cast<long>(micros / kNumMicrosecsPerSec);  // NOLINT
  tv->tv_usec = static_cast<long>(micros % kNumMicrosecsPerSec); // NOLINT

  return 0;
}

// Emulate POSIX gmtime_r().
static struct tm *gmtime_r(const time_t *timep, struct tm *result) {
  // On Windows, gmtime is thread safe.
  struct tm *tm = gmtime(timep);  // NOLINT
  if (tm == NULL) {
    return NULL;
  }
  *result = *tm;
  return result;
}
#endif  // WIN32

void CurrentTmTime(struct tm *tm, int *microseconds) {
  struct timeval timeval;
  if (gettimeofday(&timeval, NULL) < 0) {
    // Incredibly unlikely code path.
    timeval.tv_sec = timeval.tv_usec = 0;
  }
  time_t secs = timeval.tv_sec;
  gmtime_r(&secs, tm);
  *microseconds = timeval.tv_usec;
}

uint32 TimeAfter(int32 elapsed) {
  ASSERT(elapsed >= 0);
  ASSERT(static_cast<uint32>(elapsed) < HALF);
  return Time() + elapsed;
}

bool TimeIsBetween(uint32 earlier, uint32 middle, uint32 later) {
  if (earlier <= later) {
    return ((earlier <= middle) && (middle <= later));
  } else {
    return !((later < middle) && (middle < earlier));
  }
}

bool TimeIsLaterOrEqual(uint32 earlier, uint32 later) {
#if EFFICIENT_IMPLEMENTATION
  int32 diff = later - earlier;
  return (diff >= 0 && static_cast<uint32>(diff) < HALF);
#else
  const bool later_or_equal = TimeIsBetween(earlier, later, earlier + HALF);
  return later_or_equal;
#endif
}

bool TimeIsLater(uint32 earlier, uint32 later) {
#if EFFICIENT_IMPLEMENTATION
  int32 diff = later - earlier;
  return (diff > 0 && static_cast<uint32>(diff) < HALF);
#else
  const bool earlier_or_equal = TimeIsBetween(later, earlier, later + HALF);
  return !earlier_or_equal;
#endif
}

int32 TimeDiff(uint32 later, uint32 earlier) {
#if EFFICIENT_IMPLEMENTATION
  return later - earlier;
#else
  const bool later_or_equal = TimeIsBetween(earlier, later, earlier + HALF);
  if (later_or_equal) {
    if (earlier <= later) {
      return static_cast<long>(later - earlier);
    } else {
      return static_cast<long>(later + (LAST - earlier) + 1);
    }
  } else {
    if (later <= earlier) {
      return -static_cast<long>(earlier - later);
    } else {
      return -static_cast<long>(earlier + (LAST - later) + 1);
    }
  }
#endif
}

} // namespace talk_base
