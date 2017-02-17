/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <algorithm>
#include "webrtc/base/cpu_time.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/test/gtest.h"
#include "webrtc/system_wrappers/include/cpu_info.h"

namespace {
const int kAllowedErrorMillisecs = 30;
const int kProcessingTimeMillisecs = 300;

// Consumes approximately kProcessingTimeMillisecs of CPU time.
bool WorkingFunction(void* counter_pointer) {
  int64_t* counter = reinterpret_cast<int64_t*>(counter_pointer);
  *counter = 0;
  int64_t stop_time = rtc::SystemTimeNanos() +
                      kProcessingTimeMillisecs * rtc::kNumNanosecsPerMillisec;
  while (rtc::SystemTimeNanos() < stop_time) {
    (*counter)++;
  }
  return false;
}
}  // namespace

namespace rtc {

TEST(GetProcessCpuTimeTest, SingleThread) {
  int64_t start_time_nanos = GetProcessCpuTimeNanos();
  int64_t counter;
  WorkingFunction(reinterpret_cast<void*>(&counter));
  EXPECT_GT(counter, 0);
  int64_t duration_nanos = GetProcessCpuTimeNanos() - start_time_nanos;
  //  Should be about kProcessingTimeMillisecs.
  EXPECT_NEAR(duration_nanos,
              kProcessingTimeMillisecs * kNumNanosecsPerMillisec,
              kAllowedErrorMillisecs * kNumNanosecsPerMillisec);
}

TEST(GetProcessCpuTimeTest, TwoThreads) {
  int64_t start_time_nanos = GetProcessCpuTimeNanos();
  int64_t counter1;
  int64_t counter2;
  PlatformThread thread1(WorkingFunction, reinterpret_cast<void*>(&counter1),
                         "Thread1");
  PlatformThread thread2(WorkingFunction, reinterpret_cast<void*>(&counter2),
                         "Thread2");
  thread1.Start();
  thread2.Start();
  thread1.Stop();
  thread2.Stop();

  EXPECT_GE(counter1, 0);
  EXPECT_GE(counter2, 0);
  int64_t duration_nanos = GetProcessCpuTimeNanos() - start_time_nanos;
  const uint32_t kWorkingThreads = 2;
  uint32_t used_cores =
      std::min(webrtc::CpuInfo::DetectNumberOfCores(), kWorkingThreads);
  // Two working threads for kProcessingTimeMillisecs consume double CPU time
  // if there are at least 2 cores.
  EXPECT_NEAR(duration_nanos,
              used_cores * kProcessingTimeMillisecs * kNumNanosecsPerMillisec,
              used_cores * kAllowedErrorMillisecs * kNumNanosecsPerMillisec);
}

TEST(GetThreadCpuTimeTest, SingleThread) {
  int64_t start_times_nanos = GetThreadCpuTimeNanos();
  int64_t counter;
  WorkingFunction(reinterpret_cast<void*>(&counter));
  EXPECT_GT(counter, 0);
  int64_t duration_nanos = GetThreadCpuTimeNanos() - start_times_nanos;
  EXPECT_NEAR(duration_nanos,
              kProcessingTimeMillisecs * kNumNanosecsPerMillisec,
              kAllowedErrorMillisecs * kNumNanosecsPerMillisec);
}

TEST(GetThreadCpuTimeTest, TwoThreads) {
  int64_t start_time_nanos = GetThreadCpuTimeNanos();
  int64_t counter1;
  int64_t counter2;
  PlatformThread thread1(WorkingFunction, reinterpret_cast<void*>(&counter1),
                         "Thread1");
  PlatformThread thread2(WorkingFunction, reinterpret_cast<void*>(&counter2),
                         "Thread2");
  thread1.Start();
  thread2.Start();
  thread1.Stop();
  thread2.Stop();

  EXPECT_GE(counter1, 0);
  EXPECT_GE(counter2, 0);
  int64_t duration_nanos = GetThreadCpuTimeNanos() - start_time_nanos;
  // This thread didn't do any work.
  EXPECT_NEAR(duration_nanos, 0,
              kAllowedErrorMillisecs * kNumNanosecsPerMillisec);
}

}  // namespace rtc
