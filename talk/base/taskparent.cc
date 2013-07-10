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

#include <algorithm>

#include "talk/base/taskparent.h"

#include "talk/base/task.h"
#include "talk/base/taskrunner.h"

namespace talk_base {

TaskParent::TaskParent(Task* derived_instance, TaskParent *parent)
    : parent_(parent) {
  ASSERT(derived_instance != NULL);
  ASSERT(parent != NULL);
  runner_ = parent->GetRunner();
  parent_->AddChild(derived_instance);
  Initialize();
}

TaskParent::TaskParent(TaskRunner *derived_instance)
    : parent_(NULL),
      runner_(derived_instance) {
  ASSERT(derived_instance != NULL);
  Initialize();
}

// Does common initialization of member variables
void TaskParent::Initialize() {
  children_.reset(new ChildSet());
  child_error_ = false;
}

void TaskParent::AddChild(Task *child) {
  children_->insert(child);
}

#ifdef _DEBUG
bool TaskParent::IsChildTask(Task *task) {
  ASSERT(task != NULL);
  return task->parent_ == this && children_->find(task) != children_->end();
}
#endif

bool TaskParent::AllChildrenDone() {
  for (ChildSet::iterator it = children_->begin();
       it != children_->end();
       ++it) {
    if (!(*it)->IsDone())
      return false;
  }
  return true;
}

bool TaskParent::AnyChildError() {
  return child_error_;
}

void TaskParent::AbortAllChildren() {
  if (children_->size() > 0) {
#ifdef _DEBUG
    runner_->IncrementAbortCount();
#endif

    ChildSet copy = *children_;
    for (ChildSet::iterator it = copy.begin(); it != copy.end(); ++it) {
      (*it)->Abort(true);  // Note we do not wake
    }

#ifdef _DEBUG
    runner_->DecrementAbortCount();
#endif
  }
}

void TaskParent::OnStopped(Task *task) {
  AbortAllChildren();
  parent_->OnChildStopped(task);
}

void TaskParent::OnChildStopped(Task *child) {
  if (child->HasError())
    child_error_ = true;
  children_->erase(child);
}

} // namespace talk_base
