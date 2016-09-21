/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/audio_network_adaptor/frame_length_controller.h"

#include <iterator>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace webrtc {

FrameLengthController::Config::Config(
    const std::vector<int>& encoder_frame_lengths_ms,
    int initial_frame_length_ms,
    float fl_increasing_packet_loss_fraction,
    float fl_decreasing_packet_loss_fraction,
    int fl_20ms_to_60ms_bandwidth_bps,
    int fl_60ms_to_20ms_bandwidth_bps)
    : encoder_frame_lengths_ms(encoder_frame_lengths_ms),
      initial_frame_length_ms(initial_frame_length_ms),
      fl_increasing_packet_loss_fraction(fl_increasing_packet_loss_fraction),
      fl_decreasing_packet_loss_fraction(fl_decreasing_packet_loss_fraction),
      fl_20ms_to_60ms_bandwidth_bps(fl_20ms_to_60ms_bandwidth_bps),
      fl_60ms_to_20ms_bandwidth_bps(fl_60ms_to_20ms_bandwidth_bps) {}

FrameLengthController::Config::Config(const Config& other) = default;

FrameLengthController::Config::~Config() = default;

FrameLengthController::FrameLengthController(const Config& config)
    : config_(config) {
  // |encoder_frame_lengths_ms| must contain |initial_frame_length_ms|.
  RTC_DCHECK(config_.encoder_frame_lengths_ms.end() !=
             std::find(config_.encoder_frame_lengths_ms.begin(),
                       config_.encoder_frame_lengths_ms.end(),
                       config_.initial_frame_length_ms));

  // We start with assuming that the receiver only accepts
  // |config_.initial_frame_length_ms|.
  run_time_frame_lengths_ms_.push_back(config_.initial_frame_length_ms);
  frame_length_ms_ = run_time_frame_lengths_ms_.begin();

  frame_length_change_criteria_.insert(std::make_pair(
      FrameLengthChange(20, 60), config_.fl_20ms_to_60ms_bandwidth_bps));
  frame_length_change_criteria_.insert(std::make_pair(
      FrameLengthChange(60, 20), config_.fl_60ms_to_20ms_bandwidth_bps));
}

FrameLengthController::~FrameLengthController() = default;

void FrameLengthController::MakeDecision(
    const NetworkMetrics& metrics,
    AudioNetworkAdaptor::EncoderRuntimeConfig* config) {
  // Decision on |frame_length_ms| should not have been made.
  RTC_DCHECK(!config->frame_length_ms);

  if (FrameLengthIncreasingDecision(metrics, *config)) {
    ++frame_length_ms_;
  } else if (FrameLengthDecreasingDecision(metrics, *config)) {
    --frame_length_ms_;
  }
  config->frame_length_ms = rtc::Optional<int>(*frame_length_ms_);
}

void FrameLengthController::SetConstraints(const Constraints& constraints) {
  if (constraints.receiver_frame_length_range) {
    SetReceiverFrameLengthRange(
        constraints.receiver_frame_length_range->min_frame_length_ms,
        constraints.receiver_frame_length_range->max_frame_length_ms);
  }
}

FrameLengthController::FrameLengthChange::FrameLengthChange(
    int from_frame_length_ms,
    int to_frame_length_ms)
    : from_frame_length_ms(from_frame_length_ms),
      to_frame_length_ms(to_frame_length_ms) {}

FrameLengthController::FrameLengthChange::~FrameLengthChange() = default;

bool FrameLengthController::FrameLengthChange::operator<(
    const FrameLengthChange& rhs) const {
  return from_frame_length_ms < rhs.from_frame_length_ms ||
         (from_frame_length_ms == rhs.from_frame_length_ms &&
          to_frame_length_ms < rhs.to_frame_length_ms);
}

void FrameLengthController::SetReceiverFrameLengthRange(
    int min_frame_length_ms,
    int max_frame_length_ms) {
  int frame_length_ms = *frame_length_ms_;
  // Reset |run_time_frame_lengths_ms_| by filtering |config_.frame_lengths_ms|
  // with the receiver frame length range.
  run_time_frame_lengths_ms_.clear();
  std::copy_if(config_.encoder_frame_lengths_ms.begin(),
               config_.encoder_frame_lengths_ms.end(),
               std::back_inserter(run_time_frame_lengths_ms_),
               [&](int frame_length_ms) {
                 return frame_length_ms >= min_frame_length_ms &&
                        frame_length_ms <= max_frame_length_ms;
               });
  RTC_DCHECK(std::is_sorted(run_time_frame_lengths_ms_.begin(),
                            run_time_frame_lengths_ms_.end()));

  // Keep the current frame length. If it has gone out of the new range, use
  // the smallest available frame length.
  frame_length_ms_ =
      std::find(run_time_frame_lengths_ms_.begin(),
                run_time_frame_lengths_ms_.end(), frame_length_ms);
  if (frame_length_ms_ == run_time_frame_lengths_ms_.end()) {
    LOG(LS_WARNING)
        << "Actual frame length not in frame length range of the receiver";
    frame_length_ms_ = run_time_frame_lengths_ms_.begin();
  }
}

bool FrameLengthController::FrameLengthIncreasingDecision(
    const NetworkMetrics& metrics,
    const AudioNetworkAdaptor::EncoderRuntimeConfig& config) const {
  // Increase frame length if
  // 1. longer frame length is available AND
  // 2. |uplink_bandwidth_bps| is known to be smaller than a threshold AND
  // 3. |uplink_packet_loss_fraction| is known to be smaller than a threshold
  //    AND
  // 4. FEC is not decided or is OFF.
  auto longer_frame_length_ms = std::next(frame_length_ms_);
  if (longer_frame_length_ms == run_time_frame_lengths_ms_.end())
    return false;

  auto increase_threshold = frame_length_change_criteria_.find(
      FrameLengthChange(*frame_length_ms_, *longer_frame_length_ms));

  if (increase_threshold == frame_length_change_criteria_.end())
    return false;

  return (metrics.uplink_bandwidth_bps &&
          *metrics.uplink_bandwidth_bps <= increase_threshold->second) &&
         (metrics.uplink_packet_loss_fraction &&
          *metrics.uplink_packet_loss_fraction <=
              config_.fl_increasing_packet_loss_fraction) &&
         !config.enable_fec.value_or(false);
}

bool FrameLengthController::FrameLengthDecreasingDecision(
    const NetworkMetrics& metrics,
    const AudioNetworkAdaptor::EncoderRuntimeConfig& config) const {
  // Decrease frame length if
  // 1. shorter frame length is available AND one or more of the followings:
  // 2. |uplink_bandwidth_bps| is known to be larger than a threshold,
  // 3. |uplink_packet_loss_fraction| is known to be larger than a threshold,
  // 4. FEC is decided ON.
  if (frame_length_ms_ == run_time_frame_lengths_ms_.begin())
    return false;

  auto shorter_frame_length_ms = std::prev(frame_length_ms_);
  auto decrease_threshold = frame_length_change_criteria_.find(
      FrameLengthChange(*frame_length_ms_, *shorter_frame_length_ms));

  if (decrease_threshold == frame_length_change_criteria_.end())
    return false;

  return (metrics.uplink_bandwidth_bps &&
          *metrics.uplink_bandwidth_bps >= decrease_threshold->second) ||
         (metrics.uplink_packet_loss_fraction &&
          *metrics.uplink_packet_loss_fraction >=
              config_.fl_decreasing_packet_loss_fraction) ||
         config.enable_fec.value_or(false);
}

}  // namespace webrtc
