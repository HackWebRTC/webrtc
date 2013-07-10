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

#ifndef TALK_BASE_TASKRUNNER_H__
#define TALK_BASE_TASKRUNNER_H__

#include <vector>

#include "talk/base/basictypes.h"
#include "talk/base/sigslot.h"
#include "talk/base/taskparent.h"

namespace talk_base {
class Task;

const int64 kSecToMsec = 1000;
const int64 kMsecTo100ns = 10000;
const int64 kSecTo100ns = kSecToMsec * kMsecTo100ns;

class TaskRunner : public TaskParent, public sigslot::has_slots<> {
 public:
  TaskRunner();
  virtual ~TaskRunner();

  virtual void WakeTasks() = 0;

  // Returns the current time in 100ns units.  It is used for
  // determining timeouts.  The origin is not important, only
  // the units and that rollover while the computer is running.
  //
  // On Windows, GetSystemTimeAsFileTime is the typical implementation.
  virtual int64 CurrentTime() = 0 ;

  void StartTask(Task *task);
  void RunTasks();
  void PollTasks();

  void UpdateTaskTimeout(Task *task, int64 previous_task_timeout_time);

#ifdef _DEBUG
  bool is_ok_to_delete(Task* task) {
    return task == deleting_task_;
  }

  void IncrementAbortCount() {
    ++abort_count_;
  }

  void DecrementAbortCount() {
    --abort_count_;
  }
#endif

  // Returns the next absolute time when a task times out
  // OR "0" if there is no next timeout.
  int64 next_task_timeout() const;

 protected:
  // The primary usage of this method is to know if
  // a callback timer needs to be set-up or adjusted.
  // This method will be called
  //  * when the next_task_timeout() becomes a smaller value OR
  //  * when next_task_timeout() has changed values and the previous
  //    value is in the past.
  //
  // If the next_task_timeout moves to the future, this method will *not*
  // get called (because it subclass should check next_task_timeout()
  // when its timer goes off up to see if it needs to set-up a new timer).
  //
  // Note that this maybe called conservatively.  In that it may be
  // called when no time change has happened.
  virtual void OnTimeoutChange() {
    // by default, do nothing.
  }

 private:
  void InternalRunTasks(bool in_destructor);
  void CheckForTimeoutChange(int64 previous_timeout_time);

  std::vector<Task *> tasks_;
  Task *next_timeout_task_;
  bool tasks_running_;
#ifdef _DEBUG
  int abort_count_;
  Task* deleting_task_;
#endif

  void RecalcNextTimeout(Task *exclude_task);
};

} // namespace talk_base

#endif  // TASK_BASE_TASKRUNNER_H__
