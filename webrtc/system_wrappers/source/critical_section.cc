/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {

CriticalSectionWrapper* CriticalSectionWrapper::CreateCriticalSection() {
  return new CriticalSectionWrapper();
}

#if defined (WEBRTC_WIN)

CriticalSectionWrapper::CriticalSectionWrapper() {
  InitializeCriticalSection(&crit_);
}

CriticalSectionWrapper::~CriticalSectionWrapper() {
  DeleteCriticalSection(&crit_);
}

void CriticalSectionWrapper::Enter() {
  EnterCriticalSection(&crit_);
}

void CriticalSectionWrapper::Leave() {
  LeaveCriticalSection(&crit_);
}

#else

CriticalSectionWrapper::CriticalSectionWrapper() {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&mutex_, &attr);
}

CriticalSectionWrapper::~CriticalSectionWrapper() {
  pthread_mutex_destroy(&mutex_);
}

void CriticalSectionWrapper::Enter() {
  pthread_mutex_lock(&mutex_);
}

void CriticalSectionWrapper::Leave() {
  pthread_mutex_unlock(&mutex_);
}

#endif

}  // namespace webrtc
