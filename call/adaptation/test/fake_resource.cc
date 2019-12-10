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

FakeResource::FakeResource(std::string name, ResourceUsageState usage)
    : name_(std::move(name)), usage_(usage) {}

FakeResource::FakeResource(ResourceUsageState usage)
    : FakeResource("UnnamedResource", usage) {}

FakeResource::~FakeResource() {}

void FakeResource::set_usage(ResourceUsageState usage) {
  usage_ = usage;
}

std::string FakeResource::Name() const {
  return name_;
}

std::string FakeResource::UsageUnitsOfMeasurement() const {
  return "%";
}

double FakeResource::CurrentUsage() const {
  switch (usage_) {
    case ResourceUsageState::kOveruse:
      return 1.2;
    case ResourceUsageState::kStable:
      return 0.8;
    case ResourceUsageState::kUnderuse:
      return 0.4;
  }
}

ResourceUsageState FakeResource::CurrentUsageState() const {
  return usage_;
}

}  // namespace webrtc
