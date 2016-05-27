/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/fakeclock.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/messagequeue.h"

namespace rtc {

uint64_t FakeClock::TimeNanos() const {
  CritScope cs(&lock_);
  return time_;
}

void FakeClock::SetTimeNanos(uint64_t nanos) {
  {
    CritScope cs(&lock_);
    RTC_DCHECK(nanos >= time_);
    time_ = nanos;
  }
  // If message queues are waiting in a socket select() with a timeout provided
  // by the OS, they should wake up to check if there are any messages ready to
  // be dispatched based on the fake time.
  MessageQueueManager::WakeAllMessageQueues();
}

void FakeClock::AdvanceTime(TimeDelta delta) {
  CritScope cs(&lock_);
  SetTimeNanos(time_ + delta.ToNanoseconds());
}

}  // namespace rtc
