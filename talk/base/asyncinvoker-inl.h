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

#include "talk/base/bind.h"
#include "talk/base/callback.h"
#include "talk/base/criticalsection.h"
#include "talk/base/messagehandler.h"
#include "talk/base/refcount.h"
#include "talk/base/scoped_ref_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/base/thread.h"

namespace talk_base {

class AsyncInvoker;

// Helper class for AsyncInvoker. Runs a task and triggers a callback
// on the calling thread if necessary. Instances are ref-counted so their
// lifetime can be independent of AsyncInvoker.
class AsyncClosure : public RefCountInterface {
 public:
  virtual ~AsyncClosure() {}
  // Runs the asynchronous task, and triggers a callback to the calling
  // thread if needed. Should be called from the target thread.
  virtual void Execute() = 0;
};

// Simple closure that doesn't trigger a callback for the calling thread.
template <class FunctorT>
class FireAndForgetAsyncClosure : public AsyncClosure {
 public:
  explicit FireAndForgetAsyncClosure(const FunctorT& functor)
      : functor_(functor) {}
  virtual void Execute() {
    functor_();
  }
 private:
  FunctorT functor_;
};

// Base class for closures that may trigger a callback for the calling thread.
// Listens for the "destroyed" signals from the calling thread and the invoker,
// and cancels the callback to the calling thread if either is destroyed.
class NotifyingAsyncClosureBase : public AsyncClosure,
                                  public sigslot::has_slots<> {
 public:
  virtual ~NotifyingAsyncClosureBase() { disconnect_all(); }

 protected:
  NotifyingAsyncClosureBase(AsyncInvoker* invoker, Thread* calling_thread);
  void TriggerCallback();
  void SetCallback(const Callback0<void>& callback) {
    CritScope cs(&crit_);
    callback_ = callback;
  }
  bool CallbackCanceled() const { return calling_thread_ == NULL; }

 private:
  Callback0<void> callback_;
  CriticalSection crit_;
  AsyncInvoker* invoker_;
  Thread* calling_thread_;

  void CancelCallback();
};

// Closures that have a non-void return value and require a callback.
template <class ReturnT, class FunctorT, class HostT>
class NotifyingAsyncClosure : public NotifyingAsyncClosureBase {
 public:
  NotifyingAsyncClosure(AsyncInvoker* invoker,
                        Thread* calling_thread,
                        const FunctorT& functor,
                        void (HostT::*callback)(ReturnT),
                        HostT* callback_host)
      :  NotifyingAsyncClosureBase(invoker, calling_thread),
         functor_(functor),
         callback_(callback),
         callback_host_(callback_host) {}
  virtual void Execute() {
    ReturnT result = functor_();
    if (!CallbackCanceled()) {
      SetCallback(Callback0<void>(Bind(callback_, callback_host_, result)));
      TriggerCallback();
    }
  }

 private:
  FunctorT functor_;
  void (HostT::*callback_)(ReturnT);
  HostT* callback_host_;
};

// Closures that have a void return value and require a callback.
template <class FunctorT, class HostT>
class NotifyingAsyncClosure<void, FunctorT, HostT>
    : public NotifyingAsyncClosureBase {
 public:
  NotifyingAsyncClosure(AsyncInvoker* invoker,
                        Thread* calling_thread,
                        const FunctorT& functor,
                        void (HostT::*callback)(),
                        HostT* callback_host)
      : NotifyingAsyncClosureBase(invoker, calling_thread),
        functor_(functor) {
    SetCallback(Callback0<void>(Bind(callback, callback_host)));
  }
  virtual void Execute() {
    functor_();
    TriggerCallback();
  }

 private:
  FunctorT functor_;
};

}  // namespace talk_base

#endif  // TALK_BASE_ASYNCINVOKER_INL_H_
