/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/noise_level_estimator.h"

namespace webrtc {

float NoiseLevelEstimator::Analyze(AudioFrameView<const float> frame) {
  // TODO(webrtc:7494): This is a stub. Add implementation.
  return -50.f;
}

}  // namespace webrtc
