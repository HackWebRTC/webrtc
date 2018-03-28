/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/saturation_protector.h"

#include <algorithm>

#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

SaturationProtector::SaturationProtector(ApmDataDumper* apm_data_dumper) {}

void SaturationProtector::UpdateMargin(
    const VadWithLevel::LevelAndProbability& vad_data,
    float last_speech_level_estimate) {}

float SaturationProtector::LastMargin() const {
  return kInitialSaturationMarginDb;
}
}  // namespace webrtc
