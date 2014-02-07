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

#ifndef TALK_BASE_ASYNCINVOKER_H_
#define TALK_BASE_ASYNCINVOKER_H_

#include "talk/base/asyncinvoker-inl.h"
#include "talk/base/bind.h"
#include "talk/base/scopedptrcollection.h"
#include "talk/base/thread.h"

namespace talk_base {

// Invokes function objects (aka functors) asynchronously on a Thread, and
// owns the lifetime of calls (ie, when this object is destroyed, calls in
// flight are cancelled). AsyncInvoker can optionally execute a user-specified
// function when the asynchronous call is complete, or operates in
// fire-and-forget mode otherwise.
//
// AsyncInvoker does not own the thread it calls functors on.
//
// A note about async calls and object lifetimes: users should
// be mindful of object lifetimes when calling functions asynchronously and
// ensure objects used by the function _cannot_ be deleted between the
// invocation and execution of the functor. AsyncInvoker is designed to
// help: any calls in flight will be cancelled when the AsyncInvoker used to
// make the call is destructed, and any calls executing will be allowed to
// complete before AsyncInvoker destructs.
//
// The easiest way to ensure lifetimes are handled correctly is to create a
// class that owns the Thread and AsyncInvoker objects, and then call its
// methods asynchronously as needed.
//
// Example:
//   class MyClass {
//    public:
//     void FireAsyncTaskWithResult(Thread* thread, int x) {
//       // Specify a callback to get the result upon completion.
//       invoker_.AsyncInvoke<int>(
//           thread, Bind(&MyClass::AsyncTaskWithResult, this, x),
//           &MyClass::OnTaskComplete, this);
//     }
//     void FireAnotherAsyncTask(Thread* thread) {
//       // No callback specified means fire-and-forget.
//       invoker_.AsyncInvoke<void>(
//           thread, Bind(&MyClass::AnotherAsyncTask, this));
//
//    private:
//     int AsyncTaskWithResult(int x) {
//       // Some long running process...
//       return x * x;
//     }
//     void AnotherAsyncTask() {
//       // Some other long running process...
//     }
//     void OnTaskComplete(int result) { result_ = result; }
//
//     AsyncInvoker invoker_;
//     int result_;
//   };
class AsyncInvoker {
 public:
  // Call |functor| asynchronously on |thread|, with no callback upon
  // completion. Returns immediately.
  template <class ReturnT, class FunctorT>
  void AsyncInvoke(Thread* thread,
                   const FunctorT& functor,
                   uint32 id = 0) {
    FunctorMessageHandler<ReturnT, FunctorT>* handler =
        new FunctorMessageHandler<ReturnT, FunctorT>(functor);
    handler->SetCallback(Bind(&AsyncInvoker::RemoveHandler, this, handler));
    InvokeHandler(thread, handler, id);
  }

  // Call |functor| asynchronously on |thread|, calling |callback| when done.
  template <class ReturnT, class FunctorT, class HostT>
  void AsyncInvoke(Thread* thread,
                   const FunctorT& functor,
                   void (HostT::*callback)(ReturnT),
                   HostT* callback_host,
                   uint32 id = 0) {
    AsyncFunctorMessageHandler<ReturnT, FunctorT>* handler =
        new AsyncFunctorMessageHandler<ReturnT, FunctorT>(functor);
    handler->WrapCallback(
        Bind(&AsyncInvoker::OnAsyncCallCompleted<ReturnT, FunctorT, HostT>,
             this, handler, callback, callback_host));
    InvokeHandler(thread, handler, id);
  }

  // Call |functor| asynchronously on |thread|, calling |callback| when done.
  // Overloaded for void return.
  template <class ReturnT, class FunctorT, class HostT>
  void AsyncInvoke(Thread* thread,
                   const FunctorT& functor,
                   void (HostT::*callback)(),
                   HostT* callback_host,
                   uint32 id = 0) {
    AsyncFunctorMessageHandler<void, FunctorT>* handler =
        new AsyncFunctorMessageHandler<ReturnT, FunctorT>(functor);
    handler->WrapCallback(
        Bind(&AsyncInvoker::OnAsyncVoidCallCompleted<FunctorT, HostT>,
             this, handler, callback, callback_host));
    InvokeHandler(thread, handler, id);
  }

  // Synchronously execute on |thread| all outstanding calls we own
  // that are pending on |thread|, and wait for calls to complete
  // before returning. Optionally filter by message id.
  // The destructor will not wait for outstanding calls, so if that
  // behavior is desired, call Flush() before destroying this object.
  void Flush(Thread* thread, uint32 id = MQID_ANY);

 private:
  void InvokeHandler(Thread* thread, MessageHandler* handler, uint32 id);
  void RemoveHandler(MessageHandler* handler);

  template <class ReturnT, class FunctorT, class HostT>
  void OnAsyncCallCompleted(
      AsyncFunctorMessageHandler<ReturnT, FunctorT>* handler,
      void (HostT::*callback)(ReturnT),
      HostT* callback_host) {
    AsyncInvoke<void>(handler->thread(),
                Bind(callback, callback_host, handler->result()));
    RemoveHandler(handler);
  }

  template <class FunctorT, class HostT>
  void OnAsyncVoidCallCompleted(
      AsyncFunctorMessageHandler<void, FunctorT>* handler,
      void (HostT::*callback)(),
      HostT* callback_host) {
    AsyncInvoke<void>(handler->thread(), Bind(callback, callback_host));
    RemoveHandler(handler);
  }

  CriticalSection crit_;
  ScopedPtrCollection<MessageHandler> handlers_;
};

}  // namespace talk_base


#endif  // TALK_BASE_ASYNCINVOKER_H_
