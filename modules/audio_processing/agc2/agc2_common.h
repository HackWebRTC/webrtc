/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_AGC2_COMMON_H_
#define MODULES_AUDIO_PROCESSING_AGC2_AGC2_COMMON_H_

#include <cmath>

#include "rtc_base/basictypes.h"

namespace webrtc {

constexpr float kMinFloatS16Value = -32768.f;
constexpr float kMaxFloatS16Value = 32767.f;
constexpr double kMaxAbsFloatS16Value = 32768.0;

constexpr size_t kFrameDurationMs = 10;
constexpr size_t kSubFramesInFrame = 20;
constexpr size_t kMaximalNumberOfSamplesPerChannel = 480;

constexpr float kAttackFilterConstant = 0.f;

// Used in the Level Estimator for deciding when to update the speech
// level estimate.
constexpr float kVadConfidenceThreshold = 0.9f;

// The amount of 'memory' of the Level Estimator. Decides leak factors.
constexpr float kFullBufferSizeMs = 1000.f;
constexpr float kFullBufferLeakFactor = 1.f - 1.f / kFullBufferSizeMs;

constexpr float kInitialSpeechLevelEstimateDbfs = -30.f;

constexpr float kInitialSaturationMarginDb = 17.f;

// This is computed from kDecayMs by
// 10 ** (-1/20 * subframe_duration / kDecayMs).
// |subframe_duration| is |kFrameDurationMs / kSubFramesInFrame|.
// kDecayMs is defined in agc2_testing_common.h
constexpr float kDecayFilterConstant = 0.9998848773724686f;

// Number of interpolation points for each region of the limiter.
// These values have been tuned to limit the interpolated gain curve error given
// the limiter parameters and allowing a maximum error of +/- 32768^-1.
constexpr size_t kInterpolatedGainCurveKneePoints = 22;
constexpr size_t kInterpolatedGainCurveBeyondKneePoints = 10;
constexpr size_t kInterpolatedGainCurveTotalPoints =
    kInterpolatedGainCurveKneePoints + kInterpolatedGainCurveBeyondKneePoints;

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_AGC2_COMMON_H_
