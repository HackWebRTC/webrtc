/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/overuse_estimator.h"

#include <algorithm>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "webrtc/base/checks.h"
#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/system_wrappers/include/logging.h"

namespace webrtc {

enum { kMinFramePeriodHistoryLength = 60 };
enum { kDeltaCounterMax = 1000 };

OveruseEstimator::OveruseEstimator()
    : num_of_deltas_(0),
      offset_(0),
      prev_offset_(offset_),
      e_(0.1),
      process_noise_(1e-2),
      avg_noise_(0),
      var_noise_(50),
      send_delta_history_() {}

OveruseEstimator::~OveruseEstimator() {
  send_delta_history_.clear();
}

void OveruseEstimator::Update(double recv_delta_ms,
                              double send_delta_ms,
                              BandwidthUsage current_hypothesis) {
  const double min_frame_period = UpdateMinFramePeriod(send_delta_ms);
  const double delta_ms = recv_delta_ms - send_delta_ms;

  ++num_of_deltas_;
  if (num_of_deltas_ > kDeltaCounterMax) {
    num_of_deltas_ = kDeltaCounterMax;
  }

  // Update the Kalman filter.
  e_ += process_noise_;

  if ((current_hypothesis == kBwOverusing && offset_ < prev_offset_) ||
      (current_hypothesis == kBwUnderusing && offset_ > prev_offset_)) {
    e_ += 10 * process_noise_;
  }

  const double residual = delta_ms - offset_;

  const bool in_stable_state = (current_hypothesis == kBwNormal);
  const double max_residual = 3.0 * sqrt(var_noise_);
  // We try to filter out very late frames. For instance periodic key
  // frames doesn't fit the Gaussian model well.
  if (fabs(residual) < max_residual) {
    UpdateNoiseEstimate(residual, min_frame_period, in_stable_state);
  } else {
    UpdateNoiseEstimate(residual < 0 ? -max_residual : max_residual,
                        min_frame_period, in_stable_state);
  }
  const double k = e_ / (var_noise_ + e_);

  // Update state.
  e_ = e_ * (1.0 - k);

  // The covariance matrix must be positive.
  RTC_DCHECK(e_ >= 0.0);
  if (e_ < 0)
    LOG(LS_ERROR) << "The over-use estimator's covariance is negative!";

  offset_ = offset_ + k * residual;
}

double OveruseEstimator::UpdateMinFramePeriod(double send_delta_ms) {
  double min_frame_period = send_delta_ms;
  if (send_delta_history_.size() >= kMinFramePeriodHistoryLength) {
    send_delta_history_.pop_front();
  }
  for (double delta_ms : send_delta_history_) {
    min_frame_period = std::min(delta_ms, min_frame_period);
  }
  send_delta_history_.push_back(send_delta_ms);
  return min_frame_period;
}

void OveruseEstimator::UpdateNoiseEstimate(double residual,
                                           double send_delta_ms,
                                           bool stable_state) {
  if (!stable_state) {
    return;
  }
  // Faster filter during startup to faster adapt to the jitter level
  // of the network. |alpha| is tuned for 30 frames per second, but is scaled
  // according to |send_delta_ms|.
  double alpha = 0.01;
  if (num_of_deltas_ > 10*30) {
    alpha = 0.002;
  }
  // Only update the noise estimate if we're not over-using. |beta| is a
  // function of alpha and the time delta since the previous update.
  const double beta = pow(1 - alpha, send_delta_ms * 30.0 / 1000.0);
  avg_noise_ = beta * avg_noise_
              + (1 - beta) * residual;
  var_noise_ = beta * var_noise_
              + (1 - beta) * (avg_noise_ - residual) * (avg_noise_ - residual);
  if (var_noise_ < 1) {
    var_noise_ = 1;
  }
}
}  // namespace webrtc
