/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SINGLE_THREADED_TASK_QUEUE_H_
#define TEST_SINGLE_THREADED_TASK_QUEUE_H_

#include <functional>
#include <map>
#include <memory>
#include <utility>

#include "api/task_queue/task_queue_base.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/deprecation.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {
namespace test {

// DEPRECATED. This class doesn't striclty follow rtc::TaskQueue semantics,
// which makes it surprising and hard to use correctly.
// Please use TaskQueueForTest instead.

// This class gives capabilities similar to rtc::TaskQueue, but ensures
// everything happens on the same thread. This is intended to make the
// threading model of unit-tests (specifically end-to-end tests) more closely
// resemble that of real WebRTC, thereby allowing us to replace some critical
// sections by thread-checkers.
// This task is NOT tuned for performance, but rather for simplicity.
class DEPRECATED_SingleThreadedTaskQueueForTesting : public TaskQueueBase {
 public:
  using Task = std::function<void()>;
  using TaskId = size_t;
  constexpr static TaskId kInvalidTaskId = static_cast<TaskId>(-1);

  explicit DEPRECATED_SingleThreadedTaskQueueForTesting(const char* name);
  ~DEPRECATED_SingleThreadedTaskQueueForTesting() override;

  // Sends one task to the task-queue, and returns a handle by which the
  // task can be cancelled.
  // This mimics the behavior of TaskQueue, but only for lambdas, rather than
  // for both lambdas and QueuedTask objects.
  TaskId PostTask(Task task) {
    return PostDelayed(ToQueuedTask(std::move(task)), /*delay_ms=*/0);
  }

  // Same as PostTask(), but ensures that the task will not begin execution
  // less than |delay_ms| milliseconds after being posted; an upper bound
  // is not provided.
  TaskId PostDelayedTask(Task task, int64_t delay_ms) {
    return PostDelayed(ToQueuedTask(std::move(task)), delay_ms);
  }

  // Send one task to the queue. The function does not return until the task
  // has finished executing. No support for canceling the task.
  // TODO(bugs.webrtc.org/10933): Remove this function in favor of free SendTask
  // to reduce direct mentioning of the SingleThreadedTaskQueueForTesting class.
  template <typename Closure>
  void SendTask(Closure&& task) {
    ::webrtc::SendTask(this, std::forward<Closure>(task), RTC_FROM_HERE);
  }

  // Given an identifier to the task, attempts to eject it from the queue.
  // Returns true if the task was found and cancelled. Failure possible
  // only for invalid task IDs, or for tasks which have already been executed.
  bool CancelTask(TaskId task_id);

  // Returns true iff called on the thread associated with the task queue.
  bool IsCurrent();

  // Returns true iff the task queue is actively being serviced.
  bool IsRunning();

  bool HasPendingTasks() const;

  void Stop();

  // Implements TaskQueueBase.
  void Delete() override;

  void PostTask(std::unique_ptr<QueuedTask> task) override {
    PostDelayed(std::move(task), /*delay_ms=*/0);
  }

  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t delay_ms) override {
    PostDelayed(std::move(task), delay_ms);
  }

 private:
  struct StoredTask {
    StoredTask(TaskId task_id, std::unique_ptr<QueuedTask> task);
    ~StoredTask();

    TaskId task_id;
    std::unique_ptr<QueuedTask> task;
  };

  TaskId PostDelayed(std::unique_ptr<QueuedTask> task, int64_t delay_ms);

  static void Run(void* obj);

  void RunLoop();

  rtc::CriticalSection cs_;
  // Tasks are ordered by earliest execution time.
  std::multimap<int64_t, StoredTask> tasks_ RTC_GUARDED_BY(cs_);
  rtc::ThreadChecker owner_thread_checker_;
  rtc::PlatformThread thread_;
  bool running_ RTC_GUARDED_BY(cs_);

  TaskId next_task_id_;

  // The task-queue will sleep when not executing a task. Wake up occurs when:
  // * Upon destruction, to make sure that the |thead_| terminates, so that it
  //   may be joined. [Event will be set.]
  // * New task added. Because we optimize for simplicity rahter than for
  //   performance (this class is a testing facility only), waking up occurs
  //   when we get a new task even if it is scheduled with a delay. The RunLoop
  //   is in charge of sending itself back to sleep if the next task is only
  //   to be executed at a later time. [Event will be set.]
  // * When the next task in the queue is a delayed-task, and the time for
  //   its execution has come. [Event will time-out.]
  rtc::Event wake_up_;
};

// Warn if new usage.
typedef DEPRECATED_SingleThreadedTaskQueueForTesting RTC_DEPRECATED
    SingleThreadedTaskQueueForTesting;

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SINGLE_THREADED_TASK_QUEUE_H_
