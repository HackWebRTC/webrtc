/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/platform_thread.h"

#include <string.h>

#include "webrtc/base/checks.h"

#if defined(WEBRTC_LINUX)
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif

namespace rtc {

PlatformThreadId CurrentThreadId() {
  PlatformThreadId ret;
#if defined(WEBRTC_WIN)
  ret = GetCurrentThreadId();
#elif defined(WEBRTC_POSIX)
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
  RTC_DCHECK(ret);
  return ret;
}

PlatformThreadRef CurrentThreadRef() {
#if defined(WEBRTC_WIN)
  return GetCurrentThreadId();
#elif defined(WEBRTC_POSIX)
  return pthread_self();
#endif
}

bool IsThreadRefEqual(const PlatformThreadRef& a, const PlatformThreadRef& b) {
#if defined(WEBRTC_WIN)
  return a == b;
#elif defined(WEBRTC_POSIX)
  return pthread_equal(a, b);
#endif
}

void SetCurrentThreadName(const char* name) {
  RTC_DCHECK(strlen(name) < 64);
#if defined(WEBRTC_WIN)
  struct {
    DWORD dwType;
    LPCSTR szName;
    DWORD dwThreadID;
    DWORD dwFlags;
  } threadname_info = {0x1000, name, static_cast<DWORD>(-1), 0};

  __try {
    ::RaiseException(0x406D1388, 0, sizeof(threadname_info) / sizeof(DWORD),
                     reinterpret_cast<ULONG_PTR*>(&threadname_info));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID)
  prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(name));
#elif defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
  pthread_setname_np(name);
#endif
}

}  // namespace rtc
