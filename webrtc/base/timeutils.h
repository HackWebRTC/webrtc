/*
 *  Copyright 2005 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_TIMEUTILS_H_
#define WEBRTC_BASE_TIMEUTILS_H_

#include <ctime>
#include <time.h>

#include "webrtc/base/basictypes.h"

namespace rtc {

static const int64_t kNumMillisecsPerSec = INT64_C(1000);
static const int64_t kNumMicrosecsPerSec = INT64_C(1000000);
static const int64_t kNumNanosecsPerSec = INT64_C(1000000000);

static const int64_t kNumMicrosecsPerMillisec =
    kNumMicrosecsPerSec / kNumMillisecsPerSec;
static const int64_t kNumNanosecsPerMillisec =
    kNumNanosecsPerSec / kNumMillisecsPerSec;
static const int64_t kNumNanosecsPerMicrosec =
    kNumNanosecsPerSec / kNumMicrosecsPerSec;

typedef uint32_t TimeStamp;

// Returns the current time in milliseconds in 32 bits.
uint32_t Time32();

// Returns the current time in milliseconds in 64 bits.
int64_t TimeMillis();

// Returns the current time in milliseconds.
// TODO(honghaiz): Change to return TimeMillis() once majority of the webrtc
// code migrates to 64-bit timestamp.
inline uint32_t Time() {
  return Time32();
}

// Returns the current time in microseconds.
uint64_t TimeMicros();
// Returns the current time in nanoseconds.
uint64_t TimeNanos();

// Returns a future timestamp, 'elapsed' milliseconds from now.
uint32_t TimeAfter(int32_t elapsed);

bool TimeIsLaterOrEqual(uint32_t earlier, uint32_t later);  // Inclusive
bool TimeIsLater(uint32_t earlier, uint32_t later);         // Exclusive

// Returns the later of two timestamps.
inline uint32_t TimeMax(uint32_t ts1, uint32_t ts2) {
  return TimeIsLaterOrEqual(ts1, ts2) ? ts2 : ts1;
}

// Returns the earlier of two timestamps.
inline uint32_t TimeMin(uint32_t ts1, uint32_t ts2) {
  return TimeIsLaterOrEqual(ts1, ts2) ? ts1 : ts2;
}

// Number of milliseconds that would elapse between 'earlier' and 'later'
// timestamps.  The value is negative if 'later' occurs before 'earlier'.
int32_t TimeDiff(uint32_t later, uint32_t earlier);

// Number of milliseconds that would elapse between 'earlier' and 'later'
// timestamps.  The value is negative if 'later' occurs before 'earlier'.
int64_t TimeDiff64(int64_t later, int64_t earlier);

// The number of milliseconds that have elapsed since 'earlier'.
inline int32_t TimeSince(uint32_t earlier) {
  return TimeDiff(Time(), earlier);
}

// The number of milliseconds that will elapse between now and 'later'.
inline int32_t TimeUntil(uint32_t later) {
  return TimeDiff(later, Time());
}

class TimestampWrapAroundHandler {
 public:
  TimestampWrapAroundHandler();

  int64_t Unwrap(uint32_t ts);

 private:
  uint32_t last_ts_;
  int64_t num_wrap_;
};

// Convert from std::tm, which is relative to 1900-01-01 00:00 to number of
// seconds from 1970-01-01 00:00 ("epoch").  Don't return time_t since that
// is still 32 bits on many systems.
int64_t TmToSeconds(const std::tm& tm);

}  // namespace rtc

#endif  // WEBRTC_BASE_TIMEUTILS_H_
