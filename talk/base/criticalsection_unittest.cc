/*
 * libjingle
 * Copyright 2014, Google Inc.
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

#include <set>
#include <vector>

#include "talk/base/criticalsection.h"
#include "talk/base/event.h"
#include "talk/base/gunit.h"
#include "talk/base/scopedptrcollection.h"
#include "talk/base/thread.h"

namespace talk_base {

namespace {

const int kLongTime = 10000;  // 10 seconds
const int kNumThreads = 16;
const int kOperationsToRun = 1000;

template <class T>
class AtomicOpRunner : public MessageHandler {
 public:
  explicit AtomicOpRunner(int initial_value)
      : value_(initial_value),
        threads_active_(0),
        start_event_(true, false),
        done_event_(true, false) {}

  int value() const { return value_; }

  bool Run() {
    // Signal all threads to start.
    start_event_.Set();

    // Wait for all threads to finish.
    return done_event_.Wait(kLongTime);
  }

  void SetExpectedThreadCount(int count) {
    threads_active_ = count;
  }

  virtual void OnMessage(Message* msg) {
    std::vector<int> values;
    values.reserve(kOperationsToRun);

    // Wait to start.
    ASSERT_TRUE(start_event_.Wait(kLongTime));

    // Generate a bunch of values by updating value_ atomically.
    for (int i = 0; i < kOperationsToRun; ++i) {
      values.push_back(T::AtomicOp(&value_));
    }

    { // Add them all to the set.
      CritScope cs(&all_values_crit_);
      for (size_t i = 0; i < values.size(); ++i) {
        std::pair<std::set<int>::iterator, bool> result =
            all_values_.insert(values[i]);
        // Each value should only be taken by one thread, so if this value
        // has already been added, something went wrong.
        EXPECT_TRUE(result.second)
            << "Thread=" << Thread::Current() << " value=" << values[i];
      }
    }

    // Signal that we're done.
    if (AtomicOps::Decrement(&threads_active_) == 0) {
      done_event_.Set();
    }
  }

 private:
  int value_;
  int threads_active_;
  CriticalSection all_values_crit_;
  std::set<int> all_values_;
  Event start_event_;
  Event done_event_;
};

struct IncrementOp {
  static int AtomicOp(int* i) { return AtomicOps::Increment(i); }
};

struct DecrementOp {
  static int AtomicOp(int* i) { return AtomicOps::Decrement(i); }
};

void StartThreads(ScopedPtrCollection<Thread>* threads,
                  MessageHandler* handler) {
  for (int i = 0; i < kNumThreads; ++i) {
    Thread* thread = new Thread();
    thread->Start();
    thread->Post(handler);
    threads->PushBack(thread);
  }
}

}  // namespace

TEST(AtomicOpsTest, Simple) {
  int value = 0;
  EXPECT_EQ(1, AtomicOps::Increment(&value));
  EXPECT_EQ(1, value);
  EXPECT_EQ(2, AtomicOps::Increment(&value));
  EXPECT_EQ(2, value);
  EXPECT_EQ(1, AtomicOps::Decrement(&value));
  EXPECT_EQ(1, value);
  EXPECT_EQ(0, AtomicOps::Decrement(&value));
  EXPECT_EQ(0, value);
}

TEST(AtomicOpsTest, Increment) {
  // Create and start lots of threads.
  AtomicOpRunner<IncrementOp> runner(0);
  ScopedPtrCollection<Thread> threads;
  StartThreads(&threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);

  // Release the hounds!
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(kOperationsToRun * kNumThreads, runner.value());
}

TEST(AtomicOpsTest, Decrement) {
  // Create and start lots of threads.
  AtomicOpRunner<DecrementOp> runner(kOperationsToRun * kNumThreads);
  ScopedPtrCollection<Thread> threads;
  StartThreads(&threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);

  // Release the hounds!
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(0, runner.value());
}

}  // namespace talk_base
