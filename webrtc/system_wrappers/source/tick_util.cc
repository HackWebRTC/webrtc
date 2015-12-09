/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/include/tick_util.h"

#include <assert.h>

namespace webrtc {

int64_t TickTime::MillisecondTimestamp() {
  return TicksToMilliseconds(TickTime::Now().Ticks());
}

int64_t TickTime::MicrosecondTimestamp() {
  return TicksToMicroseconds(TickTime::Now().Ticks());
}

int64_t TickTime::MillisecondsToTicks(const int64_t ms) {
#if _WIN32
  return ms;
#elif defined(WEBRTC_LINUX)
  return ms * 1000000LL;
#elif defined(WEBRTC_MAC)
  // TODO(pbos): Fix unsafe use of static locals.
  static double timebase_from_millisecond_fract = 0.0;
  if (timebase_from_millisecond_fract == 0.0) {
    mach_timebase_info_data_t timebase;
    (void)mach_timebase_info(&timebase);
    timebase_from_millisecond_fract = (timebase.denom * 1e6) / timebase.numer;
  }
  return ms * timebase_from_millisecond_fract;
#else
  return ms * 1000LL;
#endif
}

int64_t TickTime::TicksToMilliseconds(const int64_t ticks) {
#if _WIN32
  return ticks;
#elif defined(WEBRTC_LINUX)
  return ticks / 1000000LL;
#elif defined(WEBRTC_MAC)
  // TODO(pbos): Fix unsafe use of static locals.
  static double timebase_microsecond_fract = 0.0;
  if (timebase_microsecond_fract == 0.0) {
    mach_timebase_info_data_t timebase;
    (void)mach_timebase_info(&timebase);
    timebase_microsecond_fract = timebase.numer / (timebase.denom * 1e6);
  }
  return ticks * timebase_microsecond_fract;
#else
  return ticks;
#endif
}

int64_t TickTime::TicksToMicroseconds(const int64_t ticks) {
#if _WIN32
  return ticks * 1000LL;
#elif defined(WEBRTC_LINUX)
  return ticks / 1000LL;
#elif defined(WEBRTC_MAC)
  // TODO(pbos): Fix unsafe use of static locals.
  static double timebase_microsecond_fract = 0.0;
  if (timebase_microsecond_fract == 0.0) {
    mach_timebase_info_data_t timebase;
    (void)mach_timebase_info(&timebase);
    timebase_microsecond_fract = timebase.numer / (timebase.denom * 1e3);
  }
  return ticks * timebase_microsecond_fract;
#else
  return ticks;
#endif
}

// Gets the native system tick count. The actual unit, resolution, and epoch
// varies by platform:
// Windows: Milliseconds of uptime with rollover count in the upper 32-bits.
// Linux/Android: Nanoseconds since the Unix epoch.
// Mach (Mac/iOS): "absolute" time since first call.
// Unknown POSIX: Microseconds since the Unix epoch.
int64_t TickTime::QueryOsForTicks() {
#if _WIN32
  static volatile LONG last_time_get_time = 0;
  static volatile int64_t num_wrap_time_get_time = 0;
  volatile LONG* last_time_get_time_ptr = &last_time_get_time;
  DWORD now = timeGetTime();
  // Atomically update the last gotten time
  DWORD old = InterlockedExchange(last_time_get_time_ptr, now);
  if (now < old) {
    // If now is earlier than old, there may have been a race between
    // threads.
    // 0x0fffffff ~3.1 days, the code will not take that long to execute
    // so it must have been a wrap around.
    if (old > 0xf0000000 && now < 0x0fffffff) {
      // TODO(pbos): Fix unsafe use of static locals.
      num_wrap_time_get_time++;
    }
  }
  return now + (num_wrap_time_get_time << 32);
#elif defined(WEBRTC_LINUX)
  struct timespec ts;
  // TODO(wu): Remove CLOCK_REALTIME implementation.
#ifdef WEBRTC_CLOCK_TYPE_REALTIME
  clock_gettime(CLOCK_REALTIME, &ts);
#else
  clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
  return 1000000000LL * ts.tv_sec + ts.tv_nsec;
#elif defined(WEBRTC_MAC)
  // Return absolute time as an offset from the first call to this function, so
  // that we can do floating-point (double) operations on it without losing
  // precision. This holds true until the elapsed time is ~11 days,
  // at which point we'll start to lose some precision, though not enough to
  // matter for millisecond accuracy for another couple years after that.
  // TODO(pbos): Fix unsafe use of static locals.
  static uint64_t timebase_start = 0;
  if (timebase_start == 0) {
    timebase_start = mach_absolute_time();
  }
  return mach_absolute_time() - timebase_start;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return 1000000LL * tv.tv_sec + tv.tv_usec;
#endif
}

}  // namespace webrtc
