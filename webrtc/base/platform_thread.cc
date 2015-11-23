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

namespace webrtc {

rtc::scoped_ptr<PlatformThread> PlatformThread::CreateThread(
    ThreadRunFunction func,
    void* obj,
    const char* thread_name) {
  return rtc::scoped_ptr<PlatformThread>(
      new PlatformThread(func, obj, thread_name));
}

namespace {
#if defined(WEBRTC_WIN)
void CALLBACK RaiseFlag(ULONG_PTR param) {
  *reinterpret_cast<bool*>(param) = true;
}
#else
struct ThreadAttributes {
  ThreadAttributes() { pthread_attr_init(&attr); }
  ~ThreadAttributes() { pthread_attr_destroy(&attr); }
  pthread_attr_t* operator&() { return &attr; }
  pthread_attr_t attr;
};

int ConvertToSystemPriority(ThreadPriority priority,
                            int min_prio,
                            int max_prio) {
  RTC_DCHECK(max_prio - min_prio > 2);
  const int top_prio = max_prio - 1;
  const int low_prio = min_prio + 1;

  switch (priority) {
    case kLowPriority:
      return low_prio;
    case kNormalPriority:
      // The -1 ensures that the kHighPriority is always greater or equal to
      // kNormalPriority.
      return (low_prio + top_prio - 1) / 2;
    case kHighPriority:
      return std::max(top_prio - 2, low_prio);
    case kHighestPriority:
      return std::max(top_prio - 1, low_prio);
    case kRealtimePriority:
      return top_prio;
  }
  RTC_DCHECK(false);
  return low_prio;
}
#endif  // defined(WEBRTC_WIN)
}

PlatformThread::PlatformThread(ThreadRunFunction func,
                               void* obj,
                               const char* thread_name)
    : run_function_(func),
      obj_(obj),
      name_(thread_name ? thread_name : "webrtc"),
#if defined(WEBRTC_WIN)
      stop_(false),
      thread_(NULL) {
#else
      stop_event_(false, false),
      thread_(0) {
#endif  // defined(WEBRTC_WIN)
  RTC_DCHECK(func);
  RTC_DCHECK(name_.length() < 64);
}

PlatformThread::~PlatformThread() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
#if defined(WEBRTC_WIN)
  RTC_DCHECK(!thread_);
#endif  // defined(WEBRTC_WIN)
}

#if defined(WEBRTC_WIN)
DWORD WINAPI PlatformThread::StartThread(void* param) {
  static_cast<PlatformThread*>(param)->Run();
  return 0;
}
#else
void* PlatformThread::StartThread(void* param) {
  static_cast<PlatformThread*>(param)->Run();
  return 0;
}
#endif  // defined(WEBRTC_WIN)

bool PlatformThread::Start() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(!thread_) << "Thread already started?";
#if defined(WEBRTC_WIN)
  stop_ = false;

  // See bug 2902 for background on STACK_SIZE_PARAM_IS_A_RESERVATION.
  // Set the reserved stack stack size to 1M, which is the default on Windows
  // and Linux.
  DWORD thread_id;
  thread_ = ::CreateThread(NULL, 1024 * 1024, &StartThread, this,
                           STACK_SIZE_PARAM_IS_A_RESERVATION, &thread_id);
  RTC_CHECK(thread_) << "CreateThread failed";
#else
  ThreadAttributes attr;
  // Set the stack stack size to 1M.
  pthread_attr_setstacksize(&attr, 1024 * 1024);
  RTC_CHECK_EQ(0, pthread_create(&thread_, &attr, &StartThread, this));
#endif  // defined(WEBRTC_WIN)
  return true;
}

bool PlatformThread::Stop() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
#if defined(WEBRTC_WIN)
  if (thread_) {
    // Set stop_ to |true| on the worker thread.
    QueueUserAPC(&RaiseFlag, thread_, reinterpret_cast<ULONG_PTR>(&stop_));
    WaitForSingleObject(thread_, INFINITE);
    CloseHandle(thread_);
    thread_ = nullptr;
  }
#else
  if (!thread_)
    return true;

  stop_event_.Set();
  RTC_CHECK_EQ(0, pthread_join(thread_, nullptr));
  thread_ = 0;
#endif  // defined(WEBRTC_WIN)
  return true;
}

void PlatformThread::Run() {
  if (!name_.empty())
    rtc::SetCurrentThreadName(name_.c_str());
  do {
    // The interface contract of Start/Stop is that for a successfull call to
    // Start, there should be at least one call to the run function.  So we
    // call the function before checking |stop_|.
    if (!run_function_(obj_))
      break;
#if defined(WEBRTC_WIN)
    // Alertable sleep to permit RaiseFlag to run and update |stop_|.
    SleepEx(0, true);
  } while (!stop_);
#else
  } while (!stop_event_.Wait(0));
#endif  // defined(WEBRTC_WIN)
}

bool PlatformThread::SetPriority(ThreadPriority priority) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
#if defined(WEBRTC_WIN)
  return thread_ && SetThreadPriority(thread_, priority);
#else
  if (!thread_)
    return false;
#if defined(WEBRTC_CHROMIUM_BUILD) && defined(WEBRTC_LINUX)
  // TODO(tommi): Switch to the same mechanism as Chromium uses for
  // changing thread priorities.
  return true;
#else
#ifdef WEBRTC_THREAD_RR
  const int policy = SCHED_RR;
#else
  const int policy = SCHED_FIFO;
#endif
  const int min_prio = sched_get_priority_min(policy);
  const int max_prio = sched_get_priority_max(policy);
  if (min_prio == -1 || max_prio == -1) {
    return false;
  }

  if (max_prio - min_prio <= 2)
    return false;

  sched_param param;
  param.sched_priority = ConvertToSystemPriority(priority, min_prio, max_prio);
  if (pthread_setschedparam(thread_, policy, &param) != 0) {
    return false;
  }

  return true;
#endif  // defined(WEBRTC_CHROMIUM_BUILD) && defined(WEBRTC_LINUX)
#endif  // defined(WEBRTC_WIN)
}

}  // namespace webrtc
