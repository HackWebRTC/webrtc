/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/test/fake_adaptation_listener.h"

namespace webrtc {

FakeAdaptationListener::FakeAdaptationListener()
    : num_adaptations_applied_(0) {}

FakeAdaptationListener::~FakeAdaptationListener() {}

size_t FakeAdaptationListener::num_adaptations_applied() const {
  return num_adaptations_applied_;
}

void FakeAdaptationListener::OnAdaptationApplied(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    rtc::scoped_refptr<Resource> reason_resource) {
  ++num_adaptations_applied_;
}

}  // namespace webrtc
