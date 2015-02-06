/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"

#include "webrtc/base/sharedexclusivelock.h"
#include "webrtc/base/thread_annotations.h"

namespace webrtc {

class LOCKABLE RwLock : public RWLockWrapper {
  virtual void AcquireLockExclusive() override EXCLUSIVE_LOCK_FUNCTION() {
    lock_.LockExclusive();
  }
  virtual void ReleaseLockExclusive() override UNLOCK_FUNCTION() {
    lock_.UnlockExclusive();
  }

  virtual void AcquireLockShared() override SHARED_LOCK_FUNCTION() {
    lock_.LockShared();
  }
  virtual void ReleaseLockShared() override UNLOCK_FUNCTION() {
    lock_.UnlockShared();
  }

 private:
  rtc::SharedExclusiveLock lock_;
};

RWLockWrapper* RWLockWrapper::CreateRWLock() {
  return new RwLock();
}

}  // namespace webrtc
