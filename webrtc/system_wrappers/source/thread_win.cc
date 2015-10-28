/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/source/thread_win.h"

#include <process.h>
#include <stdio.h>
#include <windows.h>

#include "webrtc/base/checks.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc {
namespace {
void CALLBACK RaiseFlag(ULONG_PTR param) {
  *reinterpret_cast<bool*>(param) = true;
}
}

ThreadWindows::ThreadWindows(ThreadRunFunction func, void* obj,
                             const char* thread_name)
    : run_function_(func),
      obj_(obj),
      stop_(false),
      thread_(NULL),
      name_(thread_name ? thread_name : "webrtc") {
  RTC_DCHECK(func);
}

ThreadWindows::~ThreadWindows() {
  RTC_DCHECK(main_thread_.CalledOnValidThread());
  RTC_DCHECK(!thread_);
}

// static
uint32_t ThreadWrapper::GetThreadId() {
  return GetCurrentThreadId();
}

// static
DWORD WINAPI ThreadWindows::StartThread(void* param) {
  static_cast<ThreadWindows*>(param)->Run();
  return 0;
}

bool ThreadWindows::Start() {
  RTC_DCHECK(main_thread_.CalledOnValidThread());
  RTC_DCHECK(!thread_);

  stop_ = false;

  // See bug 2902 for background on STACK_SIZE_PARAM_IS_A_RESERVATION.
  // Set the reserved stack stack size to 1M, which is the default on Windows
  // and Linux.
  DWORD thread_id;
  thread_ = ::CreateThread(NULL, 1024 * 1024, &StartThread, this,
      STACK_SIZE_PARAM_IS_A_RESERVATION, &thread_id);
  if (!thread_ ) {
    RTC_DCHECK(false) << "CreateThread failed";
    return false;
  }

  return true;
}

bool ThreadWindows::Stop() {
  RTC_DCHECK(main_thread_.CalledOnValidThread());
  if (thread_) {
    // Set stop_ to |true| on the worker thread.
    QueueUserAPC(&RaiseFlag, thread_, reinterpret_cast<ULONG_PTR>(&stop_));
    WaitForSingleObject(thread_, INFINITE);
    CloseHandle(thread_);
    thread_ = nullptr;
  }

  return true;
}

bool ThreadWindows::SetPriority(ThreadPriority priority) {
  RTC_DCHECK(main_thread_.CalledOnValidThread());
  return thread_ && SetThreadPriority(thread_, priority);
}

void ThreadWindows::Run() {
  if (!name_.empty())
    rtc::SetCurrentThreadName(name_.c_str());

  do {
    // The interface contract of Start/Stop is that for a successfull call to
    // Start, there should be at least one call to the run function.  So we
    // call the function before checking |stop_|.
    if (!run_function_(obj_))
      break;
    // Alertable sleep to permit RaiseFlag to run and update |stop_|.
    SleepEx(0, true);
  } while (!stop_);
}

}  // namespace webrtc
