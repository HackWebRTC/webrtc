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
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/source/set_thread_name_win.h"

namespace webrtc {

ThreadWindows::ThreadWindows(ThreadRunFunction func, ThreadObj obj,
                             ThreadPriority prio, const char* thread_name)
    : run_function_(func),
      obj_(obj),
      prio_(prio),
      event_(CreateEvent(NULL, FALSE, FALSE, NULL)),
      thread_(NULL),
      name_(thread_name ? thread_name : "webrtc") {
  DCHECK(func);
  DCHECK(event_);
}

ThreadWindows::~ThreadWindows() {
  DCHECK(main_thread_.CalledOnValidThread());
  DCHECK(!thread_);
  CloseHandle(event_);
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

bool ThreadWindows::Start(unsigned int& id) {
  DCHECK(main_thread_.CalledOnValidThread());
  DCHECK(!thread_);

  // See bug 2902 for stack size.
  DWORD thread_id;
  thread_ = ::CreateThread(NULL, 0, &StartThread, this,
      STACK_SIZE_PARAM_IS_A_RESERVATION, &thread_id);
  if (!thread_ ) {
    DCHECK(false) << "CreateThread failed";
    return false;
  }

  id = thread_id;

  if (prio_ != kNormalPriority) {
    int priority = THREAD_PRIORITY_NORMAL;
    switch (prio_) {
      case kLowPriority:
        priority = THREAD_PRIORITY_BELOW_NORMAL;
        break;
      case kHighPriority:
        priority = THREAD_PRIORITY_ABOVE_NORMAL;
        break;
      case kHighestPriority:
        priority = THREAD_PRIORITY_HIGHEST;
        break;
      case kRealtimePriority:
        priority = THREAD_PRIORITY_TIME_CRITICAL;
        break;
      default:
        break;
    }

    SetThreadPriority(thread_, priority);
  }

  return true;
}

bool ThreadWindows::Stop() {
  DCHECK(main_thread_.CalledOnValidThread());
  if (thread_) {
    SetEvent(event_);
    WaitForSingleObject(thread_, INFINITE);
    CloseHandle(thread_);
    thread_ = nullptr;
  }

  return true;
}

void ThreadWindows::Run() {
  if (!name_.empty())
    SetThreadName(static_cast<DWORD>(-1), name_.c_str());

  do {
    if (!run_function_(obj_))
      break;
  } while (WaitForSingleObject(event_, 0) == WAIT_TIMEOUT);
}

}  // namespace webrtc
