/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains rtc::MakeUnique and rtc::WrapUnique, which are backwards
// compatibility aliases for absl::make_unique and absl::WrapUnique,
// respectively. This file will go away soon; use the Abseil types directly in
// new code.

#ifndef RTC_BASE_PTR_UTIL_H_
#define RTC_BASE_PTR_UTIL_H_

#include "absl/memory/memory.h"

namespace rtc {

template <typename T, typename... Args>
auto MakeUnique(Args&&... args)
    -> decltype(absl::make_unique<T, Args...>(std::forward<Args>(args)...)) {
  return absl::make_unique<T, Args...>(std::forward<Args>(args)...);
}

template <typename T>
auto MakeUnique(size_t n) -> decltype(absl::make_unique<T>(n)) {
  return absl::make_unique<T>(n);
}

using absl::WrapUnique;

}  // namespace rtc

#endif  // RTC_BASE_PTR_UTIL_H_
