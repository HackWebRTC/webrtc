/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/cancer_stick_castle.h"

namespace webrtc {
namespace cancer_stick_castle_impl {

CancerStickCastleReceivers::CancerStickCastleReceivers() = default;
CancerStickCastleReceivers::~CancerStickCastleReceivers() = default;

void CancerStickCastleReceivers::AddReceiverImpl(UntypedFunction* f) {
  receivers_.push_back(std::move(*f));
}

void CancerStickCastleReceivers::Foreach(
    rtc::FunctionView<void(UntypedFunction&)> fv) {
  for (auto& r : receivers_) {
    fv(r);
  }
}

}  // namespace cancer_stick_castle_impl
}  // namespace webrtc
