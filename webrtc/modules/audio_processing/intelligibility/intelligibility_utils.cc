/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/intelligibility/intelligibility_utils.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

namespace webrtc {

namespace intelligibility {

namespace {

// Return |current| changed towards |target|, with the change being at most
// |limit|.
float UpdateFactor(float target, float current, float limit) {
  float delta = fabsf(target - current);
  float sign = copysign(1.f, target - current);
  return current + sign * fminf(delta, limit);
}

}  // namespace

PowerEstimator::PowerEstimator(size_t num_freqs,
                               float decay)
    : magnitude_(new float[num_freqs]()),
      power_(new float[num_freqs]()),
      num_freqs_(num_freqs),
      decay_(decay) {
  memset(magnitude_.get(), 0, sizeof(*magnitude_.get()) * num_freqs_);
  memset(power_.get(), 0, sizeof(*power_.get()) * num_freqs_);
}

// Compute the magnitude from the beginning, with exponential decaying of the
// series data.
void PowerEstimator::Step(const std::complex<float>* data) {
  for (size_t i = 0; i < num_freqs_; ++i) {
    magnitude_[i] = decay_ * magnitude_[i] +
                (1.f - decay_) * std::abs(data[i]);
  }
}

const float* PowerEstimator::Power() {
  for (size_t i = 0; i < num_freqs_; ++i) {
    power_[i] = magnitude_[i] * magnitude_[i];
  }
  return &power_[0];
}

GainApplier::GainApplier(size_t freqs, float change_limit)
    : num_freqs_(freqs),
      change_limit_(change_limit),
      target_(new float[freqs]()),
      current_(new float[freqs]()) {
  for (size_t i = 0; i < freqs; ++i) {
    target_[i] = 1.f;
    current_[i] = 1.f;
  }
}

void GainApplier::Apply(const std::complex<float>* in_block,
                        std::complex<float>* out_block) {
  for (size_t i = 0; i < num_freqs_; ++i) {
    float factor = sqrtf(fabsf(current_[i]));
    if (!std::isnormal(factor)) {
      factor = 1.f;
    }
    out_block[i] = factor * in_block[i];
    current_[i] = UpdateFactor(target_[i], current_[i], change_limit_);
  }
}

}  // namespace intelligibility

}  // namespace webrtc
