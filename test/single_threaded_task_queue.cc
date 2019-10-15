/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/single_threaded_task_queue.h"

#include <memory>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace test {

DEPRECATED_SingleThreadedTaskQueueForTesting::StoredTask::StoredTask(
    DEPRECATED_SingleThreadedTaskQueueForTesting::TaskId task_id,
    std::unique_ptr<QueuedTask> task)
    : task_id(task_id), task(std::move(task)) {}

DEPRECATED_SingleThreadedTaskQueueForTesting::StoredTask::~StoredTask() =
    default;

DEPRECATED_SingleThreadedTaskQueueForTesting::
    DEPRECATED_SingleThreadedTaskQueueForTesting(const char* name)
    : thread_(Run, this, name), running_(true), next_task_id_(0) {
  thread_.Start();
}

DEPRECATED_SingleThreadedTaskQueueForTesting::
    ~DEPRECATED_SingleThreadedTaskQueueForTesting() {
  Stop();
}

DEPRECATED_SingleThreadedTaskQueueForTesting::TaskId
DEPRECATED_SingleThreadedTaskQueueForTesting::PostDelayed(
    std::unique_ptr<QueuedTask> task,
    int64_t delay_ms) {
  int64_t earliest_exec_time = rtc::TimeAfter(delay_ms);

  rtc::CritScope lock(&cs_);
  if (!running_)
    return kInvalidTaskId;

  TaskId id = next_task_id_++;

  // Insert after any other tasks with an earlier-or-equal target time.
  // Note: multimap has promise "The order of the key-value pairs whose keys
  // compare equivalent is the order of insertion and does not change."
  tasks_.emplace(std::piecewise_construct,
                 std::forward_as_tuple(earliest_exec_time),
                 std::forward_as_tuple(id, std::move(task)));

  // This class is optimized for simplicty, not for performance. This will wake
  // the thread up even if the next task in the queue is only scheduled for
  // quite some time from now. In that case, the thread will just send itself
  // back to sleep.
  wake_up_.Set();

  return id;
}

bool DEPRECATED_SingleThreadedTaskQueueForTesting::CancelTask(TaskId task_id) {
  rtc::CritScope lock(&cs_);
  for (auto it = tasks_.begin(); it != tasks_.end(); it++) {
    if (it->second.task_id == task_id) {
      tasks_.erase(it);
      return true;
    }
  }
  return false;
}

bool DEPRECATED_SingleThreadedTaskQueueForTesting::IsCurrent() {
  return rtc::IsThreadRefEqual(thread_.GetThreadRef(), rtc::CurrentThreadRef());
}

bool DEPRECATED_SingleThreadedTaskQueueForTesting::IsRunning() {
  RTC_DCHECK_RUN_ON(&owner_thread_checker_);
  // We could check the |running_| flag here, but this is equivalent for the
  // purposes of this function.
  return thread_.IsRunning();
}

bool DEPRECATED_SingleThreadedTaskQueueForTesting::HasPendingTasks() const {
  rtc::CritScope lock(&cs_);
  return !tasks_.empty();
}

void DEPRECATED_SingleThreadedTaskQueueForTesting::Stop() {
  RTC_DCHECK_RUN_ON(&owner_thread_checker_);
  if (!thread_.IsRunning())
    return;

  {
    rtc::CritScope lock(&cs_);
    running_ = false;
  }

  wake_up_.Set();
  thread_.Stop();
}

void DEPRECATED_SingleThreadedTaskQueueForTesting::Run(void* obj) {
  static_cast<DEPRECATED_SingleThreadedTaskQueueForTesting*>(obj)->RunLoop();
}

void DEPRECATED_SingleThreadedTaskQueueForTesting::RunLoop() {
  CurrentTaskQueueSetter set_current(this);
  while (true) {
    std::unique_ptr<QueuedTask> queued_task;

    // An empty queue would lead to sleeping until the queue becoems non-empty.
    // A queue where the earliest task is scheduled for later than now, will
    // lead to sleeping until the time of the next scheduled task (or until
    // more tasks are scheduled).
    int wait_time = rtc::Event::kForever;

    {
      rtc::CritScope lock(&cs_);
      if (!running_) {
        return;
      }
      if (!tasks_.empty()) {
        auto next_delayed_task = tasks_.begin();
        int64_t earliest_exec_time = next_delayed_task->first;
        int64_t remaining_delay_ms =
            rtc::TimeDiff(earliest_exec_time, rtc::TimeMillis());
        if (remaining_delay_ms <= 0) {
          queued_task = std::move(next_delayed_task->second.task);
          tasks_.erase(next_delayed_task);
        } else {
          wait_time = rtc::saturated_cast<int>(remaining_delay_ms);
        }
      }
    }

    if (queued_task) {
      if (!queued_task->Run()) {
        queued_task.release();
      }
    } else {
      wake_up_.Wait(wait_time);
    }
  }
}

void DEPRECATED_SingleThreadedTaskQueueForTesting::Delete() {
  Stop();
  delete this;
}

}  // namespace test
}  // namespace webrtc
