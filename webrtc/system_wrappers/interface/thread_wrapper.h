/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// System independant wrapper for spawning threads
// Note: the spawned thread will loop over the callback function until stopped.
// Note: The callback function is expected to return every 2 seconds or more
// often.

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_THREAD_WRAPPER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_THREAD_WRAPPER_H_

#include "webrtc/common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Object that will be passed by the spawned thread when it enters the callback
// function.
// TODO(tommi): Remove this define.
#define ThreadObj void*

// Callback function that the spawned thread will enter once spawned.
// A return value of false is interpreted as that the function has no
// more work to do and that the thread can be released.
typedef bool(*ThreadRunFunction)(void*);

enum ThreadPriority {
  kLowPriority = 1,
  kNormalPriority = 2,
  kHighPriority = 3,
  kHighestPriority = 4,
  kRealtimePriority = 5
};

// Represents a simple worker thread.  The implementation must be assumed
// to be single threaded, meaning that all methods of the class, must be
// called from the same thread, including instantiation.
class ThreadWrapper {
 public:
  enum {kThreadMaxNameLength = 64};

  virtual ~ThreadWrapper() {}

  // Factory method. Constructor disabled.
  //
  // func        Pointer to a, by user, specified callback function.
  // obj         Object associated with the thread. Passed in the callback
  //             function.
  // prio        Thread priority. May require root/admin rights.
  // thread_name  NULL terminated thread name, will be visable in the Windows
  //             debugger.
  // TODO(tommi): Remove the priority argument and provide a setter instead.
  // TODO(tommi): Make thread_name non-optional (i.e. no default value).
  static ThreadWrapper* CreateThread(ThreadRunFunction func,
                                     void* obj,
                                     ThreadPriority prio = kNormalPriority,
                                     const char* thread_name = 0);

  // Get the current thread's thread ID.
  // NOTE: This is a static method. It returns the id of the calling thread,
  // *not* the id of the worker thread that a ThreadWrapper instance represents.
  // TODO(tommi): Move outside of the ThreadWrapper class to avoid confusion.
  static uint32_t GetThreadId();

  // Tries to spawns a thread and returns true if that was successful.
  // Additionally, it tries to set thread priority according to the priority
  // from when CreateThread was called. However, failure to set priority will
  // not result in a false return value.
  // TODO(tommi): Remove the id parameter.
  virtual bool Start(unsigned int& id) = 0;

  // Stops the spawned thread and waits for it to be reclaimed with a timeout
  // of two seconds. Will return false if the thread was not reclaimed.
  // Multiple tries to Stop are allowed (e.g. to wait longer than 2 seconds).
  // It's ok to call Stop() even if the spawned thread has been reclaimed.
  virtual bool Stop() = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_THREAD_WRAPPER_H_
