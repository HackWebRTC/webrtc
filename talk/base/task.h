/*
 * libjingle
 * Copyright 2004--2006, Google Inc.
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

#ifndef TALK_BASE_TASK_H__
#define TALK_BASE_TASK_H__

#include <string>
#include "talk/base/basictypes.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/base/taskparent.h"

/////////////////////////////////////////////////////////////////////
//
// TASK
//
/////////////////////////////////////////////////////////////////////
//
// Task is a state machine infrastructure.  States are pushed forward by
// pushing forwards a TaskRunner that holds on to all Tasks.  The purpose
// of Task is threefold:
//
// (1) It manages ongoing work on the UI thread.  Multitasking without
// threads, keeping it easy, keeping it real. :-)  It does this by
// organizing a set of states for each task.  When you return from your
// Process*() function, you return an integer for the next state.  You do
// not go onto the next state yourself.  Every time you enter a state,
// you check to see if you can do anything yet.  If not, you return
// STATE_BLOCKED.  If you _could_ do anything, do not return
// STATE_BLOCKED - even if you end up in the same state, return
// STATE_mysamestate.  When you are done, return STATE_DONE and then the
// task will self-delete sometime afterwards.
//
// (2) It helps you avoid all those reentrancy problems when you chain
// too many triggers on one thread.  Basically if you want to tell a task
// to process something for you, you feed your task some information and
// then you Wake() it.  Don't tell it to process it right away.  If it
// might be working on something as you send it information, you may want
// to have a queue in the task.
//
// (3) Finally it helps manage parent tasks and children.  If a parent
// task gets aborted, all the children tasks are too.  The nice thing
// about this, for example, is if you have one parent task that
// represents, say, and Xmpp connection, then you can spawn a whole bunch
// of infinite lifetime child tasks and now worry about cleaning them up.
//  When the parent task goes to STATE_DONE, the task engine will make
// sure all those children are aborted and get deleted.
//
// Notice that Task has a few built-in states, e.g.,
//
// STATE_INIT - the task isn't running yet
// STATE_START - the task is in its first state
// STATE_RESPONSE - the task is in its second state
// STATE_DONE - the task is done
//
// STATE_ERROR - indicates an error - we should audit the error code in
// light of any usage of it to see if it should be improved.  When I
// first put down the task stuff I didn't have a good sense of what was
// needed for Abort and Error, and now the subclasses of Task will ground
// the design in a stronger way.
//
// STATE_NEXT - the first undefined state number.  (like WM_USER) - you
// can start defining more task states there.
//
// When you define more task states, just override Process(int state) and
// add your own switch statement.  If you want to delegate to
// Task::Process, you can effectively delegate to its switch statement.
// No fancy method pointers or such - this is all just pretty low tech,
// easy to debug, and fast.
//
// Also notice that Task has some primitive built-in timeout functionality.
//
// A timeout is defined as "the task stays in STATE_BLOCKED longer than
// timeout_seconds_."
//
// Descendant classes can override this behavior by calling the
// various protected methods to change the timeout behavior.  For
// instance, a descendand might call SuspendTimeout() when it knows
// that it isn't waiting for anything that might timeout, but isn't
// yet in the STATE_DONE state.
//

namespace talk_base {

// Executes a sequence of steps
class Task : public TaskParent {
 public:
  Task(TaskParent *parent);
  virtual ~Task();

  int32 unique_id() { return unique_id_; }

  void Start();
  void Step();
  int GetState() const { return state_; }
  bool HasError() const { return (GetState() == STATE_ERROR); }
  bool Blocked() const { return blocked_; }
  bool IsDone() const { return done_; }
  int64 ElapsedTime();

  // Called from outside to stop task without any more callbacks
  void Abort(bool nowake = false);

  bool TimedOut();

  int64 timeout_time() const { return timeout_time_; }
  int timeout_seconds() const { return timeout_seconds_; }
  void set_timeout_seconds(int timeout_seconds);

  sigslot::signal0<> SignalTimeout;

  // Called inside the task to signal that the task may be unblocked
  void Wake();

 protected:

  enum {
    STATE_BLOCKED = -1,
    STATE_INIT = 0,
    STATE_START = 1,
    STATE_DONE = 2,
    STATE_ERROR = 3,
    STATE_RESPONSE = 4,
    STATE_NEXT = 5,  // Subclasses which need more states start here and higher
  };

  // Called inside to advise that the task should wake and signal an error
  void Error();

  int64 CurrentTime();

  virtual std::string GetStateName(int state) const;
  virtual int Process(int state);
  virtual void Stop();
  virtual int ProcessStart() = 0;
  virtual int ProcessResponse() { return STATE_DONE; }

  void ResetTimeout();
  void ClearTimeout();

  void SuspendTimeout();
  void ResumeTimeout();

 protected:
  virtual int OnTimeout() {
    // by default, we are finished after timing out
    return STATE_DONE;
  }

 private:
  void Done();

  int state_;
  bool blocked_;
  bool done_;
  bool aborted_;
  bool busy_;
  bool error_;
  int64 start_time_;
  int64 timeout_time_;
  int timeout_seconds_;
  bool timeout_suspended_;
  int32 unique_id_;
  
  static int32 unique_id_seed_;
};

}  // namespace talk_base

#endif  // TALK_BASE_TASK_H__
