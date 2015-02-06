/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_SOURCE_PROCESS_THREAD_IMPL_H_
#define WEBRTC_MODULES_UTILITY_SOURCE_PROCESS_THREAD_IMPL_H_

#include <list>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class ProcessThreadImpl : public ProcessThread {
 public:
  ProcessThreadImpl();
  ~ProcessThreadImpl() override;

  int32_t Start() override;
  int32_t Stop() override;

  void WakeUp(Module* module) override;

  int32_t RegisterModule(Module* module);
  int32_t DeRegisterModule(const Module* module);

 protected:
  static bool Run(void* obj);
  bool Process();

 private:
  rtc::ThreadChecker thread_checker_;
  const rtc::scoped_ptr<EventWrapper> wake_up_;
  rtc::scoped_ptr<ThreadWrapper> thread_;

  struct ModuleCallback {
    ModuleCallback(Module* module) : module(module), next_callback(0) {}
    bool operator==(const ModuleCallback& cb) const {
      return cb.module == module;
    }
    Module* const module;
    int64_t next_callback;  // Absolute timestamp.
  };

  rtc::CriticalSection lock_;  // Used to guard modules_ and stop_.
  typedef std::list<ModuleCallback> ModuleList;
  ModuleList modules_;
  bool stop_;
};

}  // namespace webrtc

#endif // WEBRTC_MODULES_UTILITY_SOURCE_PROCESS_THREAD_IMPL_H_
