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

#include "rtc_base/basictypes.h"

namespace webrtc {

constexpr float kMinSampleValue = -32768.f;
constexpr float kMaxSampleValue = 32767.f;

constexpr size_t kFrameDurationMs = 10;
constexpr size_t kSubFramesInFrame = 20;

constexpr float kAttackFilterConstant = 0.f;

constexpr size_t kMaximalNumberOfSamplesPerChannel = 480;

// This is computed from kDecayMs by
// 10 ** (-1/20 * subframe_duration / kDecayMs).
// |subframe_duration| is |kFrameDurationMs / kSubFramesInFrame|.
// kDecayMs is defined in agc2_testing_common.h
constexpr float kDecayFilterConstant = 0.9998848773724686f;

// TODO(aleloi): add the other constants as more AGC2 components are
// added.
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_AGC2_COMMON_H_
