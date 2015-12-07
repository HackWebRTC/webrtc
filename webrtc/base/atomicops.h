/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_ATOMICOPS_H_
#define WEBRTC_BASE_ATOMICOPS_H_

#if defined(WEBRTC_WIN)
// Include winsock2.h before including <windows.h> to maintain consistency with
// win32.h.  We can't include win32.h directly here since it pulls in
// headers such as basictypes.h which causes problems in Chromium where webrtc
// exists as two separate projects, webrtc and libjingle.
#include <winsock2.h>
#include <windows.h>
#endif  // defined(WEBRTC_WIN)

namespace rtc {

class AtomicOps {
 public:
#if defined(WEBRTC_WIN)
  // Assumes sizeof(int) == sizeof(LONG), which it is on Win32 and Win64.
  static int Increment(volatile int* i) {
    return ::InterlockedIncrement(reinterpret_cast<volatile LONG*>(i));
  }
  static int Decrement(volatile int* i) {
    return ::InterlockedDecrement(reinterpret_cast<volatile LONG*>(i));
  }
  static int AcquireLoad(volatile const int* i) {
    return *i;
  }
  static void ReleaseStore(volatile int* i, int value) {
    *i = value;
  }
  static int CompareAndSwap(volatile int* i, int old_value, int new_value) {
    return ::InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(i),
                                        new_value,
                                        old_value);
  }
  // Pointer variants.
  template <typename T>
  static T* AtomicLoadPtr(T* volatile* ptr) {
    return *ptr;
  }
  template <typename T>
  static T* CompareAndSwapPtr(T* volatile* ptr, T* old_value, T* new_value) {
    return static_cast<T*>(::InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID volatile*>(ptr), old_value, new_value));
  }
#else
  static int Increment(volatile int* i) {
    return __sync_add_and_fetch(i, 1);
  }
  static int Decrement(volatile int* i) {
    return __sync_sub_and_fetch(i, 1);
  }
  static int AcquireLoad(volatile const int* i) {
    return __atomic_load_n(i, __ATOMIC_ACQUIRE);
  }
  static void ReleaseStore(volatile int* i, int value) {
    __atomic_store_n(i, value, __ATOMIC_RELEASE);
  }
  static int CompareAndSwap(volatile int* i, int old_value, int new_value) {
    return __sync_val_compare_and_swap(i, old_value, new_value);
  }
  // Pointer variants.
  template <typename T>
  static T* AtomicLoadPtr(T* volatile* ptr) {
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
  }
  template <typename T>
  static T* CompareAndSwapPtr(T* volatile* ptr, T* old_value, T* new_value) {
    return __sync_val_compare_and_swap(ptr, old_value, new_value);
  }
#endif
};

// POD struct version of AtomicOps, prevents accidental non-atomic operator
// usage (such as ++, -- or =). Functions are static, so that the AtomicInt::
// prefix must be present in the code, clearly labeling the operations as
// atomic.
// Do not copy-initialize, since that performs non-atomic reads of value_. The
// copy constructor needs to be present for brace initialization.
struct AtomicInt {
  AtomicInt() = delete;
  // TODO(pbos): When MSVC allows brace initialization (or we move to
  // std::atomic), remove copy constructor (or have it implicitly removed by
  // std::atomic).
  void operator=(const AtomicInt&) = delete;

  // value_ is public to permit brace initialization. Should not be accessed
  // directly.
  volatile int value_;

  // Atomically increments |i|, returns the resulting incremented value.
  static int Increment(AtomicInt* i) {
    return AtomicOps::Increment(&i->value_);
  }

  // Atomically decrements |i|, returns the resulting decremented value.
  static int Decrement(AtomicInt* i) {
    return AtomicOps::Decrement(&i->value_);
  }

  // Atomically loads |i|.
  static int AcquireLoad(const AtomicInt* i) {
    return AtomicOps::AcquireLoad(&i->value_);
  }

  // Atomically stores |value| in |i|.
  static void ReleaseStore(AtomicInt* i, int value) {
    AtomicOps::ReleaseStore(&i->value_, value);
  }

  // Attempts to compare-and-swaps |old_value| for |new_value| in |i| , returns
  // |i|'s initial value. If equal to |old_value|, then the CAS succeeded,
  // otherwise no operation is performed.
  static int CompareAndSwap(AtomicInt* i, int old_value, int new_value) {
    return AtomicOps::CompareAndSwap(&i->value_, old_value, new_value);
  }
};
}

#endif  // WEBRTC_BASE_ATOMICOPS_H_
