/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/video_stream_encoder_resource.h"

#include <algorithm>
#include <utility>

namespace webrtc {

VideoStreamEncoderResource::VideoStreamEncoderResource(std::string name)
    : lock_(),
      name_(std::move(name)),
      encoder_queue_(nullptr),
      resource_adaptation_queue_(nullptr),
      listener_(nullptr) {}

VideoStreamEncoderResource::~VideoStreamEncoderResource() {
  RTC_DCHECK(!listener_)
      << "There is a listener depending on a VideoStreamEncoderResource being "
      << "destroyed.";
}

void VideoStreamEncoderResource::RegisterEncoderTaskQueue(
    TaskQueueBase* encoder_queue) {
  RTC_DCHECK(!encoder_queue_);
  RTC_DCHECK(encoder_queue);
  encoder_queue_ = encoder_queue;
}

void VideoStreamEncoderResource::RegisterAdaptationTaskQueue(
    TaskQueueBase* resource_adaptation_queue) {
  MutexLock lock(&lock_);
  RTC_DCHECK(!resource_adaptation_queue_);
  RTC_DCHECK(resource_adaptation_queue);
  resource_adaptation_queue_ = resource_adaptation_queue;
}

void VideoStreamEncoderResource::UnregisterAdaptationTaskQueue() {
  MutexLock lock(&lock_);
  RTC_DCHECK(resource_adaptation_queue_);
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  resource_adaptation_queue_ = nullptr;
}

void VideoStreamEncoderResource::SetResourceListener(
    ResourceListener* listener) {
  // If you want to change listener you need to unregister the old listener by
  // setting it to null first.
  MutexLock crit(&listener_lock_);
  RTC_DCHECK(!listener_ || !listener) << "A listener is already set";
  listener_ = listener;
}

std::string VideoStreamEncoderResource::Name() const {
  return name_;
}

void VideoStreamEncoderResource::OnResourceUsageStateMeasured(
    ResourceUsageState usage_state) {
  MutexLock crit(&listener_lock_);
  if (listener_) {
    listener_->OnResourceUsageStateMeasured(this, usage_state);
  }
}

TaskQueueBase* VideoStreamEncoderResource::encoder_queue() const {
  return encoder_queue_;
}

TaskQueueBase* VideoStreamEncoderResource::resource_adaptation_queue() const {
  MutexLock lock(&lock_);
  RTC_DCHECK(resource_adaptation_queue_);
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  return resource_adaptation_queue_;
}

}  // namespace webrtc
