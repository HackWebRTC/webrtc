/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/post_message_with_functor.h"

#include <memory>

#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace rtc {

namespace {

void ThreadIsCurrent(Thread* thread, bool* result, Event* event) {
  *result = thread->IsCurrent();
  event->Set();
}

void WaitAndSetEvent(Event* wait_event, Event* set_event) {
  wait_event->Wait(Event::kForever);
  set_event->Set();
}

// A functor that keeps track of the number of copies and moves.
class LifeCycleFunctor {
 public:
  struct Stats {
    size_t copy_count = 0;
    size_t move_count = 0;
  };

  LifeCycleFunctor(Stats* stats, Event* event) : stats_(stats), event_(event) {}
  LifeCycleFunctor(const LifeCycleFunctor& other) { *this = other; }
  LifeCycleFunctor(LifeCycleFunctor&& other) { *this = std::move(other); }

  LifeCycleFunctor& operator=(const LifeCycleFunctor& other) {
    stats_ = other.stats_;
    event_ = other.event_;
    ++stats_->copy_count;
    return *this;
  }

  LifeCycleFunctor& operator=(LifeCycleFunctor&& other) {
    stats_ = other.stats_;
    event_ = other.event_;
    ++stats_->move_count;
    return *this;
  }

  void operator()() { event_->Set(); }

 private:
  Stats* stats_;
  Event* event_;
};

// A functor that verifies the thread it was destroyed on.
class DestructionFunctor {
 public:
  DestructionFunctor(Thread* thread, bool* thread_was_current, Event* event)
      : thread_(thread),
        thread_was_current_(thread_was_current),
        event_(event) {}
  ~DestructionFunctor() {
    // Only signal the event if this was the functor that was invoked to avoid
    // the event being signaled due to the destruction of temporary/moved
    // versions of this object.
    if (was_invoked_) {
      *thread_was_current_ = thread_->IsCurrent();
      event_->Set();
    }
  }

  void operator()() { was_invoked_ = true; }

 private:
  Thread* thread_;
  bool* thread_was_current_;
  Event* event_;
  bool was_invoked_ = false;
};

}  // namespace

TEST(PostMessageWithFunctorTest, InvokesWithBind) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  Event event;
  PostMessageWithFunctor(RTC_FROM_HERE, background_thread.get(),
                         Bind(&Event::Set, &event));
  event.Wait(Event::kForever);
}

TEST(PostMessageWithFunctorTest, InvokesWithLambda) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  Event event;
  PostMessageWithFunctor(RTC_FROM_HERE, background_thread.get(),
                         [&event] { event.Set(); });
  event.Wait(Event::kForever);
}

TEST(PostMessageWithFunctorTest, InvokesWithCopiedFunctor) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  LifeCycleFunctor::Stats stats;
  Event event;
  LifeCycleFunctor functor(&stats, &event);
  PostMessageWithFunctor(RTC_FROM_HERE, background_thread.get(), functor);
  event.Wait(Event::kForever);

  EXPECT_EQ(1u, stats.copy_count);
  EXPECT_EQ(0u, stats.move_count);
}

TEST(PostMessageWithFunctorTest, InvokesWithMovedFunctor) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  LifeCycleFunctor::Stats stats;
  Event event;
  LifeCycleFunctor functor(&stats, &event);
  PostMessageWithFunctor(RTC_FROM_HERE, background_thread.get(),
                         std::move(functor));
  event.Wait(Event::kForever);

  EXPECT_EQ(0u, stats.copy_count);
  EXPECT_EQ(1u, stats.move_count);
}

TEST(PostMessageWithFunctorTest,
     InvokesWithCopiedFunctorDestroyedOnTargetThread) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  Event event;
  bool was_invoked_on_background_thread = false;
  DestructionFunctor functor(background_thread.get(),
                             &was_invoked_on_background_thread, &event);
  PostMessageWithFunctor(RTC_FROM_HERE, background_thread.get(), functor);
  event.Wait(Event::kForever);

  EXPECT_TRUE(was_invoked_on_background_thread);
}

TEST(PostMessageWithFunctorTest,
     InvokesWithMovedFunctorDestroyedOnTargetThread) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  Event event;
  bool was_invoked_on_background_thread = false;
  DestructionFunctor functor(background_thread.get(),
                             &was_invoked_on_background_thread, &event);
  PostMessageWithFunctor(RTC_FROM_HERE, background_thread.get(),
                         std::move(functor));
  event.Wait(Event::kForever);

  EXPECT_TRUE(was_invoked_on_background_thread);
}

TEST(PostMessageWithFunctorTest, InvokesOnBackgroundThread) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  Event event;
  bool was_invoked_on_background_thread = false;
  PostMessageWithFunctor(RTC_FROM_HERE, background_thread.get(),
                         Bind(&ThreadIsCurrent, background_thread.get(),
                              &was_invoked_on_background_thread, &event));
  event.Wait(Event::kForever);

  EXPECT_TRUE(was_invoked_on_background_thread);
}

TEST(PostMessageWithFunctorTest, InvokesAsynchronously) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  // The first event ensures that SendSingleMessage() is not blocking this
  // thread. The second event ensures that the message is processed.
  Event event_set_by_test_thread;
  Event event_set_by_background_thread;
  PostMessageWithFunctor(RTC_FROM_HERE, background_thread.get(),
                         Bind(&WaitAndSetEvent, &event_set_by_test_thread,
                              &event_set_by_background_thread));
  event_set_by_test_thread.Set();
  event_set_by_background_thread.Wait(Event::kForever);
}

TEST(PostMessageWithFunctorTest, InvokesInPostedOrder) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  Event first;
  Event second;
  Event third;
  Event fourth;

  PostMessageWithFunctor(RTC_FROM_HERE, background_thread.get(),
                         Bind(&WaitAndSetEvent, &first, &second));
  PostMessageWithFunctor(RTC_FROM_HERE, background_thread.get(),
                         Bind(&WaitAndSetEvent, &second, &third));
  PostMessageWithFunctor(RTC_FROM_HERE, background_thread.get(),
                         Bind(&WaitAndSetEvent, &third, &fourth));

  // All tasks have been posted before the first one is unblocked.
  first.Set();
  // Only if the chain is invoked in posted order will the last event be set.
  fourth.Wait(Event::kForever);
}

}  // namespace rtc
