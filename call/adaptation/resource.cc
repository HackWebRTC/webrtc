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

Resource::Resource() : usage_state_(absl::nullopt), listener_(nullptr) {}

Resource::~Resource() {}

void Resource::SetResourceListener(ResourceListener* listener) {
  // If you want to change listener you need to unregister the old listener by
  // setting it to null first.
  RTC_DCHECK(!listener_ || !listener) << "A listener is already set";
  listener_ = listener;
}

absl::optional<ResourceUsageState> Resource::usage_state() const {
  return usage_state_;
}

void Resource::ClearUsageState() {
  usage_state_ = absl::nullopt;
}

bool Resource::IsAdaptationUpAllowed(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    const Resource& reason_resource) const {
  return true;
}

void Resource::OnAdaptationApplied(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    const Resource& reason_resource) {}

void Resource::OnResourceUsageStateMeasured(ResourceUsageState usage_state) {
  usage_state_ = usage_state;
  if (!listener_)
    return;
  listener_->OnResourceUsageStateMeasured(*this);
}

}  // namespace webrtc
