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

#include <utility>

namespace webrtc {

FakeResource::FakeResource(std::string name)
    : Resource(),
      name_(std::move(name)),
      is_adaptation_up_allowed_(true),
      num_adaptations_applied_(0) {}

FakeResource::~FakeResource() {}

void FakeResource::set_usage_state(ResourceUsageState usage_state) {
  OnResourceUsageStateMeasured(usage_state);
}

void FakeResource::set_is_adaptation_up_allowed(bool is_adaptation_up_allowed) {
  is_adaptation_up_allowed_ = is_adaptation_up_allowed;
}

size_t FakeResource::num_adaptations_applied() const {
  return num_adaptations_applied_;
}

bool FakeResource::IsAdaptationUpAllowed(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    const Resource& reason_resource) const {
  return is_adaptation_up_allowed_;
}

void FakeResource::OnAdaptationApplied(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    const Resource& reason_resource) {
  ++num_adaptations_applied_;
}

}  // namespace webrtc
