/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_POSIX_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_POSIX_H_

#include "webrtc/base/event.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"

#include <pthread.h>

namespace webrtc {

int ConvertToSystemPriority(ThreadPriority priority, int min_prio,
                            int max_prio);

class ThreadPosix : public ThreadWrapper {
 public:
  ThreadPosix(ThreadRunFunction func, ThreadObj obj, ThreadPriority prio,
              const char* thread_name);
  ~ThreadPosix() override;

  // From ThreadWrapper.
  bool Start(unsigned int& id) override;
  bool Stop() override;

 private:
  static void* StartThread(void* param);

  struct InitParams;
  void Run(InitParams* params);

  rtc::ThreadChecker thread_checker_;
  ThreadRunFunction const run_function_;
  void* const obj_;
  ThreadPriority prio_;
  rtc::Event stop_event_;
  const std::string name_;

  pid_t thread_id_;
  pthread_t thread_;
};

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_SOURCE_THREAD_POSIX_H_
