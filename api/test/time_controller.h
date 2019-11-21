/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_TIME_CONTROLLER_H_
#define API_TEST_TIME_CONTROLLER_H_

#include <functional>
#include <memory>

#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/synchronization/yield_policy.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

// Interface for controlling time progress. This allows us to execute test code
// in either real time or simulated time by using different implementation of
// this interface.
class TimeController {
 public:
  virtual ~TimeController() = default;
  // Provides a clock instance that follows implementation defined time
  // progress.
  virtual Clock* GetClock() = 0;
  // The returned factory will created task queues that runs in implementation
  // defined time domain.
  virtual TaskQueueFactory* GetTaskQueueFactory() = 0;
  // Creates a process thread.
  virtual std::unique_ptr<ProcessThread> CreateProcessThread(
      const char* thread_name) = 0;
  // Allow task queues and process threads created by this instance to execute
  // for the given |duration|.
  virtual void Sleep(TimeDelta duration) = 0;
  // Execute closure in an implementation defined scope where rtc::Event::Wait
  // might yield to execute other tasks. This allows doing blocking waits on
  // tasks on other task queues froma a task queue without deadlocking.
  virtual void InvokeWithControlledYield(std::function<void()> closure) = 0;
  // Returns a YieldInterface which can be installed as a ScopedYieldPolicy.
  virtual rtc::YieldInterface* YieldInterface() = 0;
};

// Interface for telling time, scheduling an event to fire at a particular time,
// and waiting for time to pass.
class ControlledAlarmClock {
 public:
  virtual ~ControlledAlarmClock() = default;

  // Gets a clock that tells the alarm clock's notion of time.
  virtual Clock* GetClock() = 0;

  // Schedules the alarm to fire at |deadline|.
  // An alarm clock only supports one deadline. Calls to |ScheduleAlarmAt| with
  // an earlier deadline will reset the alarm to fire earlier.Calls to
  // |ScheduleAlarmAt| with a later deadline are ignored. Returns true if the
  // deadline changed, false otherwise.
  virtual bool ScheduleAlarmAt(Timestamp deadline) = 0;

  // Sets the callback that should be run when the alarm fires.
  virtual void SetCallback(std::function<void()> callback) = 0;

  // Waits for |duration| to pass, according to the alarm clock.
  virtual void Sleep(TimeDelta duration) = 0;
};

}  // namespace webrtc
#endif  // API_TEST_TIME_CONTROLLER_H_
