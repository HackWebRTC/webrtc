/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_WIN)
// clang-format off
#include <windows.h>  // Must come first.
#include <mmsystem.h>
// clang-format on
#endif

#include <stdint.h>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "rtc_base/bind.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/time_utils.h"
#include "test/gtest.h"

namespace rtc {

namespace {
// Noop on all platforms except Windows, where it turns on high precision
// multimedia timers which increases the precision of TimeMillis() while in
// scope.
class EnableHighResTimers {
 public:
#if !defined(WEBRTC_WIN)
  EnableHighResTimers() {}
#else
  EnableHighResTimers() : enabled_(timeBeginPeriod(1) == TIMERR_NOERROR) {}
  ~EnableHighResTimers() {
    if (enabled_)
      timeEndPeriod(1);
  }

 private:
  const bool enabled_;
#endif
};

void CheckCurrent(Event* signal, TaskQueue* queue) {
  EXPECT_TRUE(queue->IsCurrent());
  if (signal)
    signal->Set();
}

}  // namespace

// This task needs to be run manually due to the slowness of some of our bots.
// TODO(tommi): Can we run this on the perf bots?
TEST(TaskQueueTest, DISABLED_PostDelayedHighRes) {
  EnableHighResTimers high_res_scope;

  static const char kQueueName[] = "PostDelayedHighRes";
  Event event;
  TaskQueue queue(kQueueName, TaskQueue::Priority::HIGH);

  uint32_t start = Time();
  queue.PostDelayedTask(Bind(&CheckCurrent, &event, &queue), 3);
  EXPECT_TRUE(event.Wait(1000));
  uint32_t end = TimeMillis();
  // These tests are a little relaxed due to how "powerful" our test bots can
  // be.  Most recently we've seen windows bots fire the callback after 94-99ms,
  // which is why we have a little bit of leeway backwards as well.
  EXPECT_GE(end - start, 3u);
  EXPECT_NEAR(end - start, 3, 3u);
}

// TODO(danilchap): Reshape and rename tests below to show they are verifying
// rtc::NewClosure helper rather than TaskQueue implementation.
TEST(TaskQueueTest, PostLambda) {
  TaskQueue queue("PostLambda");
  Event ran;
  queue.PostTask([&ran] { ran.Set(); });
  EXPECT_TRUE(ran.Wait(1000));
}

TEST(TaskQueueTest, PostCopyableClosure) {
  struct CopyableClosure {
    CopyableClosure(int* num_copies, int* num_moves, Event* event)
        : num_copies(num_copies), num_moves(num_moves), event(event) {}
    CopyableClosure(const CopyableClosure& other)
        : num_copies(other.num_copies),
          num_moves(other.num_moves),
          event(other.event) {
      ++*num_copies;
    }
    CopyableClosure(CopyableClosure&& other)
        : num_copies(other.num_copies),
          num_moves(other.num_moves),
          event(other.event) {
      ++*num_moves;
    }
    void operator()() { event->Set(); }

    int* num_copies;
    int* num_moves;
    Event* event;
  };

  int num_copies = 0;
  int num_moves = 0;
  Event event;

  static const char kPostQueue[] = "PostCopyableClosure";
  TaskQueue post_queue(kPostQueue);
  {
    CopyableClosure closure(&num_copies, &num_moves, &event);
    post_queue.PostTask(closure);
    // Destroy closure to check with msan and tsan posted task has own copy.
  }

  EXPECT_TRUE(event.Wait(1000));
  EXPECT_EQ(num_copies, 1);
  EXPECT_EQ(num_moves, 0);
}

TEST(TaskQueueTest, PostMoveOnlyClosure) {
  struct SomeState {
    explicit SomeState(Event* event) : event(event) {}
    ~SomeState() { event->Set(); }
    Event* event;
  };
  struct MoveOnlyClosure {
    MoveOnlyClosure(int* num_moves, std::unique_ptr<SomeState> state)
        : num_moves(num_moves), state(std::move(state)) {}
    MoveOnlyClosure(const MoveOnlyClosure&) = delete;
    MoveOnlyClosure(MoveOnlyClosure&& other)
        : num_moves(other.num_moves), state(std::move(other.state)) {
      ++*num_moves;
    }
    void operator()() { state.reset(); }

    int* num_moves;
    std::unique_ptr<SomeState> state;
  };

  int num_moves = 0;
  Event event;
  std::unique_ptr<SomeState> state(new SomeState(&event));

  static const char kPostQueue[] = "PostMoveOnlyClosure";
  TaskQueue post_queue(kPostQueue);
  post_queue.PostTask(MoveOnlyClosure(&num_moves, std::move(state)));

  EXPECT_TRUE(event.Wait(1000));
  EXPECT_EQ(num_moves, 1);
}

TEST(TaskQueueTest, PostMoveOnlyCleanup) {
  struct SomeState {
    explicit SomeState(Event* event) : event(event) {}
    ~SomeState() { event->Set(); }
    Event* event;
  };
  struct MoveOnlyClosure {
    void operator()() { state.reset(); }

    std::unique_ptr<SomeState> state;
  };

  Event event_run;
  Event event_cleanup;
  std::unique_ptr<SomeState> state_run(new SomeState(&event_run));
  std::unique_ptr<SomeState> state_cleanup(new SomeState(&event_cleanup));

  static const char kPostQueue[] = "PostMoveOnlyCleanup";
  TaskQueue post_queue(kPostQueue);
  post_queue.PostTask(NewClosure(MoveOnlyClosure{std::move(state_run)},
                                 MoveOnlyClosure{std::move(state_cleanup)}));

  EXPECT_TRUE(event_cleanup.Wait(1000));
  // Expect run closure to complete before cleanup closure.
  EXPECT_TRUE(event_run.Wait(0));
}

}  // namespace rtc
