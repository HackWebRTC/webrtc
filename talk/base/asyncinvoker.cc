/*
 * libjingle
 * Copyright 2014 Google Inc.
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

#include "talk/base/asyncinvoker.h"

namespace talk_base {

// Synchronously execute all outstanding calls we own pending
// on |thread|. Optionally filter by message id.
void AsyncInvoker::Flush(Thread* thread, uint32 id /*= MQID_ANY*/) {
  // Run this on |thread| to reduce the number of context switches.
  if (Thread::Current() != thread) {
    thread->Invoke<void>(Bind(&AsyncInvoker::Flush, this, thread, id));
    return;
  }

  // Make a copy of handlers_, since it'll be modified by
  // callbacks to RemoveHandler when each is done executing.
  crit_.Enter();
  std::vector<MessageHandler*> handlers(handlers_.collection());
  crit_.Leave();
  MessageList removed;
  for (size_t i = 0; i < handlers.size(); ++i) {
    removed.clear();
    thread->Clear(handlers[i], id, &removed);
    if (!removed.empty()) {
      // Since each message gets its own handler with AsyncInvoker,
      // we expect a maximum of one removed.
      ASSERT(removed.size() == 1);
      // This handler was pending on this thread, so run it now.
      const Message& msg = removed.front();
      thread->Send(msg.phandler,
                   msg.message_id,
                   msg.pdata);
    }
  }
}

void AsyncInvoker::InvokeHandler(Thread* thread, MessageHandler* handler,
                                 uint32 id) {
  {
    CritScope cs(&crit_);
    handlers_.PushBack(handler);
  }
  thread->Post(handler, id);
}

void AsyncInvoker::RemoveHandler(MessageHandler* handler) {
  CritScope cs(&crit_);
  handlers_.Remove(handler);
  delete handler;
}

}  // namespace talk_base
