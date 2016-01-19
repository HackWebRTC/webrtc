/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INCLUDE_CRITICAL_SECTION_WRAPPER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INCLUDE_CRITICAL_SECTION_WRAPPER_H_

// If the critical section is heavily contended it may be beneficial to use
// read/write locks instead.

#if defined (WEBRTC_WIN)
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"

namespace webrtc {

class LOCKABLE CriticalSectionWrapper {
 public:
  // Legacy factory method, being deprecated. Please use the constructor.
  // TODO(tommi): Remove the CriticalSectionWrapper class and move users over
  // to using rtc::CriticalSection.  Before we can do that though, we need to
  // fix the problem with the ConditionVariable* classes (see below).
  static CriticalSectionWrapper* CreateCriticalSection();

  CriticalSectionWrapper();
  ~CriticalSectionWrapper();

  // Tries to grab lock, beginning of a critical section. Will wait for the
  // lock to become available if the grab failed.
  void Enter() EXCLUSIVE_LOCK_FUNCTION();

  // Returns a grabbed lock, end of critical section.
  void Leave() UNLOCK_FUNCTION();

private:
#if defined (WEBRTC_WIN)
  CRITICAL_SECTION crit_;

  // TODO(tommi): Remove friendness.
  friend class ConditionVariableEventWin;
  friend class ConditionVariableNativeWin;
#else
  pthread_mutex_t mutex_;
#endif
};

// RAII extension of the critical section. Prevents Enter/Leave mismatches and
// provides more compact critical section syntax.
class SCOPED_LOCKABLE CriticalSectionScoped {
 public:
  explicit CriticalSectionScoped(CriticalSectionWrapper* critsec)
      EXCLUSIVE_LOCK_FUNCTION(critsec)
      : ptr_crit_sec_(critsec) {
    ptr_crit_sec_->Enter();
  }

  ~CriticalSectionScoped() UNLOCK_FUNCTION() { ptr_crit_sec_->Leave(); }

 private:
  CriticalSectionWrapper* ptr_crit_sec_;
};

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_INCLUDE_CRITICAL_SECTION_WRAPPER_H_
