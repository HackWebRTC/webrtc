/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_NOISE_LEVEL_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AGC2_NOISE_LEVEL_ESTIMATOR_H_

#include "modules/audio_processing/include/audio_frame_view.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class NoiseLevelEstimator {
 public:
  NoiseLevelEstimator() {}

  // Returns the estimated noise level in dBFS.
  float Analyze(AudioFrameView<const float> frame);

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(NoiseLevelEstimator);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_NOISE_LEVEL_ESTIMATOR_H_
