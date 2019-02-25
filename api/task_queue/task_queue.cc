/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/task_queue.h"

#include "api/task_queue/global_task_queue_factory.h"
#include "api/task_queue/task_queue_base.h"

namespace rtc {

TaskQueue::TaskQueue(const char* queue_name, Priority priority)
    : impl_(webrtc::GlobalTaskQueueFactory()
                .CreateTaskQueue(queue_name, priority)
                .release()) {
  impl_->task_queue_ = this;
}

TaskQueue::~TaskQueue() {
  impl_->Delete();
}

// static
TaskQueue* TaskQueue::Current() {
  webrtc::TaskQueueBase* impl = webrtc::TaskQueueBase::Current();
  if (impl == nullptr) {
    return nullptr;
  }
  return impl->task_queue_;
}

bool TaskQueue::IsCurrent() const {
  return Current() == this;
}

void TaskQueue::PostTask(std::unique_ptr<QueuedTask> task) {
  return impl_->PostTask(std::move(task));
}

void TaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                uint32_t milliseconds) {
  return impl_->PostDelayedTask(std::move(task), milliseconds);
}

}  // namespace rtc
