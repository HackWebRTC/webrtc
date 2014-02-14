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

AsyncInvoker::AsyncInvoker() : destroying_(false) {}

AsyncInvoker::~AsyncInvoker() {
  destroying_ = true;
  SignalInvokerDestroyed();
  // Messages for this need to be cleared *before* our destructor is complete.
  MessageQueueManager::Clear(this);
}

void AsyncInvoker::OnMessage(Message* msg) {
  // Get the AsyncClosure shared ptr from this message's data.
  ScopedRefMessageData<AsyncClosure>* data =
      static_cast<ScopedRefMessageData<AsyncClosure>*>(msg->pdata);
  scoped_refptr<AsyncClosure> closure = data->data();
  delete msg->pdata;
  msg->pdata = NULL;

  // Execute the closure and trigger the return message if needed.
  closure->Execute();
}

void AsyncInvoker::Flush(Thread* thread, uint32 id /*= MQID_ANY*/) {
  if (destroying_) return;

  // Run this on |thread| to reduce the number of context switches.
  if (Thread::Current() != thread) {
    thread->Invoke<void>(Bind(&AsyncInvoker::Flush, this, thread, id));
    return;
  }

  MessageList removed;
  thread->Clear(this, id, &removed);
  for (MessageList::iterator it = removed.begin(); it != removed.end(); ++it) {
    // This message was pending on this thread, so run it now.
    thread->Send(it->phandler,
                 it->message_id,
                 it->pdata);
  }
}

void AsyncInvoker::DoInvoke(Thread* thread, AsyncClosure* closure,
                            uint32 id) {
  if (destroying_) {
    LOG(LS_WARNING) << "Tried to invoke while destroying the invoker.";
    // Since this call transwers ownership of |closure|, we clean it up here.
    delete closure;
    return;
  }
  thread->Post(this, id, new ScopedRefMessageData<AsyncClosure>(closure));
}

NotifyingAsyncClosureBase::NotifyingAsyncClosureBase(AsyncInvoker* invoker,
                                                     Thread* calling_thread)
    : invoker_(invoker), calling_thread_(calling_thread) {
  calling_thread->SignalQueueDestroyed.connect(
      this, &NotifyingAsyncClosureBase::CancelCallback);
  invoker->SignalInvokerDestroyed.connect(
      this, &NotifyingAsyncClosureBase::CancelCallback);
}

void NotifyingAsyncClosureBase::TriggerCallback() {
  CritScope cs(&crit_);
  if (!CallbackCanceled() && !callback_.empty()) {
    invoker_->AsyncInvoke<void>(calling_thread_, callback_);
  }
}

void NotifyingAsyncClosureBase::CancelCallback() {
  // If the callback is triggering when this is called, block the
  // destructor of the dying object here by waiting until the callback
  // is done triggering.
  CritScope cs(&crit_);
  // calling_thread_ == NULL means do not trigger the callback.
  calling_thread_ = NULL;
}

}  // namespace talk_base
