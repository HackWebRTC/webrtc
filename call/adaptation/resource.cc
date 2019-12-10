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

#include "rtc_base/strings/string_builder.h"

namespace webrtc {

namespace {

const char* ResourceUsageStateToString(ResourceUsageState usage_state) {
  switch (usage_state) {
    case ResourceUsageState::kOveruse:
      return "overuse";
    case ResourceUsageState::kStable:
      return "stable";
    case ResourceUsageState::kUnderuse:
      return "underuse";
  }
}

}  // namespace

Resource::~Resource() {}

std::string Resource::ToString() const {
  rtc::StringBuilder sb;
  sb << Name() << ": " << CurrentUsage() << " " << UsageUnitsOfMeasurement();
  sb << " (" << ResourceUsageStateToString(CurrentUsageState()) << ")";
  return sb.str();
}

}  // namespace webrtc
