/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#include "talk/base/common.h"
#include "talk/base/gunit.h"
#include "talk/base/thread.h"
#include "talk/base/timeutils.h"

namespace talk_base {

TEST(TimeTest, TimeInMs) {
  uint32 ts_earlier = Time();
  Thread::SleepMs(100);
  uint32 ts_now = Time();
  // Allow for the thread to wakeup ~20ms early.
  EXPECT_GE(ts_now, ts_earlier + 80);
  // Make sure the Time is not returning in smaller unit like microseconds.
  EXPECT_LT(ts_now, ts_earlier + 1000);
}

TEST(TimeTest, Comparison) {
  // Obtain two different times, in known order
  TimeStamp ts_earlier = Time();
  Thread::SleepMs(100);
  TimeStamp ts_now = Time();
  EXPECT_NE(ts_earlier, ts_now);

  // Common comparisons
  EXPECT_TRUE( TimeIsLaterOrEqual(ts_earlier, ts_now));
  EXPECT_TRUE( TimeIsLater(       ts_earlier, ts_now));
  EXPECT_FALSE(TimeIsLaterOrEqual(ts_now,     ts_earlier));
  EXPECT_FALSE(TimeIsLater(       ts_now,     ts_earlier));

  // Edge cases
  EXPECT_TRUE( TimeIsLaterOrEqual(ts_earlier, ts_earlier));
  EXPECT_FALSE(TimeIsLater(       ts_earlier, ts_earlier));

  // Obtain a third time
  TimeStamp ts_later = TimeAfter(100);
  EXPECT_NE(ts_now, ts_later);
  EXPECT_TRUE( TimeIsLater(ts_now,     ts_later));
  EXPECT_TRUE( TimeIsLater(ts_earlier, ts_later));

  // Common comparisons
  EXPECT_TRUE( TimeIsBetween(ts_earlier, ts_now,     ts_later));
  EXPECT_FALSE(TimeIsBetween(ts_earlier, ts_later,   ts_now));
  EXPECT_FALSE(TimeIsBetween(ts_now,     ts_earlier, ts_later));
  EXPECT_TRUE( TimeIsBetween(ts_now,     ts_later,   ts_earlier));
  EXPECT_TRUE( TimeIsBetween(ts_later,   ts_earlier, ts_now));
  EXPECT_FALSE(TimeIsBetween(ts_later,   ts_now,     ts_earlier));

  // Edge cases
  EXPECT_TRUE( TimeIsBetween(ts_earlier, ts_earlier, ts_earlier));
  EXPECT_TRUE( TimeIsBetween(ts_earlier, ts_earlier, ts_later));
  EXPECT_TRUE( TimeIsBetween(ts_earlier, ts_later,   ts_later));

  // Earlier of two times
  EXPECT_EQ(ts_earlier, TimeMin(ts_earlier, ts_earlier));
  EXPECT_EQ(ts_earlier, TimeMin(ts_earlier, ts_now));
  EXPECT_EQ(ts_earlier, TimeMin(ts_earlier, ts_later));
  EXPECT_EQ(ts_earlier, TimeMin(ts_now,     ts_earlier));
  EXPECT_EQ(ts_earlier, TimeMin(ts_later,   ts_earlier));

  // Later of two times
  EXPECT_EQ(ts_earlier, TimeMax(ts_earlier, ts_earlier));
  EXPECT_EQ(ts_now,     TimeMax(ts_earlier, ts_now));
  EXPECT_EQ(ts_later,   TimeMax(ts_earlier, ts_later));
  EXPECT_EQ(ts_now,     TimeMax(ts_now,     ts_earlier));
  EXPECT_EQ(ts_later,   TimeMax(ts_later,   ts_earlier));
}

TEST(TimeTest, Intervals) {
  TimeStamp ts_earlier = Time();
  TimeStamp ts_later = TimeAfter(500);

  // We can't depend on ts_later and ts_earlier to be exactly 500 apart
  // since time elapses between the calls to Time() and TimeAfter(500)
  EXPECT_LE(500,  TimeDiff(ts_later, ts_earlier));
  EXPECT_GE(-500, TimeDiff(ts_earlier, ts_later));

  // Time has elapsed since ts_earlier
  EXPECT_GE(TimeSince(ts_earlier), 0);

  // ts_earlier is earlier than now, so TimeUntil ts_earlier is -ve
  EXPECT_LE(TimeUntil(ts_earlier), 0);

  // ts_later likely hasn't happened yet, so TimeSince could be -ve
  // but within 500
  EXPECT_GE(TimeSince(ts_later), -500);

  // TimeUntil ts_later is at most 500
  EXPECT_LE(TimeUntil(ts_later), 500);
}

TEST(TimeTest, BoundaryComparison) {
  // Obtain two different times, in known order
  TimeStamp ts_earlier = static_cast<TimeStamp>(-50);
  TimeStamp ts_later = ts_earlier + 100;
  EXPECT_NE(ts_earlier, ts_later);

  // Common comparisons
  EXPECT_TRUE( TimeIsLaterOrEqual(ts_earlier, ts_later));
  EXPECT_TRUE( TimeIsLater(       ts_earlier, ts_later));
  EXPECT_FALSE(TimeIsLaterOrEqual(ts_later,   ts_earlier));
  EXPECT_FALSE(TimeIsLater(       ts_later,   ts_earlier));

  // Earlier of two times
  EXPECT_EQ(ts_earlier, TimeMin(ts_earlier, ts_earlier));
  EXPECT_EQ(ts_earlier, TimeMin(ts_earlier, ts_later));
  EXPECT_EQ(ts_earlier, TimeMin(ts_later,   ts_earlier));

  // Later of two times
  EXPECT_EQ(ts_earlier, TimeMax(ts_earlier, ts_earlier));
  EXPECT_EQ(ts_later,   TimeMax(ts_earlier, ts_later));
  EXPECT_EQ(ts_later,   TimeMax(ts_later,   ts_earlier));

  // Interval
  EXPECT_EQ(100,  TimeDiff(ts_later, ts_earlier));
  EXPECT_EQ(-100, TimeDiff(ts_earlier, ts_later));
}

TEST(TimeTest, DISABLED_CurrentTmTime) {
  struct tm tm;
  int microseconds;

  time_t before = ::time(NULL);
  CurrentTmTime(&tm, &microseconds);
  time_t after = ::time(NULL);

  // Assert that 'tm' represents a time between 'before' and 'after'.
  // mktime() uses local time, so we have to compensate for that.
  time_t local_delta = before - ::mktime(::gmtime(&before));  // NOLINT
  time_t t = ::mktime(&tm) + local_delta;

  EXPECT_TRUE(before <= t && t <= after);
  EXPECT_TRUE(0 <= microseconds && microseconds < 1000000);
}

}  // namespace talk_base
