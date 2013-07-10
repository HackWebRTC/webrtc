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

#ifndef TALK_BASE_WORKER_H_
#define TALK_BASE_WORKER_H_

#include "talk/base/constructormagic.h"
#include "talk/base/messagehandler.h"

namespace talk_base {

class Thread;

// A worker is an object that performs some specific long-lived task in an
// event-driven manner.
// The only method that should be considered thread-safe is HaveWork(), which
// allows you to signal the availability of work from any thread. All other
// methods are thread-hostile. Specifically:
// StartWork()/StopWork() should not be called concurrently with themselves or
// each other, and it is an error to call them while the worker is running on
// a different thread.
// The destructor may not be called if the worker is currently running
// (regardless of the thread), but you can call StopWork() in a subclass's
// destructor.
class Worker : private MessageHandler {
 public:
  Worker();

  // Destroys this Worker, but it must have already been stopped via StopWork().
  virtual ~Worker();

  // Attaches the worker to the current thread and begins processing work if not
  // already doing so.
  bool StartWork();
  // Stops processing work if currently doing so and detaches from the current
  // thread.
  bool StopWork();

 protected:
  // Signal that work is available to be done. May only be called within the
  // lifetime of a OnStart()/OnStop() pair.
  void HaveWork();

  // These must be implemented by a subclass.
  // Called on the worker thread to start working.
  virtual void OnStart() = 0;
  // Called on the worker thread when work has been signalled via HaveWork().
  virtual void OnHaveWork() = 0;
  // Called on the worker thread to stop working. Upon return, any pending
  // OnHaveWork() calls are cancelled.
  virtual void OnStop() = 0;

 private:
  // Inherited from MessageHandler.
  virtual void OnMessage(Message *msg);

  // The thread that is currently doing the work.
  Thread *worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

}  // namespace talk_base

#endif  // TALK_BASE_WORKER_H_
