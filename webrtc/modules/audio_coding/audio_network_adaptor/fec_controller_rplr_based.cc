/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/audio_network_adaptor/fec_controller_rplr_based.h"

#include <limits>
#include <utility>

#include "webrtc/base/checks.h"

namespace webrtc {

FecControllerRplrBased::Config::Threshold::Threshold(
    int low_bandwidth_bps,
    float low_bandwidth_recoverable_packet_loss,
    int high_bandwidth_bps,
    float high_bandwidth_recoverable_packet_loss)
    : low_bandwidth_bps(low_bandwidth_bps),
      low_bandwidth_recoverable_packet_loss(
          low_bandwidth_recoverable_packet_loss),
      high_bandwidth_bps(high_bandwidth_bps),
      high_bandwidth_recoverable_packet_loss(
          high_bandwidth_recoverable_packet_loss) {}

FecControllerRplrBased::Config::Config(bool initial_fec_enabled,
                                       const Threshold& fec_enabling_threshold,
                                       const Threshold& fec_disabling_threshold,
                                       int time_constant_ms,
                                       const Clock* clock)
    : initial_fec_enabled(initial_fec_enabled),
      fec_enabling_threshold(fec_enabling_threshold),
      fec_disabling_threshold(fec_disabling_threshold),
      time_constant_ms(time_constant_ms),
      clock(clock) {}

FecControllerRplrBased::FecControllerRplrBased(const Config& config)
    : config_(config),
      fec_enabled_(config.initial_fec_enabled),
      fec_enabling_threshold_info_(config_.fec_enabling_threshold),
      fec_disabling_threshold_info_(config_.fec_disabling_threshold) {
  RTC_DCHECK_LE(fec_enabling_threshold_info_.slope, 0);
  RTC_DCHECK_LE(fec_enabling_threshold_info_.slope, 0);
  RTC_DCHECK_LE(
      GetPacketLossThreshold(config_.fec_enabling_threshold.low_bandwidth_bps,
                             config_.fec_disabling_threshold,
                             fec_disabling_threshold_info_),
      config_.fec_enabling_threshold.low_bandwidth_recoverable_packet_loss);
  RTC_DCHECK_LE(
      GetPacketLossThreshold(config_.fec_enabling_threshold.high_bandwidth_bps,
                             config_.fec_disabling_threshold,
                             fec_disabling_threshold_info_),
      config_.fec_enabling_threshold.high_bandwidth_recoverable_packet_loss);
}

FecControllerRplrBased::~FecControllerRplrBased() = default;

void FecControllerRplrBased::UpdateNetworkMetrics(
    const NetworkMetrics& network_metrics) {
  if (network_metrics.uplink_bandwidth_bps)
    uplink_bandwidth_bps_ = network_metrics.uplink_bandwidth_bps;
  if (network_metrics.uplink_recoverable_packet_loss_fraction) {
    uplink_recoverable_packet_loss_ =
        network_metrics.uplink_recoverable_packet_loss_fraction;
  }
}

void FecControllerRplrBased::MakeDecision(
    AudioNetworkAdaptor::EncoderRuntimeConfig* config) {
  RTC_DCHECK(!config->enable_fec);
  RTC_DCHECK(!config->uplink_packet_loss_fraction);

  fec_enabled_ = fec_enabled_ ? !FecDisablingDecision() : FecEnablingDecision();

  config->enable_fec = rtc::Optional<bool>(fec_enabled_);
  config->uplink_packet_loss_fraction = rtc::Optional<float>(
      uplink_recoverable_packet_loss_ ? *uplink_recoverable_packet_loss_ : 0.0);
}

FecControllerRplrBased::ThresholdInfo::ThresholdInfo(
    const Config::Threshold& threshold) {
  int bandwidth_diff_bps =
      threshold.high_bandwidth_bps - threshold.low_bandwidth_bps;
  float recoverable_packet_loss_diff =
      threshold.high_bandwidth_recoverable_packet_loss -
      threshold.low_bandwidth_recoverable_packet_loss;
  slope = bandwidth_diff_bps == 0
              ? 0.0
              : recoverable_packet_loss_diff / bandwidth_diff_bps;
  offset = threshold.low_bandwidth_recoverable_packet_loss -
           slope * threshold.low_bandwidth_bps;
}

float FecControllerRplrBased::GetPacketLossThreshold(
    int bandwidth_bps,
    const Config::Threshold& threshold,
    const ThresholdInfo& threshold_info) const {
  if (bandwidth_bps < threshold.low_bandwidth_bps) {
    return std::numeric_limits<float>::max();
  } else if (bandwidth_bps >= threshold.high_bandwidth_bps) {
    return threshold.high_bandwidth_recoverable_packet_loss;
  } else {
    float rc = threshold_info.offset + threshold_info.slope * bandwidth_bps;
    RTC_DCHECK_LE(rc, threshold.low_bandwidth_recoverable_packet_loss);
    RTC_DCHECK_GE(rc, threshold.high_bandwidth_recoverable_packet_loss);
    return rc;
  }
}

bool FecControllerRplrBased::FecEnablingDecision() const {
  if (!uplink_bandwidth_bps_ || !uplink_recoverable_packet_loss_) {
    return false;
  } else {
    return *uplink_recoverable_packet_loss_ >=
           GetPacketLossThreshold(*uplink_bandwidth_bps_,
                                  config_.fec_enabling_threshold,
                                  fec_enabling_threshold_info_);
  }
}

bool FecControllerRplrBased::FecDisablingDecision() const {
  if (!uplink_bandwidth_bps_ || !uplink_recoverable_packet_loss_) {
    return false;
  } else {
    return *uplink_recoverable_packet_loss_ <=
           GetPacketLossThreshold(*uplink_bandwidth_bps_,
                                  config_.fec_disabling_threshold,
                                  fec_disabling_threshold_info_);
  }
}

}  // namespace webrtc
