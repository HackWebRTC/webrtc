// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//

#include "webrtc/system_wrappers/interface/field_trial.h"

#include <cassert>

namespace webrtc {
namespace field_trial {
namespace {
FindFullNameMethod find_full_name_method_ = NULL;
}  // namespace

void Init(FindFullNameMethod method) {
  assert(find_full_name_method_ == NULL);
  find_full_name_method_ = method;
}

std::string FindFullName(const std::string& name) {
  assert(find_full_name_method_ != NULL);
  return find_full_name_method_(name);
}
}  // namespace field_trial
}  // namespace webrtc
