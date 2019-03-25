/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/sequenced_task_checker.h"

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "api/function_view.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/thread_checker.h"
#include "test/gtest.h"

namespace rtc {
namespace {

using ::webrtc::TaskQueueForTest;

// This class is dead code, but its purpose is to make sure that
// SequencedTaskChecker is compatible with the RTC_GUARDED_BY and RTC_RUN_ON
// attributes that are checked at compile-time.
class CompileTimeTestForGuardedBy {
 public:
  int CalledOnSequence() RTC_RUN_ON(sequence_checker_) { return guarded_; }

  void CallMeFromSequence() {
    RTC_DCHECK_RUN_ON(&sequence_checker_) << "Should be called on sequence";
    guarded_ = 41;
  }

 private:
  int guarded_ RTC_GUARDED_BY(sequence_checker_);
  SequencedTaskChecker sequence_checker_;
};

void RunOnDifferentThread(FunctionView<void()> run) {
  struct Object {
    static void Run(void* obj) {
      auto* me = static_cast<Object*>(obj);
      me->run();
      me->thread_has_run_event.Set();
    }

    FunctionView<void()> run;
    Event thread_has_run_event;
  } object{run};

  PlatformThread thread(&Object::Run, &object, "thread");
  thread.Start();
  EXPECT_TRUE(object.thread_has_run_event.Wait(1000));
  thread.Stop();
}

}  // namespace

TEST(SequencedTaskCheckerTest, CallsAllowedOnSameThread) {
  SequencedTaskChecker sequenced_task_checker;
  EXPECT_TRUE(sequenced_task_checker.CalledSequentially());
}

TEST(SequencedTaskCheckerTest, DestructorAllowedOnDifferentThread) {
  auto sequenced_task_checker = absl::make_unique<SequencedTaskChecker>();
  RunOnDifferentThread([&] {
    // Verify that the destructor doesn't assert when called on a different
    // thread.
    sequenced_task_checker.reset();
  });
}

TEST(SequencedTaskCheckerTest, DetachFromThread) {
  SequencedTaskChecker sequenced_task_checker;
  sequenced_task_checker.Detach();
  RunOnDifferentThread(
      [&] { EXPECT_TRUE(sequenced_task_checker.CalledSequentially()); });
}

TEST(SequencedTaskCheckerTest, DetachFromThreadAndUseOnTaskQueue) {
  SequencedTaskChecker sequenced_task_checker;
  sequenced_task_checker.Detach();
  TaskQueueForTest queue;
  queue.SendTask(
      [&] { EXPECT_TRUE(sequenced_task_checker.CalledSequentially()); });
}

TEST(SequencedTaskCheckerTest, DetachFromTaskQueueAndUseOnThread) {
  TaskQueueForTest queue;
  queue.SendTask([] {
    SequencedTaskChecker sequenced_task_checker;
    sequenced_task_checker.Detach();
    RunOnDifferentThread(
        [&] { EXPECT_TRUE(sequenced_task_checker.CalledSequentially()); });
  });
}

TEST(SequencedTaskCheckerTest, MethodNotAllowedOnDifferentThreadInDebug) {
  SequencedTaskChecker sequenced_task_checker;
  RunOnDifferentThread([&] {
    EXPECT_EQ(sequenced_task_checker.CalledSequentially(), !RTC_DCHECK_IS_ON);
  });
}

TEST(SequencedTaskCheckerTest, MethodNotAllowedOnDifferentTaskQueueInDebug) {
  SequencedTaskChecker sequenced_task_checker;
  TaskQueueForTest queue;
  queue.SendTask([&] {
    EXPECT_EQ(sequenced_task_checker.CalledSequentially(), !RTC_DCHECK_IS_ON);
  });
}

TEST(SequencedTaskCheckerTest, DetachFromTaskQueueInDebug) {
  SequencedTaskChecker sequenced_task_checker;
  sequenced_task_checker.Detach();

  TaskQueueForTest queue1;
  queue1.SendTask(
      [&] { EXPECT_TRUE(sequenced_task_checker.CalledSequentially()); });

  // CalledSequentially should return false in debug builds after moving to
  // another task queue.
  TaskQueueForTest queue2;
  queue2.SendTask([&] {
    EXPECT_EQ(sequenced_task_checker.CalledSequentially(), !RTC_DCHECK_IS_ON);
  });
}

class TestAnnotations {
 public:
  TestAnnotations() : test_var_(false) {}

  void ModifyTestVar() {
    RTC_DCHECK_CALLED_SEQUENTIALLY(&checker_);
    test_var_ = true;
  }

 private:
  bool test_var_ RTC_GUARDED_BY(&checker_);
  SequencedTaskChecker checker_;
};

TEST(SequencedTaskCheckerTest, TestAnnotations) {
  TestAnnotations annotations;
  annotations.ModifyTestVar();
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

void TestAnnotationsOnWrongQueue() {
  TestAnnotations annotations;
  TaskQueueForTest queue;
  queue.SendTask([&] { annotations.ModifyTestVar(); });
}

#if RTC_DCHECK_IS_ON
TEST(SequencedTaskCheckerTest, TestAnnotationsOnWrongQueueDebug) {
  ASSERT_DEATH({ TestAnnotationsOnWrongQueue(); }, "");
}
#else
TEST(SequencedTaskCheckerTest, TestAnnotationsOnWrongQueueRelease) {
  TestAnnotationsOnWrongQueue();
}
#endif
#endif  // GTEST_HAS_DEATH_TEST
}  // namespace rtc
