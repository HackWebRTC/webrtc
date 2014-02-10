/*
 * libjingle
 * Copyright 2004, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_BASE_CRITICALSECTION_H__
#define TALK_BASE_CRITICALSECTION_H__

#include "talk/base/constructormagic.h"
#include "webrtc/system_wrappers/interface/thread_annotations.h"

#ifdef WIN32
#include "talk/base/win32.h"
#endif

#ifdef POSIX
#include <pthread.h>
#endif

#ifdef _DEBUG
#define CS_TRACK_OWNER 1
#endif  // _DEBUG

#if CS_TRACK_OWNER
#define TRACK_OWNER(x) x
#else  // !CS_TRACK_OWNER
#define TRACK_OWNER(x)
#endif  // !CS_TRACK_OWNER

namespace talk_base {

#ifdef WIN32
class LOCKABLE CriticalSection {
 public:
  CriticalSection() {
    InitializeCriticalSection(&crit_);
    // Windows docs say 0 is not a valid thread id
    TRACK_OWNER(thread_ = 0);
  }
  ~CriticalSection() {
    DeleteCriticalSection(&crit_);
  }
  void Enter() EXCLUSIVE_LOCK_FUNCTION() {
    EnterCriticalSection(&crit_);
    TRACK_OWNER(thread_ = GetCurrentThreadId());
  }
  bool TryEnter() EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    if (TryEnterCriticalSection(&crit_) != FALSE) {
      TRACK_OWNER(thread_ = GetCurrentThreadId());
      return true;
    }
    return false;
  }
  void Leave() UNLOCK_FUNCTION() {
    TRACK_OWNER(thread_ = 0);
    LeaveCriticalSection(&crit_);
  }

#if CS_TRACK_OWNER
  bool CurrentThreadIsOwner() const { return thread_ == GetCurrentThreadId(); }
#endif  // CS_TRACK_OWNER

 private:
  CRITICAL_SECTION crit_;
  TRACK_OWNER(DWORD thread_);  // The section's owning thread id
};
#endif // WIN32

#ifdef POSIX
class LOCKABLE CriticalSection {
 public:
  CriticalSection() {
    pthread_mutexattr_t mutex_attribute;
    pthread_mutexattr_init(&mutex_attribute);
    pthread_mutexattr_settype(&mutex_attribute, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex_, &mutex_attribute);
    pthread_mutexattr_destroy(&mutex_attribute);
    TRACK_OWNER(thread_ = 0);
  }
  ~CriticalSection() {
    pthread_mutex_destroy(&mutex_);
  }
  void Enter() EXCLUSIVE_LOCK_FUNCTION()  {
    pthread_mutex_lock(&mutex_);
    TRACK_OWNER(thread_ = pthread_self());
  }
  bool TryEnter() EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    if (pthread_mutex_trylock(&mutex_) == 0) {
      TRACK_OWNER(thread_ = pthread_self());
      return true;
    }
    return false;
  }
  void Leave() UNLOCK_FUNCTION() {
    TRACK_OWNER(thread_ = 0);
    pthread_mutex_unlock(&mutex_);
  }

#if CS_TRACK_OWNER
  bool CurrentThreadIsOwner() const { return pthread_equal(thread_, pthread_self()); }
#endif  // CS_TRACK_OWNER

 private:
  pthread_mutex_t mutex_;
  TRACK_OWNER(pthread_t thread_);
};
#endif // POSIX

// CritScope, for serializing execution through a scope.
class SCOPED_LOCKABLE CritScope {
 public:
  explicit CritScope(CriticalSection *pcrit) EXCLUSIVE_LOCK_FUNCTION(pcrit) {
    pcrit_ = pcrit;
    pcrit_->Enter();
  }
  ~CritScope() UNLOCK_FUNCTION() {
    pcrit_->Leave();
  }
 private:
  CriticalSection *pcrit_;
  DISALLOW_COPY_AND_ASSIGN(CritScope);
};

// Tries to lock a critical section on construction via
// CriticalSection::TryEnter, and unlocks on destruction if the
// lock was taken. Never blocks.
//
// IMPORTANT: Unlike CritScope, the lock may not be owned by this thread in
// subsequent code. Users *must* check locked() to determine if the
// lock was taken. If you're not calling locked(), you're doing it wrong!
class TryCritScope {
 public:
  explicit TryCritScope(CriticalSection *pcrit) {
    pcrit_ = pcrit;
    locked_ = pcrit_->TryEnter();
  }
  ~TryCritScope() {
    if (locked_) {
      pcrit_->Leave();
    }
  }
  bool locked() const {
    return locked_;
  }
 private:
  CriticalSection *pcrit_;
  bool locked_;
  DISALLOW_COPY_AND_ASSIGN(TryCritScope);
};

// TODO: Move this to atomicops.h, which can't be done easily because of
// complex compile rules.
class AtomicOps {
 public:
#ifdef WIN32
  // Assumes sizeof(int) == sizeof(LONG), which it is on Win32 and Win64.
  static int Increment(int* i) {
    return ::InterlockedIncrement(reinterpret_cast<LONG*>(i));
  }
  static int Decrement(int* i) {
    return ::InterlockedDecrement(reinterpret_cast<LONG*>(i));
  }
#else
  static int Increment(int* i) {
    return __sync_add_and_fetch(i, 1);
  }
  static int Decrement(int* i) {
    return __sync_sub_and_fetch(i, 1);
  }
#endif
};

} // namespace talk_base

#endif // TALK_BASE_CRITICALSECTION_H__
