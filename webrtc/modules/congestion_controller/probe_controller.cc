/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/probe_controller.h"

#include <initializer_list>

#include "webrtc/base/logging.h"

namespace webrtc {

namespace {

// Number of deltas between probes per cluster. On the very first cluster,
// we will need kProbeDeltasPerCluster + 1 probes, but on a cluster following
// another, we need kProbeDeltasPerCluster probes.
constexpr int kProbeDeltasPerCluster = 5;

// Maximum waiting time from the time of initiating probing to getting
// the measured results back.
constexpr int64_t kMaxWaitingTimeForProbingResultMs = 1000;

// Value of |min_bitrate_to_probe_further_bps_| that indicates
// further probing is disabled.
constexpr int kExponentialProbingDisabled = 0;

}  // namespace

ProbeController::ProbeController(PacedSender* pacer, Clock* clock)
    : pacer_(pacer),
      clock_(clock),
      state_(State::kInit),
      min_bitrate_to_probe_further_bps_(kExponentialProbingDisabled),
      time_last_probing_initiated_ms_(0),
      estimated_bitrate_bps_(0),
      max_bitrate_bps_(0) {}

void ProbeController::SetBitrates(int min_bitrate_bps,
                                  int start_bitrate_bps,
                                  int max_bitrate_bps) {
  rtc::CritScope cs(&critsect_);
  if (state_ == State::kInit) {
    // When probing at 1.8 Mbps ( 6x 300), this represents a threshold of
    // 1.2 Mbps to continue probing.
    InitiateProbing({3 * start_bitrate_bps, 6 * start_bitrate_bps},
                    4 * start_bitrate_bps);
  }

  // Only do probing if:
  //   we are mid-call, which we consider to be if
  //     exponential probing is not active and
  //     |estimated_bitrate_bps_| is valid (> 0) and
  //     the current bitrate is lower than the new |max_bitrate_bps|, and
  //     we actually want to increase the |max_bitrate_bps_|.
  if (state_ != State::kWaitingForProbingResult &&
      estimated_bitrate_bps_ != 0 && estimated_bitrate_bps_ < max_bitrate_bps &&
      max_bitrate_bps > max_bitrate_bps_) {
    InitiateProbing({max_bitrate_bps}, kExponentialProbingDisabled);
  }
  max_bitrate_bps_ = max_bitrate_bps;
}

void ProbeController::SetEstimatedBitrate(int bitrate_bps) {
  rtc::CritScope cs(&critsect_);
  if (state_ == State::kWaitingForProbingResult) {
    if ((clock_->TimeInMilliseconds() - time_last_probing_initiated_ms_) >
        kMaxWaitingTimeForProbingResultMs) {
      LOG(LS_INFO) << "kWaitingForProbingResult: timeout";
      state_ = State::kProbingComplete;
      min_bitrate_to_probe_further_bps_ = kExponentialProbingDisabled;
    } else {
      // Continue probing if probing results indicate channel has greater
      // capacity.
      LOG(LS_INFO) << "Measured bitrate: " << bitrate_bps
                   << " Minimum to probe further: "
                   << min_bitrate_to_probe_further_bps_;
      if (min_bitrate_to_probe_further_bps_ != kExponentialProbingDisabled &&
          bitrate_bps > min_bitrate_to_probe_further_bps_) {
        // Double the probing bitrate and expect a minimum of 25% gain to
        // continue probing.
        InitiateProbing({2 * bitrate_bps}, 1.25 * bitrate_bps);
      }
    }
  }
  estimated_bitrate_bps_ = bitrate_bps;
}

void ProbeController::InitiateProbing(
    std::initializer_list<int> bitrates_to_probe,
    int min_bitrate_to_probe_further_bps) {
  bool first_cluster = true;
  for (int bitrate : bitrates_to_probe) {
    if (first_cluster) {
      pacer_->CreateProbeCluster(bitrate, kProbeDeltasPerCluster + 1);
      first_cluster = false;
    } else {
      pacer_->CreateProbeCluster(bitrate, kProbeDeltasPerCluster);
    }
  }
  min_bitrate_to_probe_further_bps_ = min_bitrate_to_probe_further_bps;
  time_last_probing_initiated_ms_ = clock_->TimeInMilliseconds();
  if (min_bitrate_to_probe_further_bps == kExponentialProbingDisabled)
    state_ = State::kProbingComplete;
  else
    state_ = State::kWaitingForProbingResult;
}

}  // namespace webrtc
