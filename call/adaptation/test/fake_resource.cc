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

FakeResource::FakeResource(ResourceUsageState usage_state) : Resource() {
  set_usage_state(usage_state);
}

FakeResource::~FakeResource() {}

void FakeResource::set_usage_state(ResourceUsageState usage_state) {
  OnResourceUsageStateMeasured(usage_state);
}

}  // namespace webrtc
