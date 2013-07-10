/*
 * libjingle
 * Copyright 2009, Google Inc.
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

#include <string>

#include "talk/base/event.h"
#include "talk/base/filelock.h"
#include "talk/base/fileutils.h"
#include "talk/base/gunit.h"
#include "talk/base/pathutils.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"

namespace talk_base {

const static std::string kLockFile = "TestLockFile";
const static int kTimeoutMS = 5000;

class FileLockTest : public testing::Test, public Runnable {
 public:
  FileLockTest() : done_(false, false), thread_lock_failed_(false) {
  }

  virtual void Run(Thread* t) {
    scoped_ptr<FileLock> lock(FileLock::TryLock(temp_file_.pathname()));
    // The lock is already owned by the main thread of
    // this test, therefore the TryLock(...) call should fail.
    thread_lock_failed_ = lock.get() == NULL;
    done_.Set();
  }

 protected:
  virtual void SetUp() {
    thread_lock_failed_ = false;
    Filesystem::GetAppTempFolder(&temp_dir_);
    temp_file_ = Pathname(temp_dir_.pathname(), kLockFile);
  }

  void LockOnThread() {
    locker_.Start(this);
    done_.Wait(kTimeoutMS);
  }

  Event done_;
  Thread locker_;
  bool thread_lock_failed_;
  Pathname temp_dir_;
  Pathname temp_file_;
};

TEST_F(FileLockTest, TestLockFileDeleted) {
  scoped_ptr<FileLock> lock(FileLock::TryLock(temp_file_.pathname()));
  EXPECT_TRUE(lock.get() != NULL);
  EXPECT_FALSE(Filesystem::IsAbsent(temp_file_.pathname()));
  lock->Unlock();
  EXPECT_TRUE(Filesystem::IsAbsent(temp_file_.pathname()));
}

TEST_F(FileLockTest, TestLock) {
  scoped_ptr<FileLock> lock(FileLock::TryLock(temp_file_.pathname()));
  EXPECT_TRUE(lock.get() != NULL);
}

TEST_F(FileLockTest, TestLockX2) {
  scoped_ptr<FileLock> lock1(FileLock::TryLock(temp_file_.pathname()));
  EXPECT_TRUE(lock1.get() != NULL);

  scoped_ptr<FileLock> lock2(FileLock::TryLock(temp_file_.pathname()));
  EXPECT_TRUE(lock2.get() == NULL);
}

TEST_F(FileLockTest, TestThreadedLock) {
  scoped_ptr<FileLock> lock(FileLock::TryLock(temp_file_.pathname()));
  EXPECT_TRUE(lock.get() != NULL);

  LockOnThread();
  EXPECT_TRUE(thread_lock_failed_);
}

}  // namespace talk_base
