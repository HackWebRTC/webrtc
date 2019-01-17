/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_RATE_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_RATE_CONTROLLER_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/transport/network_types.h"
#include "api/transport/webrtc_key_value_config.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/congestion_controller/goog_cc/link_capacity_estimator.h"
#include "modules/congestion_controller/goog_cc/packet_grouping.h"
#include "modules/congestion_controller/goog_cc/trendline_estimator.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"

namespace webrtc {
struct DelayBasedRateControllerConfig {
  FieldTrialFlag enabled;
  FieldTrialParameter<double> no_ack_backoff_fraction;
  FieldTrialParameter<TimeDelta> no_ack_backoff_interval;
  FieldTrialParameter<double> ack_backoff_fraction;
  FieldTrialParameter<double> probe_backoff_fraction;
  FieldTrialParameter<double> initial_increase_rate;
  FieldTrialParameter<double> increase_rate;
  FieldTrialParameter<DataRate> first_period_increase_rate;
  FieldTrialParameter<TimeDelta> stop_increase_after;
  FieldTrialParameter<TimeDelta> min_increase_interval;
  FieldTrialParameter<DataRate> linear_increase_threshold;
  FieldTrialParameter<TimeDelta> reference_duration_offset;
  explicit DelayBasedRateControllerConfig(
      const WebRtcKeyValueConfig* key_value_config);
  ~DelayBasedRateControllerConfig();
};

// Rate controller for GoogCC, increases the target rate based on a
// fixed increase interval and an RTT dependent increase rate.
class DelayBasedRateController {
 public:
  DelayBasedRateController(const WebRtcKeyValueConfig* key_value_config,
                           RtcEventLog* event_log,
                           TargetRateConstraints constraints);
  ~DelayBasedRateController();
  void OnRouteChange();
  void UpdateConstraints(TargetRateConstraints constraints);
  void SetAcknowledgedRate(DataRate acknowledged_rate);
  void OnTransportPacketsFeedback(TransportPacketsFeedback msg,
                                  absl::optional<DataRate> probe_bitrate);
  void OnTimeUpdate(Timestamp at_time);
  void OnRemoteBitrateControl(RemoteBitrateReport msg);
  TimeDelta GetExpectedBandwidthPeriod() const;

  bool Enabled() const { return conf_.enabled; }
  DataRate target_rate() const;
  bool in_underuse() const;

 private:
  enum class ControllerSate { kHold, kExponentialIncrease, kLinearIncrease };
  friend class GoogCcStatePrinter;
  void OnFeedbackUpdate(BandwidthUsage usage,
                        absl::optional<DataRate> probe_bitrate,
                        Timestamp at_time);
  void MaybeLog();
  const DelayBasedRateControllerConfig conf_;
  RtcEventLog* event_log_;

  PacketDelayGrouper packet_grouper_;
  std::unique_ptr<TrendlineEstimator> overuse_detector_;
  LinkCapacityEstimator link_capacity_;

  DataRate min_rate_ = DataRate::Zero();
  DataRate max_rate_ = DataRate::Infinity();

  absl::optional<DataRate> acknowledged_rate_;
  TimeDelta last_rtt_ = TimeDelta::seconds(1);
  Timestamp first_unacked_send_ = Timestamp::PlusInfinity();
  Timestamp last_feedback_update_ = Timestamp::MinusInfinity();

  DataRate target_rate_;

  Timestamp last_no_ack_backoff_ = Timestamp::MinusInfinity();
  bool increasing_state_ = false;
  double accumulated_duration_ = 0;
  Timestamp last_increase_update_ = Timestamp::PlusInfinity();
  DataRate increase_reference_ = DataRate::PlusInfinity();

  absl::optional<BandwidthUsage> logged_state_;
  DataRate logged_target_ = DataRate::PlusInfinity();
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_RATE_CONTROLLER_H_
