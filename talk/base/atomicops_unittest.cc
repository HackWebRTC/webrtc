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

#if !defined(__arm__)
// For testing purposes, define faked versions of the atomic operations
#include "talk/base/basictypes.h"
namespace talk_base {
typedef uint32 Atomic32;
static inline void MemoryBarrier() { }
static inline void AtomicIncrement(volatile Atomic32* ptr) {
  *ptr = *ptr + 1;
}
}
#define SKIP_ATOMIC_CHECK
#endif

#include "talk/base/atomicops.h"
#include "talk/base/gunit.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"

TEST(FixedSizeLockFreeQueueTest, TestDefaultConstruct) {
  talk_base::FixedSizeLockFreeQueue<int> queue;
  EXPECT_EQ(0u, queue.capacity());
  EXPECT_EQ(0u, queue.Size());
  EXPECT_FALSE(queue.PushBack(1));
  int val;
  EXPECT_FALSE(queue.PopFront(&val));
}

TEST(FixedSizeLockFreeQueueTest, TestConstruct) {
  talk_base::FixedSizeLockFreeQueue<int> queue(5);
  EXPECT_EQ(5u, queue.capacity());
  EXPECT_EQ(0u, queue.Size());
  int val;
  EXPECT_FALSE(queue.PopFront(&val));
}

TEST(FixedSizeLockFreeQueueTest, TestPushPop) {
  talk_base::FixedSizeLockFreeQueue<int> queue(2);
  EXPECT_EQ(2u, queue.capacity());
  EXPECT_EQ(0u, queue.Size());
  EXPECT_TRUE(queue.PushBack(1));
  EXPECT_EQ(1u, queue.Size());
  EXPECT_TRUE(queue.PushBack(2));
  EXPECT_EQ(2u, queue.Size());
  EXPECT_FALSE(queue.PushBack(3));
  EXPECT_EQ(2u, queue.Size());
  int val;
  EXPECT_TRUE(queue.PopFront(&val));
  EXPECT_EQ(1, val);
  EXPECT_EQ(1u, queue.Size());
  EXPECT_TRUE(queue.PopFront(&val));
  EXPECT_EQ(2, val);
  EXPECT_EQ(0u, queue.Size());
  EXPECT_FALSE(queue.PopFront(&val));
  EXPECT_EQ(0u, queue.Size());
}

TEST(FixedSizeLockFreeQueueTest, TestResize) {
  talk_base::FixedSizeLockFreeQueue<int> queue(2);
  EXPECT_EQ(2u, queue.capacity());
  EXPECT_EQ(0u, queue.Size());
  EXPECT_TRUE(queue.PushBack(1));
  EXPECT_EQ(1u, queue.Size());

  queue.ClearAndResizeUnsafe(5);
  EXPECT_EQ(5u, queue.capacity());
  EXPECT_EQ(0u, queue.Size());
  int val;
  EXPECT_FALSE(queue.PopFront(&val));
}
