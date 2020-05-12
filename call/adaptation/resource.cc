/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/resource.h"

#include "absl/algorithm/container.h"
#include "rtc_base/checks.h"

namespace webrtc {

ResourceListener::~ResourceListener() {}

Resource::Resource()
    : encoder_queue_(nullptr),
      resource_adaptation_queue_(nullptr),
      usage_state_(absl::nullopt),
      listener_(nullptr) {}

Resource::~Resource() {
  RTC_DCHECK(!listener_)
      << "There is a listener depending on a Resource being destroyed.";
}

void Resource::Initialize(rtc::TaskQueue* encoder_queue,
                          rtc::TaskQueue* resource_adaptation_queue) {
  RTC_DCHECK(!encoder_queue_);
  RTC_DCHECK(encoder_queue);
  RTC_DCHECK(!resource_adaptation_queue_);
  RTC_DCHECK(resource_adaptation_queue);
  encoder_queue_ = encoder_queue;
  resource_adaptation_queue_ = resource_adaptation_queue;
}

void Resource::SetResourceListener(ResourceListener* listener) {
  RTC_DCHECK(resource_adaptation_queue_);
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  // If you want to change listener you need to unregister the old listener by
  // setting it to null first.
  RTC_DCHECK(!listener_ || !listener) << "A listener is already set";
  listener_ = listener;
}

absl::optional<ResourceUsageState> Resource::usage_state() const {
  RTC_DCHECK(resource_adaptation_queue_);
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  return usage_state_;
}

void Resource::ClearUsageState() {
  RTC_DCHECK(resource_adaptation_queue_);
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  usage_state_ = absl::nullopt;
}

bool Resource::IsAdaptationUpAllowed(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    rtc::scoped_refptr<Resource> reason_resource) const {
  return true;
}

void Resource::OnAdaptationApplied(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    rtc::scoped_refptr<Resource> reason_resource) {}

rtc::TaskQueue* Resource::encoder_queue() const {
  return encoder_queue_;
}

rtc::TaskQueue* Resource::resource_adaptation_queue() const {
  return resource_adaptation_queue_;
}

void Resource::OnResourceUsageStateMeasured(ResourceUsageState usage_state) {
  RTC_DCHECK(resource_adaptation_queue_);
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  usage_state_ = usage_state;
  if (!listener_)
    return;
  listener_->OnResourceUsageStateMeasured(this);
}

}  // namespace webrtc
