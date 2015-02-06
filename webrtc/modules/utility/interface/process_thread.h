/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_INTERFACE_PROCESS_THREAD_H_
#define WEBRTC_MODULES_UTILITY_INTERFACE_PROCESS_THREAD_H_

#include "webrtc/typedefs.h"
#include "webrtc/base/scoped_ptr.h"

namespace webrtc {
class Module;

class ProcessThread {
 public:
  virtual ~ProcessThread();

  static rtc::scoped_ptr<ProcessThread> Create();

  // Starts the worker thread.  Must be called from the construction thread.
  virtual int32_t Start() = 0;

  // Stops the worker thread.  Must be called from the construction thread.
  virtual int32_t Stop() = 0;

  // Wakes the thread up to give a module a chance to do processing right
  // away.  This causes the worker thread to wake up and requery the specified
  // module for when it should be called back. (Typically the module should
  // return 0 from TimeUntilNextProcess on the worker thread at that point).
  // Can be called on any thread.
  virtual void WakeUp(Module* module) = 0;

  // Adds a module that will start to receive callbacks on the worker thread.
  // Can be called from any thread.
  virtual int32_t RegisterModule(Module* module) = 0;

  // Removes a previously registered module.
  // Can be called from any thread.
  virtual int32_t DeRegisterModule(const Module* module) = 0;
};

}  // namespace webrtc

#endif // WEBRTC_MODULES_UTILITY_INTERFACE_PROCESS_THREAD_H_
