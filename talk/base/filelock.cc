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

#include "talk/base/filelock.h"

#include "talk/base/fileutils.h"
#include "talk/base/logging.h"
#include "talk/base/pathutils.h"
#include "talk/base/stream.h"

namespace talk_base {

FileLock::FileLock(const std::string& path, FileStream* file)
    : path_(path), file_(file) {
}

FileLock::~FileLock() {
  MaybeUnlock();
}

void FileLock::Unlock() {
  LOG_F(LS_INFO);
  MaybeUnlock();
}

void FileLock::MaybeUnlock() {
  if (file_) {
    LOG(LS_INFO) << "Unlocking:" << path_;
    file_->Close();
    Filesystem::DeleteFile(path_);
    file_.reset();
  }
}

FileLock* FileLock::TryLock(const std::string& path) {
  FileStream* stream = new FileStream();
  bool ok = false;
#ifdef WIN32
  // Open and lock in a single operation.
  ok = stream->OpenShare(path, "a", _SH_DENYRW, NULL);
#else // Linux and OSX
  ok = stream->Open(path, "a", NULL) && stream->TryLock();
#endif
  if (ok) {
    return new FileLock(path, stream);
  } else {
    // Something failed, either we didn't succeed to open the
    // file or we failed to lock it. Anyway remove the heap
    // allocated object and then return NULL to indicate failure.
    delete stream;
    return NULL;
  }
}

}  // namespace talk_base
