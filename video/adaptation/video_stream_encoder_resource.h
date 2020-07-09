/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_VIDEO_STREAM_ENCODER_RESOURCE_H_
#define VIDEO_ADAPTATION_VIDEO_STREAM_ENCODER_RESOURCE_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/adaptation/resource.h"
#include "api/task_queue/task_queue_base.h"
#include "call/adaptation/adaptation_constraint.h"
#include "call/adaptation/adaptation_listener.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/synchronization/sequence_checker.h"

namespace webrtc {

class VideoStreamEncoderResource : public Resource {
 public:
  ~VideoStreamEncoderResource() override;

  // Registering task queues must be performed as part of initialization.
  void RegisterEncoderTaskQueue(TaskQueueBase* encoder_queue);

  // Resource implementation.
  std::string Name() const override;
  void SetResourceListener(ResourceListener* listener) override;

  // Provides a pointer to the adaptation task queue. After this call, all
  // methods defined in this interface, including
  // UnregisterAdaptationTaskQueue() MUST be invoked on the adaptation task
  // queue. Registering the adaptation task queue may, however, happen off the
  // adaptation task queue.
  void RegisterAdaptationTaskQueue(TaskQueueBase* resource_adaptation_queue);
  // Signals that the adaptation task queue is no longer safe to use. No
  // assumptions must be made as to whether or not tasks in-flight will run.
  void UnregisterAdaptationTaskQueue();

 protected:
  explicit VideoStreamEncoderResource(std::string name);

  void OnResourceUsageStateMeasured(ResourceUsageState usage_state);

  // The caller is responsible for ensuring the task queue is still valid.
  TaskQueueBase* encoder_queue() const;
  // Validity of returned pointer is ensured by only allowing this method to be
  // called on the adaptation task queue. Designed for use with RTC_GUARDED_BY.
  // For posting from a different queue, use
  // MaybePostTaskToResourceAdaptationQueue() instead, which only posts if the
  // task queue is currently registered.
  TaskQueueBase* resource_adaptation_queue() const;
  template <typename Closure>
  void MaybePostTaskToResourceAdaptationQueue(Closure&& closure) {
    MutexLock lock(&lock_);
    if (!resource_adaptation_queue_)
      return;
    resource_adaptation_queue_->PostTask(ToQueuedTask(closure));
  }

 private:
  mutable Mutex lock_;
  const std::string name_;
  // Treated as const after initialization.
  TaskQueueBase* encoder_queue_;
  TaskQueueBase* resource_adaptation_queue_ RTC_GUARDED_BY(lock_);
  mutable Mutex listener_lock_;
  ResourceListener* listener_ RTC_GUARDED_BY(listener_lock_);
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_VIDEO_STREAM_ENCODER_RESOURCE_H_
