/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#include "talk/base/messagequeue.h"

#include "talk/base/bind.h"
#include "talk/base/gunit.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"
#include "talk/base/timeutils.h"
#include "talk/base/nullsocketserver.h"

using namespace talk_base;

class MessageQueueTest: public testing::Test, public MessageQueue {
 public:
  bool IsLocked_Worker() {
    if (!crit_.TryEnter()) {
      return true;
    }
    crit_.Leave();
    return false;
  }
  bool IsLocked() {
    // We have to do this on a worker thread, or else the TryEnter will
    // succeed, since our critical sections are reentrant.
    Thread worker;
    worker.Start();
    return worker.Invoke<bool>(
        talk_base::Bind(&MessageQueueTest::IsLocked_Worker, this));
  }
};

struct DeletedLockChecker {
  DeletedLockChecker(MessageQueueTest* test, bool* was_locked, bool* deleted)
      : test(test), was_locked(was_locked), deleted(deleted) { }
  ~DeletedLockChecker() {
    *deleted = true;
    *was_locked = test->IsLocked();
  }
  MessageQueueTest* test;
  bool* was_locked;
  bool* deleted;
};

static void DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(
    MessageQueue* q) {
  EXPECT_TRUE(q != NULL);
  TimeStamp now = Time();
  q->PostAt(now, NULL, 3);
  q->PostAt(now - 2, NULL, 0);
  q->PostAt(now - 1, NULL, 1);
  q->PostAt(now, NULL, 4);
  q->PostAt(now - 1, NULL, 2);

  Message msg;
  for (size_t i=0; i<5; ++i) {
    memset(&msg, 0, sizeof(msg));
    EXPECT_TRUE(q->Get(&msg, 0));
    EXPECT_EQ(i, msg.message_id);
  }

  EXPECT_FALSE(q->Get(&msg, 0));  // No more messages
}

TEST_F(MessageQueueTest,
       DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder) {
  MessageQueue q;
  DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(&q);
  NullSocketServer nullss;
  MessageQueue q_nullss(&nullss);
  DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(&q_nullss);
}

TEST_F(MessageQueueTest, DisposeNotLocked) {
  bool was_locked = true;
  bool deleted = false;
  DeletedLockChecker* d = new DeletedLockChecker(this, &was_locked, &deleted);
  Dispose(d);
  Message msg;
  EXPECT_FALSE(Get(&msg, 0));
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(was_locked);
}

class DeletedMessageHandler : public MessageHandler {
 public:
  explicit DeletedMessageHandler(bool* deleted) : deleted_(deleted) { }
  ~DeletedMessageHandler() {
    *deleted_ = true;
  }
  void OnMessage(Message* msg) { }
 private:
  bool* deleted_;
};

TEST_F(MessageQueueTest, DiposeHandlerWithPostedMessagePending) {
  bool deleted = false;
  DeletedMessageHandler *handler = new DeletedMessageHandler(&deleted);
  // First, post a dispose.
  Dispose(handler);
  // Now, post a message, which should *not* be returned by Get().
  Post(handler, 1);
  Message msg;
  EXPECT_FALSE(Get(&msg, 0));
  EXPECT_TRUE(deleted);
}

