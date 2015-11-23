/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_PLATFORM_THREAD_H_
#define WEBRTC_BASE_PLATFORM_THREAD_H_

#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/event.h"
#include "webrtc/base/platform_thread_types.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_checker.h"

namespace rtc {

PlatformThreadId CurrentThreadId();
PlatformThreadRef CurrentThreadRef();

// Compares two thread identifiers for equality.
bool IsThreadRefEqual(const PlatformThreadRef& a, const PlatformThreadRef& b);

// Sets the current thread name.
void SetCurrentThreadName(const char* name);

}  // namespace rtc

// TODO(pbos): Merge with namespace rtc.
namespace webrtc {

// Callback function that the spawned thread will enter once spawned.
// A return value of false is interpreted as that the function has no
// more work to do and that the thread can be released.
typedef bool (*ThreadRunFunction)(void*);

enum ThreadPriority {
#ifdef WEBRTC_WIN
  kLowPriority = THREAD_PRIORITY_BELOW_NORMAL,
  kNormalPriority = THREAD_PRIORITY_NORMAL,
  kHighPriority = THREAD_PRIORITY_ABOVE_NORMAL,
  kHighestPriority = THREAD_PRIORITY_HIGHEST,
  kRealtimePriority = THREAD_PRIORITY_TIME_CRITICAL
#else
  kLowPriority = 1,
  kNormalPriority = 2,
  kHighPriority = 3,
  kHighestPriority = 4,
  kRealtimePriority = 5
#endif
};

// Represents a simple worker thread.  The implementation must be assumed
// to be single threaded, meaning that all methods of the class, must be
// called from the same thread, including instantiation.
// TODO(tommi): There's no need for this to be a virtual interface since there's
// only ever a single implementation of it.
class PlatformThread {
 public:
  PlatformThread(ThreadRunFunction func, void* obj, const char* thread_name);
  virtual ~PlatformThread();

  // Factory method. Constructor disabled.
  //
  // func        Pointer to a, by user, specified callback function.
  // obj         Object associated with the thread. Passed in the callback
  //             function.
  // prio        Thread priority. May require root/admin rights.
  // thread_name  NULL terminated thread name, will be visable in the Windows
  //             debugger.
  // TODO(pbos): Move users onto explicit initialization/member ownership
  // instead of additional heap allocation due to CreateThread.
  static rtc::scoped_ptr<PlatformThread> CreateThread(ThreadRunFunction func,
                                                      void* obj,
                                                      const char* thread_name);

  // Tries to spawns a thread and returns true if that was successful.
  // Additionally, it tries to set thread priority according to the priority
  // from when CreateThread was called. However, failure to set priority will
  // not result in a false return value.
  // TODO(pbos): Make void not war.
  bool Start();

  // Stops the spawned thread and waits for it to be reclaimed with a timeout
  // of two seconds. Will return false if the thread was not reclaimed.
  // Multiple tries to Stop are allowed (e.g. to wait longer than 2 seconds).
  // It's ok to call Stop() even if the spawned thread has been reclaimed.
  // TODO(pbos): Make void not war.
  bool Stop();

  // Set the priority of the worker thread.  Must be called when thread
  // is running.
  bool SetPriority(ThreadPriority priority);

 private:
  void Run();

  ThreadRunFunction const run_function_;
  void* const obj_;
  // TODO(pbos): Make sure call sites use string literals and update to a const
  // char* instead of a std::string.
  const std::string name_;
  rtc::ThreadChecker thread_checker_;
#if defined(WEBRTC_WIN)
  static DWORD WINAPI StartThread(void* param);

  bool stop_;
  HANDLE thread_;
#else
  static void* StartThread(void* param);

  rtc::Event stop_event_;

  pthread_t thread_;
#endif  // defined(WEBRTC_WIN)
  RTC_DISALLOW_COPY_AND_ASSIGN(PlatformThread);
};

}  // namespace webrtc

#endif  // WEBRTC_BASE_PLATFORM_THREAD_H_
