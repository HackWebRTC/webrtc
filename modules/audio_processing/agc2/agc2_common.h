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

namespace webrtc {

constexpr float kMinSampleValue = -32768.f;
constexpr float kMaxSampleValue = 32767.f;

// TODO(aleloi): add the other constants as more AGC2 components are
// added.
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_AGC2_COMMON_H_
