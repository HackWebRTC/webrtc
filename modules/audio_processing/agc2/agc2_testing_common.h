/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_AGC2_TESTING_COMMON_H_
#define MODULES_AUDIO_PROCESSING_AGC2_AGC2_TESTING_COMMON_H_

#include <vector>

#include "rtc_base/basictypes.h"

namespace webrtc {

namespace test {

// Level Estimator test parameters.
constexpr float kDecayMs = 500.f;

// Limiter parameters.
constexpr float kLimiterMaxInputLevelDbFs = 1.f;
constexpr float kLimiterKneeSmoothnessDb = 1.f;
constexpr float kLimiterCompressionRatio = 5.f;

std::vector<double> LinSpace(const double l, const double r, size_t num_points);
}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_AGC2_TESTING_COMMON_H_
