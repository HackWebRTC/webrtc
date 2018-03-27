/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_mode_level_estimator.h"

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

AdaptiveModeLevelEstimator::AdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper) {}

void AdaptiveModeLevelEstimator::UpdateEstimation(
    const VadWithLevel::LevelAndProbability& vad_data) {
  RTC_DCHECK_GT(vad_data.speech_rms_dbfs, -150.f);
  RTC_DCHECK_LT(vad_data.speech_rms_dbfs, 50.f);
  RTC_DCHECK_GT(vad_data.speech_peak_dbfs, -150.f);
  RTC_DCHECK_LT(vad_data.speech_peak_dbfs, 50.f);
  RTC_DCHECK_GE(vad_data.speech_probability, 0.f);
  RTC_DCHECK_LE(vad_data.speech_probability, 1.f);
}

float AdaptiveModeLevelEstimator::LatestLevelEstimate() const {
  // TODO(webrtc:7494): This is a stub. Add implementation.
  return 0.f;
}
}  // namespace webrtc
