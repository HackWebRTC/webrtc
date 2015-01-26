/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Borrowed from Chromium's src/base/threading/thread_checker_impl.cc.

#include "webrtc/base/thread_checker_impl.h"

#include "webrtc/base/checks.h"

#if defined(WEBRTC_LINUX)
#include <sys/syscall.h>
#endif

namespace rtc {
namespace {
PlatformThreadId CurrentThreadId() {
#if defined(WEBRTC_WIN)
  return GetCurrentThreadId();
#elif defined(WEBRTC_POSIX)
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if defined(WEBRTC_MAC)
  return pthread_mach_thread_np(pthread_self());
#elif defined(WEBRTC_LINUX)
  return syscall(__NR_gettid);
#elif defined(WEBRTC_ANDROID)
  return gettid();
#else
  // Default implementation for nacl and solaris.
  return reinterpret_cast<pid_t>(pthread_self());
#endif
#endif  // defined(WEBRTC_POSIX)
}
}  // namespace

ThreadCheckerImpl::ThreadCheckerImpl() : valid_thread_(CurrentThreadId()) {
}

ThreadCheckerImpl::~ThreadCheckerImpl() {
}

bool ThreadCheckerImpl::CalledOnValidThread() const {
  CritScope scoped_lock(&lock_);
  const PlatformThreadId current_thread = CurrentThreadId();
  if (!valid_thread_)  // Set if previously detached.
    valid_thread_ = current_thread;
  return valid_thread_ == current_thread;
}

void ThreadCheckerImpl::DetachFromThread() {
  CritScope scoped_lock(&lock_);
  valid_thread_ = 0;
}

}  // namespace rtc
