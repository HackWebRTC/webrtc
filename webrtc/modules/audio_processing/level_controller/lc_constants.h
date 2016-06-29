/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_LC_CONSTANTS_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_LC_CONSTANTS_H_

namespace webrtc {

const float kMaxLcGain = 45;
const float kMaxLcNoisePower = 200.f * 200.f;
const float kTargetLcPeakLevel = 0.8f * 32767.f;

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_LC_CONSTANTS_H_
