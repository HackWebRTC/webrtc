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

#ifndef TALK_BASE_ASYNCINVOKER_INL_H_
#define TALK_BASE_ASYNCINVOKER_INL_H_

#include "talk/base/criticalsection.h"
#include "talk/base/messagehandler.h"
#include "talk/base/sigslot.h"
#include "talk/base/thread.h"

namespace talk_base {

// Helper class for AsyncInvoker. Runs a functor on a message queue or thread
// and doesn't execute the callback when finished if the calling thread ends.
template <class ReturnT, class FunctorT>
class AsyncFunctorMessageHandler
    : public FunctorMessageHandler<ReturnT, FunctorT>,
      public sigslot::has_slots<> {
  typedef AsyncFunctorMessageHandler<ReturnT, FunctorT> ThisT;
 public:
  explicit AsyncFunctorMessageHandler(const FunctorT& functor)
      : FunctorMessageHandler<ReturnT, FunctorT>(functor),
        thread_(Thread::Current()),
        shutting_down_(false) {
    thread_->SignalQueueDestroyed.connect(this, &ThisT::OnThreadDestroyed);
  }

  virtual ~AsyncFunctorMessageHandler() {
    CritScope cs(&running_crit_);
    shutting_down_ = true;
  }

  virtual void OnMessage(Message* msg) {
    CritScope cs(&running_crit_);
    if (!shutting_down_) {
      FunctorMessageHandler<ReturnT, FunctorT>::OnMessage(msg);
    }
  }

  // Returns the thread that initiated the async call.
  Thread* thread() const { return thread_; }

  // Wraps a callback so that it won't execute if |thread_| goes away.
  void WrapCallback(Callback0<void> cb) {
    this->SetCallback(
        Callback0<void>(Bind(&ThisT::MaybeRunCallback, this, cb)));
  }

 private:
  void OnThreadDestroyed() {
    CritScope cs(&thread_crit_);
    thread_ = NULL;
    this->SetCallback(Callback0<void>());  // Clear out the callback.
  }

  void MaybeRunCallback(Callback0<void> cb) {
#ifdef _DEBUG
    ASSERT(running_crit_.CurrentThreadIsOwner());
#endif
    CritScope cs(&thread_crit_);
    if (thread_ && !shutting_down_) {
      cb();
    }
  }

  FunctorT functor_;
  Thread* thread_;
  CriticalSection thread_crit_;
  CriticalSection running_crit_;
  bool shutting_down_;
};

}  // namespace talk_base


#endif  // TALK_BASE_ASYNCINVOKER_INL_H_
