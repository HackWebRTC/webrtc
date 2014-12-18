/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/bitrate_controller/remb_suppressor.h"

#include <math.h>

#include "webrtc/system_wrappers/interface/field_trial.h"

namespace webrtc {

// If BWE falls more than this fraction from one REMB to the next,
// classify this as a glitch.
static const double kMaxBweDropRatio = 0.45;

// If we are sending less then this fraction of the last REMB when a glitch
// is detected, start suppressing REMB.
static const double kMinSendBitrateFraction = 0.75;

// Minimum fractional BWE growth per second needed to keep suppressing.
static const double kMinGrowth = 0.015;

RembSuppressor::RembSuppressor(Clock* clock)
    : enabled_(false),
      clock_(clock),
      last_remb_bps_(0),
      bitrate_sent_bps_(0),
      last_remb_ignored_bps_(0),
      last_remb_ignore_time_ms_(0),
      remb_silence_start_(0) {
}

RembSuppressor::~RembSuppressor() {
}

bool RembSuppressor::SuppresNewRemb(uint32_t bitrate_bps) {
  if (!Enabled())
    return false;

  if (remb_silence_start_ == 0) {
    // Not currently suppressing. Check if there is a bit rate drop
    // significant enough to warrant suppression.
    return StartSuppressing(bitrate_bps);
  }

  // Check if signs point to recovery, otherwise back off suppression.
  if (!ContinueSuppressing(bitrate_bps)) {
    remb_silence_start_ = 0;
    last_remb_ignored_bps_ = 0;
    last_remb_ignore_time_ms_ = 0;
    return false;
  }
  return true;
}

bool RembSuppressor::StartSuppressing(uint32_t bitrate_bps) {
  if (bitrate_bps <
      static_cast<uint32_t>(last_remb_bps_ * kMaxBweDropRatio + 0.5)) {
    if (bitrate_sent_bps_ <
        static_cast<uint32_t>(last_remb_bps_ * kMinSendBitrateFraction + 0.5)) {
      int64_t now = clock_->TimeInMilliseconds();
      remb_silence_start_ = now;
      last_remb_ignore_time_ms_ = now;
      last_remb_ignored_bps_ = bitrate_bps;
      return true;
    }
  }
  last_remb_bps_ = bitrate_bps;
  return false;
}

bool RembSuppressor::ContinueSuppressing(uint32_t bitrate_bps) {
  int64_t now = clock_->TimeInMilliseconds();

  if (bitrate_bps >= last_remb_bps_) {
    // We have fully recovered, stop suppressing!
    return false;
  }

  // If exactly the same REMB, we probably don't have a new estimate. Keep on
  // suppressing. However, if REMB is going down or just not increasing fast
  // enough (kMinGrowth = 0.015 => REMB should increase by at least 1.5% / s)
  // it looks like the link capacity has actually deteriorated and we are
  // currently over-utilizing; back off.
  if (bitrate_bps != last_remb_ignored_bps_) {
    double delta_t = (now - last_remb_ignore_time_ms_) / 1000.0;
    double min_increase = pow(1.0 + kMinGrowth, delta_t);
    if (bitrate_bps < last_remb_ignored_bps_ * min_increase) {
      return false;
    }
  }

  last_remb_ignored_bps_ = bitrate_bps;
  last_remb_ignore_time_ms_ = now;

  return true;
}

void RembSuppressor::SetBitrateSent(uint32_t bitrate_bps) {
  bitrate_sent_bps_ = bitrate_bps;
}

bool RembSuppressor::Enabled() {
  return enabled_;
}

void RembSuppressor::SetEnabled(bool enabled) {
  enabled_ = enabled &&
             webrtc::field_trial::FindFullName(
                 "WebRTC-ConditionalRembSuppression") == "Enabled";
}

}  // namespace webrtc
