/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#ifndef TALK_BASE_SHAREDEXCLUSIVELOCK_H_
#define TALK_BASE_SHAREDEXCLUSIVELOCK_H_

#include "talk/base/constructormagic.h"
#include "talk/base/criticalsection.h"
#include "talk/base/event.h"

namespace talk_base {

// This class provides shared-exclusive lock. It can be used in cases like
// multiple-readers/single-writer model.
class SharedExclusiveLock {
 public:
  SharedExclusiveLock();

  // Locking/unlocking methods. It is encouraged to use SharedScope or
  // ExclusiveScope for protection.
  void LockExclusive();
  void UnlockExclusive();
  void LockShared();
  void UnlockShared();

 private:
  talk_base::CriticalSection cs_exclusive_;
  talk_base::CriticalSection cs_shared_;
  talk_base::Event shared_count_is_zero_;
  int shared_count_;

  DISALLOW_COPY_AND_ASSIGN(SharedExclusiveLock);
};

class SharedScope {
 public:
  explicit SharedScope(SharedExclusiveLock* lock) : lock_(lock) {
    lock_->LockShared();
  }

  ~SharedScope() {
    lock_->UnlockShared();
  }

 private:
  SharedExclusiveLock* lock_;

  DISALLOW_COPY_AND_ASSIGN(SharedScope);
};

class ExclusiveScope {
 public:
  explicit ExclusiveScope(SharedExclusiveLock* lock) : lock_(lock) {
    lock_->LockExclusive();
  }

  ~ExclusiveScope() {
    lock_->UnlockExclusive();
  }

 private:
  SharedExclusiveLock* lock_;

  DISALLOW_COPY_AND_ASSIGN(ExclusiveScope);
};

}  // namespace talk_base

#endif  // TALK_BASE_SHAREDEXCLUSIVELOCK_H_
