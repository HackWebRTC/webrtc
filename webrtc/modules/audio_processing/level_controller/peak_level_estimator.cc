/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/level_controller/peak_level_estimator.h"

#include <algorithm>

#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/level_controller/lc_constants.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

PeakLevelEstimator::PeakLevelEstimator() {
  Initialize();
}

PeakLevelEstimator::~PeakLevelEstimator() {}

void PeakLevelEstimator::Initialize() {
  peak_level_ = kTargetLcPeakLevel;
  hold_counter_ = 0;
  initialization_phase_ = true;
}

float PeakLevelEstimator::Analyze(SignalClassifier::SignalType signal_type,
                                  float frame_peak_level) {
  if (frame_peak_level == 0) {
    RTC_DCHECK_LE(30.f, peak_level_);
    return peak_level_;
  }

  if (peak_level_ < frame_peak_level) {
    // Smoothly update the estimate upwards when the frame peak level is
    // higher than the estimate.
    peak_level_ += 0.1f * (frame_peak_level - peak_level_);
    hold_counter_ = 100;
    initialization_phase_ = false;
  } else {
    hold_counter_ = std::max(0, hold_counter_ - 1);

    // When the signal is highly non-stationary, update the estimate slowly
    // downwards if the estimate is lower than the frame peak level.
    if ((signal_type == SignalClassifier::SignalType::kHighlyNonStationary &&
         hold_counter_ == 0) ||
        initialization_phase_) {
      peak_level_ =
          std::max(peak_level_ + 0.01f * (frame_peak_level - peak_level_),
                   peak_level_ * 0.995f);
    }
  }

  peak_level_ = std::max(peak_level_, 30.f);

  return peak_level_;
}

}  // namespace webrtc
