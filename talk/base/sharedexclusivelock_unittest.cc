/*
 * libjingle
 * Copyright 2011, Google Inc.
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
#include "talk/base/messagehandler.h"
#include "talk/base/messagequeue.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sharedexclusivelock.h"
#include "talk/base/thread.h"
#include "talk/base/timeutils.h"

namespace talk_base {

static const uint32 kMsgRead = 0;
static const uint32 kMsgWrite = 0;
static const int kNoWaitThresholdInMs = 10;
static const int kWaitThresholdInMs = 80;
static const int kProcessTimeInMs = 100;
static const int kProcessTimeoutInMs = 5000;

class SharedExclusiveTask : public MessageHandler {
 public:
  SharedExclusiveTask(SharedExclusiveLock* shared_exclusive_lock,
                      int* value,
                      bool* done)
      : shared_exclusive_lock_(shared_exclusive_lock),
        waiting_time_in_ms_(0),
        value_(value),
        done_(done) {
    worker_thread_.reset(new Thread());
    worker_thread_->Start();
  }

  int waiting_time_in_ms() const { return waiting_time_in_ms_; }

 protected:
  scoped_ptr<Thread> worker_thread_;
  SharedExclusiveLock* shared_exclusive_lock_;
  int waiting_time_in_ms_;
  int* value_;
  bool* done_;
};

class ReadTask : public SharedExclusiveTask {
 public:
  ReadTask(SharedExclusiveLock* shared_exclusive_lock, int* value, bool* done)
      : SharedExclusiveTask(shared_exclusive_lock, value, done) {
  }

  void PostRead(int* value) {
    worker_thread_->Post(this, kMsgRead, new TypedMessageData<int*>(value));
  }

 private:
  virtual void OnMessage(Message* message) {
    ASSERT(talk_base::Thread::Current() == worker_thread_.get());
    ASSERT(message != NULL);
    ASSERT(message->message_id == kMsgRead);

    TypedMessageData<int*>* message_data =
        static_cast<TypedMessageData<int*>*>(message->pdata);

    uint32 start_time = Time();
    {
      SharedScope ss(shared_exclusive_lock_);
      waiting_time_in_ms_ = TimeDiff(Time(), start_time);

      Thread::SleepMs(kProcessTimeInMs);
      *message_data->data() = *value_;
      *done_ = true;
    }
    delete message->pdata;
    message->pdata = NULL;
  }
};

class WriteTask : public SharedExclusiveTask {
 public:
  WriteTask(SharedExclusiveLock* shared_exclusive_lock, int* value, bool* done)
      : SharedExclusiveTask(shared_exclusive_lock, value, done) {
  }

  void PostWrite(int value) {
    worker_thread_->Post(this, kMsgWrite, new TypedMessageData<int>(value));
  }

 private:
  virtual void OnMessage(Message* message) {
    ASSERT(talk_base::Thread::Current() == worker_thread_.get());
    ASSERT(message != NULL);
    ASSERT(message->message_id == kMsgWrite);

    TypedMessageData<int>* message_data =
        static_cast<TypedMessageData<int>*>(message->pdata);

    uint32 start_time = Time();
    {
      ExclusiveScope es(shared_exclusive_lock_);
      waiting_time_in_ms_ = TimeDiff(Time(), start_time);

      Thread::SleepMs(kProcessTimeInMs);
      *value_ = message_data->data();
      *done_ = true;
    }
    delete message->pdata;
    message->pdata = NULL;
  }
};

// Unit test for SharedExclusiveLock.
class SharedExclusiveLockTest
    : public testing::Test {
 public:
  SharedExclusiveLockTest() : value_(0) {
  }

  virtual void SetUp() {
    shared_exclusive_lock_.reset(new SharedExclusiveLock());
  }

 protected:
  scoped_ptr<SharedExclusiveLock> shared_exclusive_lock_;
  int value_;
};

TEST_F(SharedExclusiveLockTest, TestSharedShared) {
  int value0, value1;
  bool done0, done1;
  ReadTask reader0(shared_exclusive_lock_.get(), &value_, &done0);
  ReadTask reader1(shared_exclusive_lock_.get(), &value_, &done1);

  // Test shared locks can be shared without waiting.
  {
    SharedScope ss(shared_exclusive_lock_.get());
    value_ = 1;
    done0 = false;
    done1 = false;
    reader0.PostRead(&value0);
    reader1.PostRead(&value1);
    Thread::SleepMs(kProcessTimeInMs);
  }

  EXPECT_TRUE_WAIT(done0, kProcessTimeoutInMs);
  EXPECT_EQ(1, value0);
  EXPECT_LE(reader0.waiting_time_in_ms(), kNoWaitThresholdInMs);
  EXPECT_TRUE_WAIT(done1, kProcessTimeoutInMs);
  EXPECT_EQ(1, value1);
  EXPECT_LE(reader1.waiting_time_in_ms(), kNoWaitThresholdInMs);
}

TEST_F(SharedExclusiveLockTest, TestSharedExclusive) {
  bool done;
  WriteTask writer(shared_exclusive_lock_.get(), &value_, &done);

  // Test exclusive lock needs to wait for shared lock.
  {
    SharedScope ss(shared_exclusive_lock_.get());
    value_ = 1;
    done = false;
    writer.PostWrite(2);
    Thread::SleepMs(kProcessTimeInMs);
    EXPECT_EQ(1, value_);
  }

  EXPECT_TRUE_WAIT(done, kProcessTimeoutInMs);
  EXPECT_EQ(2, value_);
  EXPECT_GE(writer.waiting_time_in_ms(), kWaitThresholdInMs);
}

TEST_F(SharedExclusiveLockTest, TestExclusiveShared) {
  int value;
  bool done;
  ReadTask reader(shared_exclusive_lock_.get(), &value_, &done);

  // Test shared lock needs to wait for exclusive lock.
  {
    ExclusiveScope es(shared_exclusive_lock_.get());
    value_ = 1;
    done = false;
    reader.PostRead(&value);
    Thread::SleepMs(kProcessTimeInMs);
    value_ = 2;
  }

  EXPECT_TRUE_WAIT(done, kProcessTimeoutInMs);
  EXPECT_EQ(2, value);
  EXPECT_GE(reader.waiting_time_in_ms(), kWaitThresholdInMs);
}

TEST_F(SharedExclusiveLockTest, TestExclusiveExclusive) {
  bool done;
  WriteTask writer(shared_exclusive_lock_.get(), &value_, &done);

  // Test exclusive lock needs to wait for exclusive lock.
  {
    ExclusiveScope es(shared_exclusive_lock_.get());
    value_ = 1;
    done = false;
    writer.PostWrite(2);
    Thread::SleepMs(kProcessTimeInMs);
    EXPECT_EQ(1, value_);
  }

  EXPECT_TRUE_WAIT(done, kProcessTimeoutInMs);
  EXPECT_EQ(2, value_);
  EXPECT_GE(writer.waiting_time_in_ms(), kWaitThresholdInMs);
}

}  // namespace talk_base
