/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/clock.h"

#if defined(_WIN32)
#include <Windows.h>
#include <WinSock.h>
#include <MMSystem.h>
#elif ((defined WEBRTC_LINUX) || (defined WEBRTC_MAC))
#include <sys/time.h>
#include <time.h>
#endif

#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {

#if defined(_WIN32)

struct reference_point {
  FILETIME      file_time;
  LARGE_INTEGER counterMS;
};

struct WindowsHelpTimer {
  volatile LONG _timeInMs;
  volatile LONG _numWrapTimeInMs;
  reference_point _ref_point;

  volatile LONG _sync_flag;
};

void Synchronize(WindowsHelpTimer* help_timer) {
  const LONG start_value = 0;
  const LONG new_value = 1;
  const LONG synchronized_value = 2;

  LONG compare_flag = new_value;
  while (help_timer->_sync_flag == start_value) {
    const LONG new_value = 1;
    compare_flag = InterlockedCompareExchange(
        &help_timer->_sync_flag, new_value, start_value);
  }
  if (compare_flag != start_value) {
    // This thread was not the one that incremented the sync flag.
    // Block until synchronization finishes.
    while (compare_flag != synchronized_value) {
      ::Sleep(0);
    }
    return;
  }
  // Only the synchronizing thread gets here so this part can be
  // considered single threaded.

  // set timer accuracy to 1 ms
  timeBeginPeriod(1);
  FILETIME    ft0 = { 0, 0 },
              ft1 = { 0, 0 };
  //
  // Spin waiting for a change in system time. Get the matching
  // performance counter value for that time.
  //
  ::GetSystemTimeAsFileTime(&ft0);
  do {
    ::GetSystemTimeAsFileTime(&ft1);

    help_timer->_ref_point.counterMS.QuadPart = ::timeGetTime();
    ::Sleep(0);
  } while ((ft0.dwHighDateTime == ft1.dwHighDateTime) &&
          (ft0.dwLowDateTime == ft1.dwLowDateTime));
  help_timer->_ref_point.file_time = ft1;
  timeEndPeriod(1);
}

void get_time(WindowsHelpTimer* help_timer, FILETIME& current_time) {
  // we can't use query performance counter due to speed stepping
  DWORD t = timeGetTime();
  // NOTE: we have a missmatch in sign between _timeInMs(LONG) and
  // (DWORD) however we only use it here without +- etc
  volatile LONG* timeInMsPtr = &help_timer->_timeInMs;
  // Make sure that we only inc wrapper once.
  DWORD old = InterlockedExchange(timeInMsPtr, t);
  if(old > t) {
    // wrap
    help_timer->_numWrapTimeInMs++;
  }
  LARGE_INTEGER elapsedMS;
  elapsedMS.HighPart = help_timer->_numWrapTimeInMs;
  elapsedMS.LowPart = t;

  elapsedMS.QuadPart = elapsedMS.QuadPart -
      help_timer->_ref_point.counterMS.QuadPart;

  // Translate to 100-nanoseconds intervals (FILETIME resolution)
  // and add to reference FILETIME to get current FILETIME.
  ULARGE_INTEGER filetime_ref_as_ul;

  filetime_ref_as_ul.HighPart =
      help_timer->_ref_point.file_time.dwHighDateTime;
  filetime_ref_as_ul.LowPart =
      help_timer->_ref_point.file_time.dwLowDateTime;
  filetime_ref_as_ul.QuadPart +=
      (ULONGLONG)((elapsedMS.QuadPart)*1000*10);

  // Copy to result
  current_time.dwHighDateTime = filetime_ref_as_ul.HighPart;
  current_time.dwLowDateTime = filetime_ref_as_ul.LowPart;
}
#endif

class RealTimeClock : public Clock {
  // Return a timestamp in milliseconds relative to some arbitrary source; the
  // source is fixed for this clock.
  virtual int64_t TimeInMilliseconds() {
    return TickTime::MillisecondTimestamp();
  }

  // Return a timestamp in microseconds relative to some arbitrary source; the
  // source is fixed for this clock.
  virtual int64_t TimeInMicroseconds() {
    return TickTime::MicrosecondTimestamp();
  }
};

