/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_INTERFACE_MODULE_H_
#define MODULES_INTERFACE_MODULE_H_

#include "webrtc/typedefs.h"

namespace webrtc {

class Module {
 public:
  // Returns the number of milliseconds until the module wants a worker
  // thread to call Process.
  // This method is called on the same worker thread as Process will
  // be called on.
  virtual int64_t TimeUntilNextProcess() = 0;

  // Process any pending tasks such as timeouts.
  // Called on a worker thread.
  virtual int32_t Process() = 0;

 protected:
  virtual ~Module() {}
};

// Reference counted version of the Module interface.
class RefCountedModule : public Module {
 public:
  // Increase the reference count by one.
  // Returns the incremented reference count.
  virtual int32_t AddRef() = 0;

  // Decrease the reference count by one.
  // Returns the decreased reference count.
  // Returns 0 if the last reference was just released.
  // When the reference count reaches 0 the object will self-destruct.
  virtual int32_t Release() = 0;

 protected:
  virtual ~RefCountedModule() {}
};

}  // namespace webrtc

#endif  // MODULES_INTERFACE_MODULE_H_
