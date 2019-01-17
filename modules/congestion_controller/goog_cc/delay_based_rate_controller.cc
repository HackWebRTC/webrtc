/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/goog_cc/delay_based_rate_controller.h"

#include <algorithm>
#include <cmath>

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"

namespace webrtc {
namespace {
// Parameters for linear least squares fit of regression line to noisy data.
constexpr size_t kDefaultTrendlineWindowSize = 20;
constexpr double kDefaultTrendlineSmoothingCoeff = 0.9;
constexpr double kDefaultTrendlineThresholdGain = 4.0;

}  // namespace

DelayBasedRateControllerConfig::DelayBasedRateControllerConfig(
    const WebRtcKeyValueConfig* key_value_config)
    : enabled("Enabled"),
      no_ack_backoff_fraction("no_ack_frac", 0.8),
      no_ack_backoff_interval("no_ack_int", TimeDelta::ms(1000)),
      ack_backoff_fraction("ack_dec", 0.90),
      probe_backoff_fraction("probe_dec", 0.85),
      initial_increase_rate("probe_inc", 0.03),
      increase_rate("inc", 0.01),
      first_period_increase_rate("min_step", DataRate::kbps(5)),
      stop_increase_after("stop", TimeDelta::ms(500)),
      min_increase_interval("int", TimeDelta::ms(100)),
      linear_increase_threshold("cut", DataRate::kbps(300)),
      reference_duration_offset("dur_offs", TimeDelta::ms(100)) {
  ParseFieldTrial(
      {&enabled, &no_ack_backoff_fraction, &no_ack_backoff_interval,
       &ack_backoff_fraction, &probe_backoff_fraction, &initial_increase_rate,
       &increase_rate, &stop_increase_after, &min_increase_interval,
       &first_period_increase_rate, &linear_increase_threshold,
       &reference_duration_offset},
      key_value_config->Lookup("WebRTC-Bwe-DelayBasedRateController"));
}
DelayBasedRateControllerConfig::~DelayBasedRateControllerConfig() = default;

DelayBasedRateController::DelayBasedRateController(
    const WebRtcKeyValueConfig* key_value_config,
    RtcEventLog* event_log,
    TargetRateConstraints constraints)
    : conf_(key_value_config),
      event_log_(event_log),
      overuse_detector_(new TrendlineEstimator(kDefaultTrendlineWindowSize,
                                               kDefaultTrendlineSmoothingCoeff,
                                               kDefaultTrendlineThresholdGain)),
      target_rate_(constraints.starting_rate.value()) {
  UpdateConstraints(constraints);
  MaybeLog();
}

DelayBasedRateController::~DelayBasedRateController() = default;

void DelayBasedRateController::OnRouteChange() {
  packet_grouper_.Reset();
  link_capacity_.Reset();
  overuse_detector_.reset(new TrendlineEstimator(
      kDefaultTrendlineWindowSize, kDefaultTrendlineSmoothingCoeff,
      kDefaultTrendlineThresholdGain));
  logged_state_.reset();
}

void DelayBasedRateController::UpdateConstraints(TargetRateConstraints msg) {
  if (msg.min_data_rate)
    min_rate_ = *msg.min_data_rate;
  if (msg.max_data_rate)
    max_rate_ = *msg.max_data_rate;
  if (msg.starting_rate)
    target_rate_ = *msg.starting_rate;
  target_rate_.Clamp(min_rate_, max_rate_);
}

void DelayBasedRateController::SetAcknowledgedRate(DataRate acknowledged_rate) {
  acknowledged_rate_ = acknowledged_rate;
  if (acknowledged_rate > link_capacity_.UpperBound())
    link_capacity_.Reset();
}

void DelayBasedRateController::OnTransportPacketsFeedback(
    TransportPacketsFeedback msg,
    absl::optional<DataRate> probe_bitrate) {
  auto packets = msg.ReceivedWithSendInfo();

  last_rtt_ = msg.feedback_time - packets.back().sent_packet.send_time;
  first_unacked_send_ = msg.first_unacked_send_time;

  for (auto& packet : packets) {
    packet_grouper_.AddPacketInfo(packet, msg.feedback_time);
  }

  for (auto& delta : packet_grouper_.PopDeltas()) {
    overuse_detector_->Update(delta.receive.ms<double>(),
                              delta.send.ms<double>(), delta.receive_time.ms());
  }

  BandwidthUsage usage = overuse_detector_->State();
  Timestamp at_time = msg.feedback_time;
  last_feedback_update_ = at_time;
  if (probe_bitrate) {
    if (!acknowledged_rate_)
      acknowledged_rate_ = *probe_bitrate;
    target_rate_ = *probe_bitrate * conf_.probe_backoff_fraction;
    increase_reference_ = target_rate_;
    link_capacity_.OnProbeRate(*probe_bitrate);
  }

  if (usage == BandwidthUsage::kBwNormal) {
    if (!increasing_state_) {
      increasing_state_ = true;
      // Offset the next increase time by one RTT to avoid increasing too soon
      // after overuse.
      last_increase_update_ = at_time + last_rtt_;
      accumulated_duration_ = 0;
      increase_reference_ = target_rate_;
    }
  } else if (usage == BandwidthUsage::kBwOverusing && !probe_bitrate) {
    increasing_state_ = false;
    if (!acknowledged_rate_ &&
        at_time - last_no_ack_backoff_ >= conf_.no_ack_backoff_interval) {
      // Until we recieve out first acknowledged rate, we back of from the
      // target rate, but pace the backoffs to avoid dropping the rate too fast.
      last_no_ack_backoff_ = at_time;
      target_rate_ = target_rate_ * conf_.no_ack_backoff_fraction;
    } else if (acknowledged_rate_) {
      if (acknowledged_rate_ < link_capacity_.LowerBound())
        link_capacity_.Reset();
      link_capacity_.OnOveruseDetected(*acknowledged_rate_);
      target_rate_ = acknowledged_rate_.value() * conf_.ack_backoff_fraction;
    }
    target_rate_.Clamp(min_rate_, max_rate_);
  }
  MaybeLog();
}

void DelayBasedRateController::OnFeedbackUpdate(
    BandwidthUsage usage,
    absl::optional<DataRate> probe_bitrate,
    Timestamp at_time) {}

void DelayBasedRateController::OnTimeUpdate(Timestamp at_time) {
  if (!increasing_state_ ||
      at_time < last_increase_update_ + conf_.min_increase_interval)
    return;
  TimeDelta time_span = at_time - last_increase_update_;
  last_increase_update_ = at_time;

  if (at_time > last_feedback_update_ + conf_.stop_increase_after)
    return;

  TimeDelta rtt_lower_bound =
      std::max(last_rtt_, at_time - first_unacked_send_);
  TimeDelta reference_span = rtt_lower_bound + conf_.reference_duration_offset;
  accumulated_duration_ += time_span / reference_span;
  if (link_capacity_.has_estimate() &&
      increase_reference_ > conf_.linear_increase_threshold) {
    DataRate linear_increase_rate =
        conf_.increase_rate.Get() * conf_.linear_increase_threshold.Get();
    DataRate increase_amount = accumulated_duration_ * linear_increase_rate;
    target_rate_ = increase_reference_ + increase_amount;
  } else {
    double increase_rate = link_capacity_.has_estimate()
                               ? conf_.initial_increase_rate
                               : conf_.increase_rate;
    double increase_factor = 1 + increase_rate;
    double increase_amount = pow(increase_factor, accumulated_duration_);
    target_rate_ = increase_reference_ * increase_amount;
  }
  target_rate_.Clamp(min_rate_, max_rate_);
  MaybeLog();
}

void DelayBasedRateController::OnRemoteBitrateControl(RemoteBitrateReport msg) {
  target_rate_ = msg.bandwidth;
  increasing_state_ = false;
}

TimeDelta DelayBasedRateController::GetExpectedBandwidthPeriod() const {
  double expected_overuse = 0.05;
  double bandwidth_cycle_max_min_ratio =
      1 / conf_.ack_backoff_fraction + expected_overuse;
  TimeDelta reference_span = last_rtt_ + conf_.reference_duration_offset;
  TimeDelta period = reference_span * log(bandwidth_cycle_max_min_ratio) /
                     log(1 + conf_.increase_rate);
  return period.Clamped(TimeDelta::seconds(1), TimeDelta::seconds(20));
}

DataRate DelayBasedRateController::target_rate() const {
  return target_rate_;
}

bool DelayBasedRateController::in_underuse() const {
  return overuse_detector_->State() == BandwidthUsage::kBwUnderusing;
}

void DelayBasedRateController::MaybeLog() {
  if (event_log_ && (logged_target_ != target_rate_ ||
                     logged_state_ != overuse_detector_->State())) {
    event_log_->Log(absl::make_unique<RtcEventBweUpdateDelayBased>(
        target_rate_.bps(), overuse_detector_->State()));
    logged_state_ = overuse_detector_->State();
    logged_target_ = target_rate_;
  }
}

}  // namespace webrtc
