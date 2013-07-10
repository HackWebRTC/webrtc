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

#include "talk/base/task.h"
#include "talk/base/common.h"
#include "talk/base/taskrunner.h"

namespace talk_base {

int32 Task::unique_id_seed_ = 0;

Task::Task(TaskParent *parent)
    : TaskParent(this, parent),
      state_(STATE_INIT),
      blocked_(false),
      done_(false),
      aborted_(false),
      busy_(false),
      error_(false),
      start_time_(0),
      timeout_time_(0),
      timeout_seconds_(0),
      timeout_suspended_(false)  {
  unique_id_ = unique_id_seed_++;

  // sanity check that we didn't roll-over our id seed
  ASSERT(unique_id_ < unique_id_seed_);
}

Task::~Task() {
  // Is this task being deleted in the correct manner?
  ASSERT(!done_ || GetRunner()->is_ok_to_delete(this));
  ASSERT(state_ == STATE_INIT || done_);
  ASSERT(state_ == STATE_INIT || blocked_);

  // If the task is being deleted without being done, it
  // means that it hasn't been removed from its parent.
  // This happens if a task is deleted outside of TaskRunner.
  if (!done_) {
    Stop();
  }
}

int64 Task::CurrentTime() {
  return GetRunner()->CurrentTime();
}

int64 Task::ElapsedTime() {
  return CurrentTime() - start_time_;
}

void Task::Start() {
  if (state_ != STATE_INIT)
    return;
  // Set the start time before starting the task.  Otherwise if the task
  // finishes quickly and deletes the Task object, setting start_time_
  // will crash.
  start_time_ = CurrentTime();
  GetRunner()->StartTask(this);
}

void Task::Step() {
  if (done_) {
#ifdef _DEBUG
    // we do not know how !blocked_ happens when done_ - should be impossible.
    // But it causes problems, so in retail build, we force blocked_, and
    // under debug we assert.
    ASSERT(blocked_);
#else
    blocked_ = true;
#endif
    return;
  }

  // Async Error() was called
  if (error_) {
    done_ = true;
    state_ = STATE_ERROR;
    blocked_ = true;
//   obsolete - an errored task is not considered done now
//   SignalDone();

    Stop();
#ifdef _DEBUG
    // verify that stop removed this from its parent
    ASSERT(!parent()->IsChildTask(this));
#endif
    return;
  }

  busy_ = true;
  int new_state = Process(state_);
  busy_ = false;

  if (aborted_) {
    Abort(true);  // no need to wake because we're awake
    return;
  }

  if (new_state == STATE_BLOCKED) {
    blocked_ = true;
    // Let the timeout continue
  } else {
    state_ = new_state;
    blocked_ = false;
    ResetTimeout();
  }

  if (new_state == STATE_DONE) {
    done_ = true;
  } else if (new_state == STATE_ERROR) {
    done_ = true;
    error_ = true;
  }

  if (done_) {
//  obsolete - call this yourself
//    SignalDone();

    Stop();
#if _DEBUG
    // verify that stop removed this from its parent
    ASSERT(!parent()->IsChildTask(this));
#endif
    blocked_ = true;
  }
}

void Task::Abort(bool nowake) {
  // Why only check for done_ (instead of "aborted_ || done_")?
  //
  // If aborted_ && !done_, it means the logic for aborting still
  // needs to be executed (because busy_ must have been true when
  // Abort() was previously called).
  if (done_)
    return;
  aborted_ = true;
  if (!busy_) {
    done_ = true;
    blocked_ = true;
    error_ = true;

    // "done_" is set before calling "Stop()" to ensure that this code 
    // doesn't execute more than once (recursively) for the same task.
    Stop();
#ifdef _DEBUG
    // verify that stop removed this from its parent
    ASSERT(!parent()->IsChildTask(this));
#endif
    if (!nowake) {
      // WakeTasks to self-delete.
      // Don't call Wake() because it is a no-op after "done_" is set.
      // Even if Wake() did run, it clears "blocked_" which isn't desireable.
      GetRunner()->WakeTasks();
    }
  }
}

void Task::Wake() {
  if (done_)
    return;
  if (blocked_) {
    blocked_ = false;
    GetRunner()->WakeTasks();
  }
}

void Task::Error() {
  if (error_ || done_)
    return;
  error_ = true;
  Wake();
}

std::string Task::GetStateName(int state) const {
  switch (state) {
    case STATE_BLOCKED: return "BLOCKED";
    case STATE_INIT: return "INIT";
    case STATE_START: return "START";
    case STATE_DONE: return "DONE";
    case STATE_ERROR: return "ERROR";
    case STATE_RESPONSE: return "RESPONSE";
  }
  return "??";
}

int Task::Process(int state) {
  int newstate = STATE_ERROR;

  if (TimedOut()) {
    ClearTimeout();
    newstate = OnTimeout();
    SignalTimeout();
  } else {
    switch (state) {
      case STATE_INIT:
        newstate = STATE_START;
        break;
      case STATE_START:
        newstate = ProcessStart();
        break;
      case STATE_RESPONSE:
        newstate = ProcessResponse();
        break;
      case STATE_DONE:
      case STATE_ERROR:
        newstate = STATE_BLOCKED;
        break;
    }
  }

  return newstate;
}

void Task::Stop() {
  // No need to wake because we're either awake or in abort
  TaskParent::OnStopped(this);
}

void Task::set_timeout_seconds(const int timeout_seconds) {
  timeout_seconds_ = timeout_seconds;
  ResetTimeout();
}

bool Task::TimedOut() {
  return timeout_seconds_ &&
    timeout_time_ &&
    CurrentTime() >= timeout_time_;
}

void Task::ResetTimeout() {
  int64 previous_timeout_time = timeout_time_;
  bool timeout_allowed = (state_ != STATE_INIT)
                      && (state_ != STATE_DONE)
                      && (state_ != STATE_ERROR);
  if (timeout_seconds_ && timeout_allowed && !timeout_suspended_)
    timeout_time_ = CurrentTime() +
                    (timeout_seconds_ * kSecToMsec * kMsecTo100ns);
  else
    timeout_time_ = 0;

  GetRunner()->UpdateTaskTimeout(this, previous_timeout_time);
}

void Task::ClearTimeout() {
  int64 previous_timeout_time = timeout_time_;
  timeout_time_ = 0;
  GetRunner()->UpdateTaskTimeout(this, previous_timeout_time);
}

void Task::SuspendTimeout() {
  if (!timeout_suspended_) {
    timeout_suspended_ = true;
    ResetTimeout();
  }
}

void Task::ResumeTimeout() {
  if (timeout_suspended_) {
    timeout_suspended_ = false;
    ResetTimeout();
  }
}

} // namespace talk_base
