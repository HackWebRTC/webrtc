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

PlatformThreadId CurrentThreadId() {
  PlatformThreadId ret;
#if defined(WEBRTC_WIN)
  ret = GetCurrentThreadId();
#elif defined(WEBRTC_POSIX)
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
  ret = pthread_mach_thread_np(pthread_self());
#elif defined(WEBRTC_LINUX)
  ret =  syscall(__NR_gettid);
#elif defined(WEBRTC_ANDROID)
  ret = gettid();
#else
  // Default implementation for nacl and solaris.
  ret = reinterpret_cast<pid_t>(pthread_self());
#endif
#endif  // defined(WEBRTC_POSIX)
  DCHECK(ret);
  return ret;
}

ThreadCheckerImpl::ThreadCheckerImpl() : valid_thread_(CurrentThreadId()) {
}

ThreadCheckerImpl::~ThreadCheckerImpl() {
}

bool ThreadCheckerImpl::CalledOnValidThread() const {
  const PlatformThreadId current_thread = CurrentThreadId();
  CritScope scoped_lock(&lock_);
  if (!valid_thread_)  // Set if previously detached.
    valid_thread_ = current_thread;
#if defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
  // TODO(tommi): Remove this hack after we've figured out the roll issue
  // with chromium's Mac 10.9 debug bot.
  if (valid_thread_ != current_thread) {
    // At the moment, this file cannot use logging from either webrtc or
    // libjingle. :(
    printf("*** WRONG THREAD *** current=%i vs valid=%i\n",
        current_thread, valid_thread_);
  }
  return true;  // le sigh.
#else
  return valid_thread_ == current_thread;
#endif
}

void ThreadCheckerImpl::DetachFromThread() {
  CritScope scoped_lock(&lock_);
  valid_thread_ = 0;
}

}  // namespace rtc
