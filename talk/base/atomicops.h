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

#ifndef TALK_BASE_ATOMICOPS_H_
#define TALK_BASE_ATOMICOPS_H_

#include <string>

#include "talk/base/basictypes.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"

namespace talk_base {

// A single-producer, single-consumer, fixed-size queue.
// All methods not ending in Unsafe can be safely called without locking,
// provided that calls to consumer methods (Peek/Pop) or producer methods (Push)
// only happen on a single thread per method type. If multiple threads need to
// read simultaneously or write simultaneously, other synchronization is
// necessary. Synchronization is also required if a call into any Unsafe method
// could happen at the same time as a call to any other method.
template <typename T>
class FixedSizeLockFreeQueue {
 private:
// Atomic primitives and memory barrier
#if defined(__arm__)
  typedef uint32 Atomic32;

  // Copied from google3/base/atomicops-internals-arm-v6plus.h
  static inline void MemoryBarrier() {
    asm volatile("dmb":::"memory");
  }

  // Adapted from google3/base/atomicops-internals-arm-v6plus.h
  static inline void AtomicIncrement(volatile Atomic32* ptr) {
    Atomic32 str_success, value;
    asm volatile (
        "1:\n"
        "ldrex  %1, [%2]\n"
        "add    %1, %1, #1\n"
        "strex  %0, %1, [%2]\n"
        "teq    %0, #0\n"
        "bne    1b"
        : "=&r"(str_success), "=&r"(value)
        : "r" (ptr)
        : "cc", "memory");
  }
#elif !defined(SKIP_ATOMIC_CHECK)
#error "No atomic operations defined for the given architecture."
#endif

 public:
  // Constructs an empty queue, with capacity 0.
  FixedSizeLockFreeQueue() : pushed_count_(0),
                             popped_count_(0),
                             capacity_(0),
                             data_() {}
  // Constructs an empty queue with the given capacity.
  FixedSizeLockFreeQueue(size_t capacity) : pushed_count_(0),
                                            popped_count_(0),
                                            capacity_(capacity),
                                            data_(new T[capacity]) {}

  // Pushes a value onto the queue. Returns true if the value was successfully
  // pushed (there was space in the queue). This method can be safely called at
  // the same time as PeekFront/PopFront.
  bool PushBack(T value) {
    if (capacity_ == 0) {
      LOG(LS_WARNING) << "Queue capacity is 0.";
      return false;
    }
    if (IsFull()) {
      return false;
    }

    data_[pushed_count_ % capacity_] = value;
    // Make sure the data is written before the count is incremented, so other
    // threads can't see the value exists before being able to read it.
    MemoryBarrier();
    AtomicIncrement(&pushed_count_);
    return true;
  }

  // Retrieves the oldest value pushed onto the queue. Returns true if there was
  // an item to peek (the queue was non-empty). This method can be safely called
  // at the same time as PushBack.
  bool PeekFront(T* value_out) {
    if (capacity_ == 0) {
      LOG(LS_WARNING) << "Queue capacity is 0.";
      return false;
    }
    if (IsEmpty()) {
      return false;
    }

    *value_out = data_[popped_count_ % capacity_];
    return true;
  }

  // Retrieves the oldest value pushed onto the queue and removes it from the
  // queue. Returns true if there was an item to pop (the queue was non-empty).
  // This method can be safely called at the same time as PushBack.
  bool PopFront(T* value_out) {
    if (PeekFront(value_out)) {
      AtomicIncrement(&popped_count_);
      return true;
    }
    return false;
  }

  // Clears the current items in the queue and sets the new (fixed) size. This
  // method cannot be called at the same time as any other method.
  void ClearAndResizeUnsafe(int new_capacity) {
    capacity_ = new_capacity;
    data_.reset(new T[new_capacity]);
    pushed_count_ = 0;
    popped_count_ = 0;
  }

  // Returns true if there is no space left in the queue for new elements.
  int IsFull() const { return pushed_count_ == popped_count_ + capacity_; }
  // Returns true if there are no elements in the queue.
  int IsEmpty() const { return pushed_count_ == popped_count_; }
  // Returns the current number of elements in the queue. This is always in the
  // range [0, capacity]
  size_t Size() const { return pushed_count_ - popped_count_; }

  // Returns the capacity of the queue (max size).
  size_t capacity() const { return capacity_; }

 private:
  volatile Atomic32 pushed_count_;
  volatile Atomic32 popped_count_;
  size_t capacity_;
  talk_base::scoped_ptr<T[]> data_;
  DISALLOW_COPY_AND_ASSIGN(FixedSizeLockFreeQueue);
};

}

#endif  // TALK_BASE_ATOMICOPS_H_
