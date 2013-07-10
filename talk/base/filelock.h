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

#ifndef TALK_BASE_FILELOCK_H_
#define TALK_BASE_FILELOCK_H_

#include <string>

#include "talk/base/constructormagic.h"
#include "talk/base/scoped_ptr.h"

namespace talk_base {

class FileStream;

// Implements a very simple cross process lock based on a file.
// When Lock(...) is called we try to open/create the file in read/write
// mode without any sharing. (Or locking it with flock(...) on Unix)
// If the process crash the OS will make sure that the file descriptor
// is released and another process can accuire the lock.
// This doesn't work on ancient OSX/Linux versions if used on NFS.
// (Nfs-client before: ~2.6 and Linux Kernel < 2.6.)
class FileLock {
 public:
  virtual ~FileLock();

  // Attempts to lock the file. The caller owns the returned
  // lock object. Returns NULL if the file already was locked.
  static FileLock* TryLock(const std::string& path);
  void Unlock();

 protected:
  FileLock(const std::string& path, FileStream* file);

 private:
  void MaybeUnlock();

  std::string path_;
  scoped_ptr<FileStream> file_;

  DISALLOW_EVIL_CONSTRUCTORS(FileLock);
};

}  // namespace talk_base

#endif  // TALK_BASE_FILELOCK_H_
