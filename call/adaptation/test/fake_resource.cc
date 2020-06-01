/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/test/fake_resource.h"

#include <algorithm>
#include <utility>

#include "rtc_base/ref_counted_object.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {

// static
rtc::scoped_refptr<FakeResource> FakeResource::Create(std::string name) {
  return new rtc::RefCountedObject<FakeResource>(name);
}

FakeResource::FakeResource(std::string name)
    : Resource(),
      lock_(),
      name_(std::move(name)),
      resource_adaptation_queue_(nullptr),
      is_adaptation_up_allowed_(true),
      num_adaptations_applied_(0),
      usage_state_(absl::nullopt),
      listener_(nullptr) {}

FakeResource::~FakeResource() {}

void FakeResource::set_usage_state(ResourceUsageState usage_state) {
  if (!resource_adaptation_queue_->IsCurrent()) {
    resource_adaptation_queue_->PostTask(ToQueuedTask(
        [this_ref = rtc::scoped_refptr<FakeResource>(this), usage_state] {
          this_ref->set_usage_state(usage_state);
        }));
    return;
  }
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  usage_state_ = usage_state;
  if (listener_) {
    listener_->OnResourceUsageStateMeasured(this);
  }
}

void FakeResource::set_is_adaptation_up_allowed(bool is_adaptation_up_allowed) {
  rtc::CritScope crit(&lock_);
  is_adaptation_up_allowed_ = is_adaptation_up_allowed;
}

size_t FakeResource::num_adaptations_applied() const {
  rtc::CritScope crit(&lock_);
  return num_adaptations_applied_;
}

void FakeResource::RegisterAdaptationTaskQueue(
    TaskQueueBase* resource_adaptation_queue) {
  RTC_DCHECK(!resource_adaptation_queue_);
  RTC_DCHECK(resource_adaptation_queue);
  resource_adaptation_queue_ = resource_adaptation_queue;
}

void FakeResource::UnregisterAdaptationTaskQueue() {
  RTC_DCHECK(resource_adaptation_queue_);
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  resource_adaptation_queue_ = nullptr;
}

void FakeResource::SetResourceListener(ResourceListener* listener) {
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  listener_ = listener;
}

std::string FakeResource::Name() const {
  return name_;
}

absl::optional<ResourceUsageState> FakeResource::UsageState() const {
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  return usage_state_;
}

void FakeResource::ClearUsageState() {
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  usage_state_ = absl::nullopt;
}

bool FakeResource::IsAdaptationUpAllowed(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    rtc::scoped_refptr<Resource> reason_resource) const {
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  rtc::CritScope crit(&lock_);
  return is_adaptation_up_allowed_;
}

void FakeResource::OnAdaptationApplied(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    rtc::scoped_refptr<Resource> reason_resource) {
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  rtc::CritScope crit(&lock_);
  ++num_adaptations_applied_;
}

}  // namespace webrtc
