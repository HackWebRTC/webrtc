/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/messagequeue.h"

#include "webrtc/base/bind.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/nullsocketserver.h"
#include "webrtc/test/testsupport/gtest_disable.h"

using namespace rtc;

class MessageQueueForTest : public MessageQueue {
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
        rtc::Bind(&MessageQueueForTest::IsLocked_Worker, this));
  }

  size_t GetDmsgqSize() {
    return dmsgq_.size();
  }

  const DelayedMessage& GetDmsgqTop() {
    return dmsgq_.top();
  }
};

class MessageQueueTest : public testing::Test {
 protected:
  MessageQueueForTest q_;
};

struct DeletedLockChecker {
  DeletedLockChecker(MessageQueueForTest* q, bool* was_locked, bool* deleted)
      : q_(q), was_locked(was_locked), deleted(deleted) { }
  ~DeletedLockChecker() {
    *deleted = true;
    *was_locked = q_->IsLocked();
  }
  MessageQueueForTest* q_;
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
  DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(&q_);
  NullSocketServer nullss;
  MessageQueue q_nullss(&nullss);
  DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(&q_nullss);
}

TEST_F(MessageQueueTest, DisposeNotLocked) {
  bool was_locked = true;
  bool deleted = false;
  DeletedLockChecker* d = new DeletedLockChecker(&q_, &was_locked, &deleted);
  q_.Dispose(d);
  Message msg;
  EXPECT_FALSE(q_.Get(&msg, 0));
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

// TODO(decurtis): Test that ordering of elements is done properly.
// TODO(decurtis): Test that timestamps are being properly set.

TEST_F(MessageQueueTest, DisposeHandlerWithPostedMessagePending) {
  bool deleted = false;
  DeletedMessageHandler *handler = new DeletedMessageHandler(&deleted);
  // First, post a dispose.
  q_.Dispose(handler);
  // Now, post a message, which should *not* be returned by Get().
  q_.Post(handler, 1);
  Message msg;
  EXPECT_FALSE(q_.Get(&msg, 0));
  EXPECT_TRUE(deleted);
}

// Test Clear for removing messages that have been posted for times in
// the past.
TEST_F(MessageQueueTest, ClearPast) {
  TimeStamp now = Time();
  Message msg;

  // Test removing the only element.
  q_.PostAt(now - 4, NULL, 1);
  q_.Clear(NULL, 1, NULL);

  // Make sure the queue is empty now.
  EXPECT_FALSE(q_.Get(&msg, 0));

  // Test removing the one element with a two element list.
  q_.PostAt(now - 4, NULL, 1);
  q_.PostAt(now - 2, NULL, 3);

  q_.Clear(NULL, 1, NULL);

  EXPECT_TRUE(q_.Get(&msg, 0));
  EXPECT_EQ(3U, msg.message_id);

  // Make sure the queue is empty now.
  EXPECT_FALSE(q_.Get(&msg, 0));


  // Test removing the three element with a two element list.
  q_.PostAt(now - 4, NULL, 1);
  q_.PostAt(now - 2, NULL, 3);

  q_.Clear(NULL, 3, NULL);

  EXPECT_TRUE(q_.Get(&msg, 0));
  EXPECT_EQ(1U, msg.message_id);

  // Make sure the queue is empty now.
  EXPECT_FALSE(q_.Get(&msg, 0));


  // Test removing the two element in a three element list.
  q_.PostAt(now - 4, NULL, 1);
  q_.PostAt(now - 3, NULL, 2);
  q_.PostAt(now - 2, NULL, 3);

  q_.Clear(NULL, 2, NULL);

  EXPECT_TRUE(q_.Get(&msg, 0));
  EXPECT_EQ(1U, msg.message_id);

  EXPECT_TRUE(q_.Get(&msg, 0));
  EXPECT_EQ(3U, msg.message_id);

  // Make sure the queue is empty now.
  EXPECT_FALSE(q_.Get(&msg, 0));


  // Test not clearing any messages.
  q_.PostAt(now - 4, NULL, 1);
  q_.PostAt(now - 3, NULL, 2);
  q_.PostAt(now - 2, NULL, 3);

  // Remove nothing.
  q_.Clear(NULL, 0, NULL);
  q_.Clear(NULL, 4, NULL);

  EXPECT_TRUE(q_.Get(&msg, 0));
  EXPECT_EQ(1U, msg.message_id);

  EXPECT_TRUE(q_.Get(&msg, 0));
  EXPECT_EQ(2U, msg.message_id);

  EXPECT_TRUE(q_.Get(&msg, 0));
  EXPECT_EQ(3U, msg.message_id);

  // Make sure the queue is empty now.
  EXPECT_FALSE(q_.Get(&msg, 0));
}

// Test clearing messages that have been posted for the future.
TEST_F(MessageQueueTest, ClearFuture) {
  EXPECT_EQ(0U, q_.GetDmsgqSize());
  q_.PostDelayed(10, NULL, 4);
  EXPECT_EQ(1U, q_.GetDmsgqSize());
  q_.PostDelayed(13, NULL, 4);
  EXPECT_EQ(2U, q_.GetDmsgqSize());
  q_.PostDelayed(9, NULL, 2);
  EXPECT_EQ(3U, q_.GetDmsgqSize());
  q_.PostDelayed(11, NULL, 10);
  EXPECT_EQ(4U, q_.GetDmsgqSize());

  EXPECT_EQ(9, q_.GetDmsgqTop().cmsDelay_);

  MessageList removed;
  q_.Clear(NULL, 10, &removed);
  EXPECT_EQ(1U, removed.size());
  EXPECT_EQ(3U, q_.GetDmsgqSize());

  removed.clear();
  q_.Clear(NULL, 4, &removed);
  EXPECT_EQ(2U, removed.size());
  EXPECT_EQ(1U, q_.GetDmsgqSize());

  removed.clear();
  q_.Clear(NULL, 4, &removed);
  EXPECT_EQ(0U, removed.size());
  EXPECT_EQ(1U, q_.GetDmsgqSize());

  removed.clear();
  q_.Clear(NULL, 2, &removed);
  EXPECT_EQ(1U, removed.size());
  EXPECT_EQ(0U, q_.GetDmsgqSize());

  Message msg;
  EXPECT_FALSE(q_.Get(&msg, 0));
}


struct UnwrapMainThreadScope {
  UnwrapMainThreadScope() : rewrap_(Thread::Current() != NULL) {
    if (rewrap_) ThreadManager::Instance()->UnwrapCurrentThread();
  }
  ~UnwrapMainThreadScope() {
    if (rewrap_) ThreadManager::Instance()->WrapCurrentThread();
  }
 private:
  bool rewrap_;
};

TEST(MessageQueueManager, DeletedHandler) {
  UnwrapMainThreadScope s;
  if (MessageQueueManager::IsInitialized()) {
    LOG(LS_INFO) << "Unable to run MessageQueueManager::Clear test, since the "
                 << "MessageQueueManager was already initialized by some "
                 << "other test in this run.";
    return;
  }
  bool deleted = false;
  DeletedMessageHandler* handler = new DeletedMessageHandler(&deleted);
  delete handler;
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(MessageQueueManager::IsInitialized());
}
