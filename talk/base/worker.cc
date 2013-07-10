/*
 * libjingle
 * Copyright 2004--2010, Google Inc.
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

#include "talk/base/worker.h"

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"

namespace talk_base {

enum {
  MSG_HAVEWORK = 0,
};

Worker::Worker() : worker_thread_(NULL) {}

Worker::~Worker() {
  // We need to already be stopped before being destroyed. We cannot call
  // StopWork() from here because the subclass's data has already been
  // destructed, so OnStop() cannot be called.
  ASSERT(!worker_thread_);
}

bool Worker::StartWork() {
  talk_base::Thread *me = talk_base::Thread::Current();
  if (worker_thread_) {
    if (worker_thread_ == me) {
      // Already working on this thread, so nothing to do.
      return true;
    } else {
      LOG(LS_ERROR) << "Automatically switching threads is not supported";
      ASSERT(false);
      return false;
    }
  }
  worker_thread_ = me;
  OnStart();
  return true;
}

bool Worker::StopWork() {
  if (!worker_thread_) {
    // Already not working, so nothing to do.
    return true;
  } else if (worker_thread_ != talk_base::Thread::Current()) {
    LOG(LS_ERROR) << "Stopping from a different thread is not supported";
    ASSERT(false);
    return false;
  }
  OnStop();
  worker_thread_->Clear(this, MSG_HAVEWORK);
  worker_thread_ = NULL;
  return true;
}

void Worker::HaveWork() {
  ASSERT(worker_thread_ != NULL);
  worker_thread_->Post(this, MSG_HAVEWORK);
}

void Worker::OnMessage(talk_base::Message *msg) {
  ASSERT(msg->message_id == MSG_HAVEWORK);
  ASSERT(worker_thread_ == talk_base::Thread::Current());
  OnHaveWork();
}

}  // namespace talk_base
