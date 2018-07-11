/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(bugs.webrtc.org/9078): Use absl::optional directly.
#ifndef API_OPTIONAL_H_
#define API_OPTIONAL_H_

#include "absl/types/optional.h"

namespace rtc {

using absl::nullopt_t;
using absl::nullopt;

template <typename T>
using Optional = absl::optional<T>;

}  // namespace rtc

#endif  // API_OPTIONAL_H_
