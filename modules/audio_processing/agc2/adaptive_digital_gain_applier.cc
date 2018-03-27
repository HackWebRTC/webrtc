/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_digital_gain_applier.h"

#include <algorithm>

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

AdaptiveDigitalGainApplier::AdaptiveDigitalGainApplier(
    ApmDataDumper* apm_data_dumper)
    : apm_data_dumper_(apm_data_dumper) {}

void AdaptiveDigitalGainApplier::Process(
    float input_level_dbfs,
    float input_noise_level_dbfs,
    rtc::ArrayView<const VadWithLevel::LevelAndProbability> vad_results,
    AudioFrameView<float> float_frame) {
  RTC_DCHECK_GE(input_level_dbfs, -150.f);
  RTC_DCHECK_LE(input_level_dbfs, 0.f);
  RTC_DCHECK_GE(float_frame.num_channels(), 1);
  RTC_DCHECK_GE(float_frame.samples_per_channel(), 1);

  // TODO(webrtc:8925): compute and apply the gain.

  last_gain_db_ = 1.f;
  apm_data_dumper_->DumpRaw("agc2_applied_gain_db", last_gain_db_);
}
}  // namespace webrtc