#if defined(_WIN32)
class WindowsRealTimeClock : public RealTimeClock {
 public:
  WindowsRealTimeClock(WindowsHelpTimer* helpTimer)
      : _helpTimer(helpTimer) {}

  virtual ~WindowsRealTimeClock() {}

  // Retrieve an NTP absolute timestamp.
  virtual void CurrentNtp(uint32_t& seconds, uint32_t& fractions) {
    const uint64_t FILETIME_1970 = 0x019db1ded53e8000;

    FILETIME StartTime;
    uint64_t Time;
    struct timeval tv;

    // We can't use query performance counter since they can change depending on
    // speed steping
    get_time(_helpTimer, StartTime);

    Time = (((uint64_t) StartTime.dwHighDateTime) << 32) +
           (uint64_t) StartTime.dwLowDateTime;

    // Convert the hecto-nano second time to tv format
    Time -= FILETIME_1970;

    tv.tv_sec = (uint32_t)(Time / (uint64_t)10000000);
    tv.tv_usec = (uint32_t)((Time % (uint64_t)10000000) / 10);

    double dtemp;

    seconds = tv.tv_sec + kNtpJan1970;
    dtemp = tv.tv_usec / 1e6;

    if (dtemp >= 1) {
      dtemp -= 1;
      seconds++;
    } else if (dtemp < -1) {
      dtemp += 1;
      seconds--;
    }
    dtemp *= kMagicNtpFractionalUnit;
    fractions = (uint32_t)dtemp;
  }

 private:
  WindowsHelpTimer* _helpTimer;
};

#elif ((defined WEBRTC_LINUX) || (defined WEBRTC_MAC))
class UnixRealTimeClock : public RealTimeClock {
 public:
  UnixRealTimeClock() {}

  virtual ~UnixRealTimeClock() {}

  // Retrieve an NTP absolute timestamp.
  virtual void CurrentNtp(uint32_t& seconds, uint32_t& fractions) {
    double dtemp;
    struct timeval tv;
    struct timezone tz;
    tz.tz_minuteswest  = 0;
    tz.tz_dsttime = 0;
    gettimeofday(&tv, &tz);

    seconds = tv.tv_sec + kNtpJan1970;
    dtemp = tv.tv_usec / 1e6;
    if (dtemp >= 1) {
      dtemp -= 1;
      seconds++;
    } else if (dtemp < -1) {
      dtemp += 1;
      seconds--;
    }
    dtemp *= kMagicNtpFractionalUnit;
    fractions = (uint32_t)dtemp;
  }
};
#endif


#if defined(_WIN32)
// Keeps the global state for the Windows implementation of RtpRtcpClock.
// Note that this is a POD. Only PODs are allowed to have static storage
// duration according to the Google Style guide.
static WindowsHelpTimer global_help_timer = {0, 0, {{ 0, 0}, 0}, 0};
#endif

Clock* Clock::GetRealTimeClock() {
#if defined(_WIN32)
  static WindowsRealTimeClock clock(&global_help_timer);
  return &clock;
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
  static UnixRealTimeClock clock;
  return &clock;
#else
  return NULL;
#endif
}

SimulatedClock::SimulatedClock(int64_t initial_time_us)
    : time_us_(initial_time_us) {}

int64_t SimulatedClock::TimeInMilliseconds() {
  return (time_us_ + 500) / 1000;
}

int64_t SimulatedClock::TimeInMicroseconds() {
  return time_us_;
}

void SimulatedClock::CurrentNtp(uint32_t& seconds, uint32_t& fractions) {
  seconds = (TimeInMilliseconds() / 1000) + kNtpJan1970;
  fractions = (uint32_t)((TimeInMilliseconds() % 1000) *
      kMagicNtpFractionalUnit / 1000);
}

void SimulatedClock::AdvanceTimeMilliseconds(int64_t milliseconds) {
  AdvanceTimeMicroseconds(1000 * milliseconds);
}

void SimulatedClock::AdvanceTimeMicroseconds(int64_t microseconds) {
  time_us_ += microseconds;
}

};  // namespace webrtc
