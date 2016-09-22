/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/audio_network_adaptor/controller_manager.h"

#include <cmath>
#include <utility>

#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

ControllerManagerImpl::Config::Config(int min_reordering_time_ms,
                                      float min_reordering_squared_distance,
                                      const Clock* clock)
    : min_reordering_time_ms(min_reordering_time_ms),
      min_reordering_squared_distance(min_reordering_squared_distance),
      clock(clock) {}

ControllerManagerImpl::Config::~Config() = default;

ControllerManagerImpl::ControllerManagerImpl(const Config& config)
    : ControllerManagerImpl(
          config,
          std::vector<std::unique_ptr<Controller>>(),
          std::map<const Controller*, std::pair<int, float>>()) {}

ControllerManagerImpl::ControllerManagerImpl(
    const Config& config,
    std::vector<std::unique_ptr<Controller>>&& controllers,
    const std::map<const Controller*, std::pair<int, float>>&
        chracteristic_points)
    : config_(config),
      controllers_(std::move(controllers)),
      last_reordering_time_ms_(rtc::Optional<int64_t>()),
      last_scoring_point_(0, 0.0) {
  for (auto& controller : controllers_)
    default_sorted_controllers_.push_back(controller.get());
  sorted_controllers_ = default_sorted_controllers_;
  for (auto& controller_point : chracteristic_points) {
    controller_scoring_points_.insert(std::make_pair(
        controller_point.first, ScoringPoint(controller_point.second.first,
                                             controller_point.second.second)));
  }
}

ControllerManagerImpl::~ControllerManagerImpl() = default;

std::vector<Controller*> ControllerManagerImpl::GetSortedControllers(
    const Controller::NetworkMetrics& metrics) {
  int64_t now_ms = config_.clock->TimeInMilliseconds();

  if (!metrics.uplink_bandwidth_bps || !metrics.uplink_packet_loss_fraction)
    return sorted_controllers_;

  if (last_reordering_time_ms_ &&
      now_ms - *last_reordering_time_ms_ < config_.min_reordering_time_ms)
    return sorted_controllers_;

  ScoringPoint scoring_point(*metrics.uplink_bandwidth_bps,
                             *metrics.uplink_packet_loss_fraction);

  if (last_reordering_time_ms_ &&
      last_scoring_point_.SquaredDistanceTo(scoring_point) <
          config_.min_reordering_squared_distance)
    return sorted_controllers_;

  // Sort controllers according to the distances of |scoring_point| to the
  // characteristic scoring points of controllers.
  //
  // A controller that does not associate with any scoring point
  // are treated as if
  // 1) they are less important than any controller that has a scoring point,
  // 2) they are equally important to any controller that has no scoring point,
  //    and their relative order will follow |default_sorted_controllers_|.
  std::vector<Controller*> sorted_controllers(default_sorted_controllers_);
  std::stable_sort(
      sorted_controllers.begin(), sorted_controllers.end(),
      [this, &scoring_point](const Controller* lhs, const Controller* rhs) {
        auto lhs_scoring_point = controller_scoring_points_.find(lhs);
        auto rhs_scoring_point = controller_scoring_points_.find(rhs);

        if (lhs_scoring_point == controller_scoring_points_.end())
          return false;

        if (rhs_scoring_point == controller_scoring_points_.end())
          return true;

        return lhs_scoring_point->second.SquaredDistanceTo(scoring_point) <
               rhs_scoring_point->second.SquaredDistanceTo(scoring_point);
      });

  if (sorted_controllers_ != sorted_controllers) {
    sorted_controllers_ = sorted_controllers;
    last_reordering_time_ms_ = rtc::Optional<int64_t>(now_ms);
    last_scoring_point_ = scoring_point;
  }
  return sorted_controllers_;
}

std::vector<Controller*> ControllerManagerImpl::GetControllers() const {
  return default_sorted_controllers_;
}

ControllerManagerImpl::ScoringPoint::ScoringPoint(
    int uplink_bandwidth_bps,
    float uplink_packet_loss_fraction)
    : uplink_bandwidth_bps(uplink_bandwidth_bps),
      uplink_packet_loss_fraction(uplink_packet_loss_fraction) {}

namespace {

constexpr int kMinUplinkBandwidthBps = 0;
constexpr int kMaxUplinkBandwidthBps = 120000;

float NormalizeUplinkBandwidth(int uplink_bandwidth_bps) {
  uplink_bandwidth_bps =
      std::min(kMaxUplinkBandwidthBps,
               std::max(kMinUplinkBandwidthBps, uplink_bandwidth_bps));
  return static_cast<float>(uplink_bandwidth_bps - kMinUplinkBandwidthBps) /
         (kMaxUplinkBandwidthBps - kMinUplinkBandwidthBps);
}

float NormalizePacketLossFraction(float uplink_packet_loss_fraction) {
  // |uplink_packet_loss_fraction| is seldom larger than 0.3, so we scale it up
  // by 3.3333f.
  return std::min(uplink_packet_loss_fraction * 3.3333f, 1.0f);
}

}  // namespace

float ControllerManagerImpl::ScoringPoint::SquaredDistanceTo(
    const ScoringPoint& scoring_point) const {
  float diff_normalized_bitrate_bps =
      NormalizeUplinkBandwidth(scoring_point.uplink_bandwidth_bps) -
      NormalizeUplinkBandwidth(uplink_bandwidth_bps);
  float diff_normalized_packet_loss =
      NormalizePacketLossFraction(scoring_point.uplink_packet_loss_fraction) -
      NormalizePacketLossFraction(uplink_packet_loss_fraction);
  return std::pow(diff_normalized_bitrate_bps, 2) +
         std::pow(diff_normalized_packet_loss, 2);
}

}  // namespace webrtc
