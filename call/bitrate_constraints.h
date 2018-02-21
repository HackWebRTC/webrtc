/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_BITRATE_CONSTRAINTS_H_
#define CALL_BITRATE_CONSTRAINTS_H_

#include <algorithm>

#include "api/optional.h"

namespace webrtc {
// TODO(srte): BitrateConstraints and BitrateConstraintsMask should be merged.
// Both represent the same kind data, but are using different default
// initializer and representation of unset values.
struct BitrateConstraints {
  int min_bitrate_bps = 0;
  int start_bitrate_bps = kDefaultStartBitrateBps;
  int max_bitrate_bps = -1;

 private:
  static constexpr int kDefaultStartBitrateBps = 300000;
};

// BitrateConstraintsMask is used for the local client's bitrate preferences.
// Semantically it carries the same kind of information as BitrateConstraints,
// but is used in a slightly different way.
struct BitrateConstraintsMask {
  BitrateConstraintsMask();
  ~BitrateConstraintsMask();
  BitrateConstraintsMask(const BitrateConstraintsMask&);
  rtc::Optional<int> min_bitrate_bps;
  rtc::Optional<int> start_bitrate_bps;
  rtc::Optional<int> max_bitrate_bps;
};

// Like std::min, but considers non-positive values to be unset.
template <typename T>
static T MinPositive(T a, T b) {
  if (a <= 0) {
    return b;
  }
  if (b <= 0) {
    return a;
  }
  return std::min(a, b);
}
}  // namespace webrtc
#endif  // CALL_BITRATE_CONSTRAINTS_H_
